/*
    _____ _____ _____ __
   |   __|     |  |  |  |      The SOUL language
   |__   |  |  |  |  |  |__    Copyright (c) 2019 - ROLI Ltd.
   |_____|_____|_____|_____|

   The code in this file is provided under the terms of the ISC license:

   Permission to use, copy, modify, and/or distribute this software for any purpose
   with or without fee is hereby granted, provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD
   TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN
   NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
   DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
   IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

namespace soul::patch
{

/**
    Implementation of the PatchPlayer interface.
*/
struct PatchPlayerImpl  : public RefCountHelper<PatchPlayer>
{
    PatchPlayerImpl (FileList f, PatchPlayerConfiguration c, std::unique_ptr<soul::Performer> p)
        : fileList (std::move (f)), config (c), performer (std::move (p)), wrapper (*performer)
    {
    }

    ~PatchPlayerImpl() override
    {
        if (performer != nullptr)
        {
            performer->unload();
            performer.release();
        }
    }

    Span<CompilationMessage> getCompileMessages() const override    { return compileMessagesSpan; }
    bool isPlayable() const override                                { return ! anyErrors; }
    Description getDescription() const override                     { return fileList.createDescription(); }

    bool needsRebuilding (const PatchPlayerConfiguration& newConfig) override
    {
        return config != newConfig || fileList.hasChanged();
    }

    //==============================================================================
    Span<Bus> getInputBuses() const override                    { return inputBusesSpan; }
    Span<Bus> getOutputBuses() const override                   { return outputBusesSpan; }
    Span<Parameter::Ptr> getParameters() const override         { return parameterSpan; }

    //==============================================================================
    void addSource (soul::CompileMessageList& messageList,
                    SourceFilePreprocessor* preprocessor,
                    soul::Compiler& compiler)
    {
        for (auto& fileState : fileList.sourceFiles)
        {
            VirtualFile::Ptr source;

            if (preprocessor != nullptr)
                source = preprocessor->preprocessSourceFile (*fileState.file);

            if (source == nullptr)
                source = fileState.file;

            juce::String readError;
            auto content = loadVirtualFileAsString (*source, readError);

            if (readError.isNotEmpty())
                throwPatchLoadError (readError.toStdString());

            compiler.addCode (messageList, CodeLocation::createFromString (fileState.path.toStdString(),
                                                                           content.toStdString()));
        }
    }

    soul::Program compileSources (soul::CompileMessageList& messageList,
                                  soul::LinkOptions linkOptions,
                                  SourceFilePreprocessor* preprocessor)
    {
        soul::Compiler compiler;

        addSource (messageList, preprocessor, compiler);

        auto program = compiler.link (messageList, linkOptions);

       #if JUCE_BELA
        {
            soul::Compiler wrappedCompiler;
            addSource (messageList, preprocessor, wrappedCompiler);
            wrappedCompiler.addCode (messageList, CodeLocation::createFromString ("BelaWrapper", soul::patch::BelaWrapper::build (program)));

            auto wrappedLinkOptions = linkOptions;
            wrappedLinkOptions.setMainProcessor ("BelaWrapper");
            program = wrappedCompiler.link (messageList, wrappedLinkOptions);
        }
       #endif

        return program;
    }

    void compile (soul::CompileMessageList& messageList,
                  const soul::LinkOptions& linkOptions,
                  CompilerCache* cache,
                  SourceFilePreprocessor* preprocessor,
                  ExternalDataProvider* externalDataProvider,
                  ConsoleMessageHandler* consoleHandler)
    {
        if (performer == nullptr)
            return messageList.addError ("Failed to initialise JIT engine", {});

        messageList.onMessage = [] (const soul::CompileMessage& message)
        {
            if (message.isError())
                throwPatchLoadError (message.getFullDescription() + "\n" + message.getAnnotatedSourceLine());
        };

        auto program = compileSources (messageList, linkOptions, preprocessor);

        if (program.isEmpty())
            return messageList.addError ("Empty program", {});

        if (! performer->load (messageList, program))
            return messageList.addError ("Failed to load program", {});

        createBuses();
        createRenderOperations (program, consoleHandler);
        resolveExternalVariables (externalDataProvider);

        if (! performer->link (messageList, linkOptions, CacheConverter::create (cache).get()))
            return messageList.addError ("Failed to link", {});
    }

    void compile (const soul::LinkOptions& linkOptions,
                  CompilerCache* cache,
                  SourceFilePreprocessor* preprocessor,
                  ExternalDataProvider* externalDataProvider,
                  ConsoleMessageHandler* consoleHandler)
    {
        soul::CompileMessageList messageList;
        compile (messageList, linkOptions, cache, preprocessor, externalDataProvider, consoleHandler);

        compileMessages.reserve (messageList.messages.size());

        for (auto& m : messageList.messages)
        {
            CompilationMessage cm;
            cm.fullMessage = makeString (m.getFullDescription());
            cm.filename = makeString (m.location.getFilename());
            cm.description = makeString (m.description);
            auto lc = m.location.getLineAndColumn();
            cm.line = lc.line;
            cm.column = lc.column;
            cm.isError = m.isError();

            compileMessages.push_back (cm);
        }

        updateCompileMessageStatus();
    }

    void updateCompileMessageStatus()
    {
        compileMessagesSpan = makeSpan (compileMessages);
        anyErrors = false;

        for (auto& m : compileMessages)
            anyErrors = anyErrors || m.isError;
    }

    void resolveExternalVariables (ExternalDataProvider* externalDataProvider)
    {
        for (auto& ev : performer->getExternalVariables())
        {
            auto value = resolveExternalVariable (externalDataProvider, ev);

            if (value.isValid())
                performer->setExternalVariable (ev.name.c_str(), std::move (value));
        }
    }

    soul::Value resolveExternalVariable (ExternalDataProvider* externalDataProvider, const soul::Performer::ExternalVariable& ev)
    {
        auto convertFixedToUnsizedArray = [this] (soul::Value v)
        {
            auto elementType = v.getType().getElementType();
            return Value::createUnsizedArray (elementType, performer->addConstant (std::move (v)));
        };

        if (externalDataProvider != nullptr)
            if (auto file = externalDataProvider->getExternalFile (ev.name.c_str()))
                return AudioFileToValue::load (std::move (file), ev.type, ev.annotation, convertFixedToUnsizedArray);

        if (auto externals = fileList.getExternalsList())
        {
            for (auto& e : externals->getProperties())
            {
                if (e.name.toString().trim() == ev.name.c_str())
                {
                    try
                    {
                        return convertJSONToValue (e.value, ev.type,
                                                   [this, &ev, &convertFixedToUnsizedArray] (const Type& targetType, const juce::String& stringValue) -> Value
                                                   {
                                                       if (auto file = fileList.checkAndCreateVirtualFile (stringValue.toStdString()))
                                                           return AudioFileToValue::load (std::move (file), targetType, ev.annotation, convertFixedToUnsizedArray);

                                                       return {};
                                                   },
                                                   convertFixedToUnsizedArray);
                    }
                    catch (const PatchLoadError& error)
                    {
                        throwPatchLoadError ("Error resolving external " + quoteName (ev.name) + ": " + error.message);
                    }

                    return {};
                }
            }
        }

        return {};
    }

    void createBuses()
    {
        for (auto& i : performer->getInputEndpoints())
            if (auto numChans = i.getNumAudioChannels())
                if (! isParameterInput (i))
                    inputBuses.push_back ({ makeString (i.name), (uint32_t) numChans });

        for (auto& o : performer->getOutputEndpoints())
            if (auto numChans = o.getNumAudioChannels())
                outputBuses.push_back ({ makeString (o.name), (uint32_t) numChans });

        inputBusesSpan  = makeSpan (inputBuses);
        outputBusesSpan = makeSpan (outputBuses);
    }

    template <typename DebugHandler>
    static void printConsoleMessage (const Program& program, const std::string& endpointName, uint64_t eventTime, double sampleRate,
                                     const soul::Value& eventData, DebugHandler&& handler)
    {
        if (isConsoleEndpoint (endpointName))
            handler (eventTime, endpointName.c_str(), program.getValueDump (eventData, false).c_str());
        else
            handler (eventTime, endpointName.c_str(), (endpointName + "  " + toStringWithDecPlaces (eventTime / sampleRate, 3)
                                                         + ": " + program.getValueDump (eventData) + "\n").c_str());
    }

    void createRenderOperations (Program& program, ConsoleMessageHandler* consoleHandler)
    {
        parameters.clear();
        checkSampleRateAndBlockSize();

        decltype (wrapper)::HandleUnusedEventFn handleUnusedEvents;
        auto sampleRate = config.sampleRate;

        if (consoleHandler != nullptr)
        {
            handleUnusedEvents = [consoleHandler, program, sampleRate] (uint64_t eventTime, const std::string& endpointName, const Value& eventData) -> bool
                                 {
                                     printConsoleMessage (program, endpointName, eventTime, sampleRate, eventData,
                                                          [consoleHandler] (uint64_t time, const char* name, const char* message)
                                                          { consoleHandler->handleConsoleMessage (time, name, message); });
                                     return true;
                                 };
        }

        wrapper.buildRenderingPipeline ((uint32_t) config.maxFramesPerBlock,
                                        [&] (const EndpointDetails& endpoint) -> std::function<const float*()>
                                        {
                                            auto param = new ParameterImpl (endpoint);
                                            parameters.push_back (Parameter::Ptr (param));
                                            return [param] { return param->getValueIfChanged(); };
                                        },
                                        [] (const EndpointDetails& endpoint) -> uint32_t
                                        {
                                            return checkRampLength (endpoint.annotation.getValue ("rampFrames"));
                                        },
                                        std::move (handleUnusedEvents));

        parameterSpan = makeSpan (parameters);
    }

    //==============================================================================
    void reset() override
    {
        performer->reset();

        for (auto& p : parameters)
            static_cast<ParameterImpl&>(*p).changed = true;
    }

    RenderResult render (RenderContext& rc) override
    {
        if (anyErrors)
            return RenderResult::noProgramLoaded;

        if (rc.numInputChannels != wrapper.getExpectedNumInputChannels()
             || rc.numOutputChannels != wrapper.getExpectedNumOutputChannels())
            return RenderResult::wrongNumberOfChannels;

        wrapper.render ({ rc.inputChannels,  (uint32_t) rc.numInputChannels,  0, (uint32_t) rc.numFrames },
                        { rc.outputChannels, (uint32_t) rc.numOutputChannels, 0, (uint32_t) rc.numFrames },
                        rc.incomingMIDI,
                        rc.outgoingMIDI,
                        rc.numMIDIMessagesIn,
                        rc.maximumMIDIMessagesOut,
                        rc.numMIDIMessagesOut);

        return RenderResult::ok;
    }

    //==============================================================================
    struct ParameterImpl  : public RefCountHelper<Parameter>
    {
        ParameterImpl (const EndpointDetails& details)  : annotation (details.annotation)
        {
            ID = makeString (details.name);

            PatchPropertiesFromEndpointDetails props (details);
            name         = makeString (props.name);
            unit         = makeString (props.unit);
            minValue     = props.minValue;
            maxValue     = props.maxValue;
            step         = props.step;
            initialValue = props.initialValue;

            value = initialValue;

            propertyNameStrings = annotation.getNames();
            propertyNameRawStrings.reserve (propertyNameStrings.size());

            for (auto& p : propertyNameStrings)
                propertyNameRawStrings.push_back (p.c_str());

            propertyNameSpan = makeSpan (propertyNameRawStrings);
        }

        float getValue() const override
        {
            return value;
        }

        void setValue (float newValue) override
        {
            newValue = snapToLegalValue (newValue);

            if (value != newValue)
            {
                value = newValue;
                changed = true;
            }
        }

        const float* getValueIfChanged()
        {
            if (! changed)
                return nullptr;

            changed = false;
            return std::addressof (value);
        }

        String::Ptr getProperty (const char* propertyName) const override
        {
            if (annotation.hasValue (propertyName))
                return makeString (annotation.getString (propertyName));

            return {};
        }

        Span<const char*> getPropertyNames() const override   { return propertyNameSpan; }

        float snapToLegalValue (float v) const
        {
            if (step > 0)
                v = minValue + step * std::floor ((v - minValue) / step + 0.5f);

            return v < minValue ? minValue : (v > maxValue ? maxValue : v);
        }

        float value = 0;
        std::atomic<bool> changed { true };
        Annotation annotation;
        std::vector<std::string> propertyNameStrings;
        std::vector<const char*> propertyNameRawStrings;
        Span<const char*> propertyNameSpan;
    };

    //==============================================================================
    static float castValueToFloat (const soul::Value& v, float defaultValue)
    {
        if (v.getType().isPrimitive() && (v.getType().isFloatingPoint() || v.getType().isInteger()))
            return static_cast<float> (v.getAsDouble());

        return defaultValue;
    }

    static uint32_t checkRampLength (const soul::Value& v)
    {
        if (v.getType().isPrimitive() && (v.getType().isFloatingPoint() || v.getType().isInteger()))
        {
            auto frames = v.getAsInt64();

            if (frames < 0)
                return 0;

            if (frames > maxRampLength)
                return (uint32_t) maxRampLength;

            return (uint32_t) frames;
        }

        return 1000;
    }

    void checkSampleRateAndBlockSize() const
    {
        if (config.sampleRate <= 0)         throwPatchLoadError ("Illegal sample rate");
        if (config.maxFramesPerBlock <= 0)  throwPatchLoadError ("Illegal block size");
    }

    std::vector<CompilationMessage> compileMessages;
    Span<CompilationMessage> compileMessagesSpan;
    bool anyErrors = false;

    FileList fileList;

    std::vector<Bus> inputBuses, outputBuses;
    std::vector<Parameter::Ptr> parameters;

    Span<Bus> inputBusesSpan = {}, outputBusesSpan = {};
    Span<Parameter::Ptr> parameterSpan = {};

    PatchPlayerConfiguration config;
    std::unique_ptr<soul::Performer> performer;
    AudioMIDIWrapper<MIDIMessage> wrapper;

    static constexpr int64_t maxRampLength = 0x7fffffff;
};

}
