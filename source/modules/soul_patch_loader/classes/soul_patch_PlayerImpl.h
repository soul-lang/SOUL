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
        : fileList (std::move (f)), config (c), performer (std::move (p))
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

        if (linkOptions.getPlatform() == "bela")
        {
            soul::Compiler wrappedCompiler;
            addSource (messageList, preprocessor, wrappedCompiler);
            wrappedCompiler.addCode (messageList,  CodeLocation::createFromString ("BelaWrapper", soul::patch::BelaWrapper::build (program)));

            auto wrappedLinkOptions = linkOptions;
            wrappedLinkOptions.setMainProcessor ("BelaWrapper");
            program = wrappedCompiler.link (messageList, wrappedLinkOptions);
        }

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
        createParameters (program.getStringDictionary());
        connectEndpoints (program, consoleHandler);

        auto options = linkOptions;
        options.externalValueProvider = [this, externalDataProvider] (ConstantTable& constantTable,
                                                                      const char* name, const Type& type,
                                                                      const Annotation& annotation) -> ConstantTable::Handle
        {
            if (externalDataProvider != nullptr)
                if (auto file = externalDataProvider->getExternalFile (name))
                    return constantTable.getHandleForValue (AudioFileToValue::load (std::move (file), type, annotation, constantTable));

            return findExternalDefinitionInManifest (constantTable, name, type, annotation);
        };

        if (! performer->link (messageList, options, CacheConverter::create (cache).get()))
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

    ConstantTable::Handle findExternalDefinitionInManifest (ConstantTable& constantTable,
                                                            const char* name, const Type& type,
                                                            const Annotation& annotation) const
    {
        if (auto externals = fileList.getExternalsList())
        {
            for (auto& e : externals->getProperties())
            {
                if (e.name.toString().trim() == name)
                {
                    try
                    {
                        JSONtoValue jsonConverter (constantTable,
                                                   [this, &annotation, &constantTable] (const Type& targetType, const juce::String& stringValue) -> Value
                                                   {
                                                       if (auto file = fileList.checkAndCreateVirtualFile (stringValue))
                                                           return AudioFileToValue::load (std::move (file), targetType, annotation, constantTable);

                                                       return {};
                                                   });

                        return constantTable.getHandleForValue (jsonConverter.createValue (type, e.value));
                    }
                    catch (const PatchLoadError& error)
                    {
                        throwPatchLoadError ("Error resolving external " + quoteName (name) + ": " + error.message);
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

    void createParameters (const StringDictionary& stringDictionary)
    {
        auto inputEndpoints = performer->getInputEndpoints();
        parameters.reserve (inputEndpoints.size());

        for (auto& i : inputEndpoints)
        {
            if (isParameterInput (i))
                parameters.push_back (Parameter::Ptr (new ParameterImpl (stringDictionary, *performer->getInputSource (i.endpointID), i)));
        }

        parameterSpan = makeSpan (parameters);
    }

    void connectEndpoints (Program& program, ConsoleMessageHandler* consoleHandler)
    {
        wrapper = std::make_unique<SynchronousPerformerWrapper> (*performer);
        auto rateAndBlockSize = getSampleRateAndBlockSize();
        SOUL_ASSERT (rateAndBlockSize.isValid());
        wrapper->attach (rateAndBlockSize);

        if (consoleHandler != nullptr)
            for (auto& outputEndpoint : performer->getOutputEndpoints())
                if (isEvent (outputEndpoint.kind))
                    soul::utilities::attachConsoleOutputHandler (program,
                                                                 *performer->getOutputSink (outputEndpoint.endpointID),
                                                                 outputEndpoint, rateAndBlockSize,
                                                                 [consoleHandler] (uint64_t eventTime, const char* endpointName, const char* message)
                                                                 { consoleHandler->handleConsoleMessage (eventTime, endpointName, message); });
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

        if (rc.numInputChannels != wrapper->getExpectedNumInputChannels()
             || rc.numOutputChannels != wrapper->getExpectedNumOutputChannels())
            return RenderResult::wrongNumberOfChannels;

        DiscreteChannelSet<const float> input;
        input.channels = rc.inputChannels;
        input.numChannels = (uint32_t) rc.numInputChannels;
        input.offset = 0;
        input.numFrames = (uint32_t) rc.numFrames;

        DiscreteChannelSet<float> output;
        output.channels = rc.outputChannels;
        output.numChannels = (uint32_t) rc.numOutputChannels;
        output.offset = 0;
        output.numFrames = (uint32_t) rc.numFrames;

        auto midi = rc.incomingMIDI;
        auto midiEnd = midi != nullptr ? midi + rc.numMIDIMessagesIn : nullptr;

        wrapper->render (input, output,
                         midi, midiEnd,
                         rc.outgoingMIDI, rc.maximumMIDIMessagesOut, rc.numMIDIMessagesOut);

        return RenderResult::ok;
    }

    //==============================================================================
    struct ParameterImpl  : public RefCountHelper<Parameter>
    {
        ParameterImpl (const StringDictionary& d, InputSource& input,
                       const EndpointDetails& details)
            : stringDictionary (d)
        {
            annotation = details.annotation;
            ID = makeString (details.name);
            name = makeString (stringDictionary.getStringForHandle (details.annotation.getStringLiteral ("name")));

            if (name == nullptr || name.toString<juce::String>().trim().isEmpty())
                name = ID;

            minValue = 0;
            maxValue = 1.0f;
            step = 0;
            int numIntervals = 0;

            auto textValue = details.annotation.getValue ("text");

            if (textValue.getType().isStringLiteral())
            {
                auto items = juce::StringArray::fromTokens (juce::String (textValue.getDescription()).unquoted(), "|", {});

                if (items.size() > 1)
                {
                    numIntervals = items.size() - 1;
                    maxValue = float (numIntervals);
                }
            }

            unit         = makeString (stringDictionary.getStringForHandle (details.annotation.getStringLiteral ("unit")));
            minValue     = castValueToFloat (details.annotation.getValue ("min"), minValue);
            maxValue     = castValueToFloat (details.annotation.getValue ("max"), maxValue);
            step         = castValueToFloat (details.annotation.getValue ("step"), maxValue / (numIntervals == 0 ? 1000 : numIntervals));
            initialValue = castValueToFloat (details.annotation.getValue ("init"), minValue);
            rampFrames   = checkRampLength (details.annotation.getValue ("rampFrames"));

            value = initialValue;

            if (isEvent (details.kind))
            {
                input.setEventSource ([this] (size_t, uint32_t, callbacks::PostNextEvent postEvent)
                {
                    if (changed)
                    {
                        changed = false;
                        postEvent (Value (value));
                    }

                    return 1024;
                });
            }
            else if (isStream (details.kind))
            {
                input.setSparseStreamSource ([this] (uint64_t /*totalFramesElapsed*/,
                                                     callbacks::SetSparseStreamTarget setTargetValue) -> uint32_t
                {
                    if (changed)
                    {
                        changed = false;
                        setTargetValue (Value (value), rampFrames, 0.0f);
                    }

                    return 1024;
                });
            }
            else if (isValue (details.kind))
            {
                input.setCurrentValue (Value (initialValue));

                onValueUpdated = [this, &input]
                {
                    input.setCurrentValue (Value (value));
                };
            }

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

                if (onValueUpdated)
                    onValueUpdated();
            }
        }

        String::Ptr getProperty (const char* propertyName) const override
        {
            auto v = annotation.getValue (propertyName);

            if (v.isValid())
                return makeString (v.getDescription (std::addressof (stringDictionary)));

            return {};
        }

        Span<const char*> getPropertyNames() const override   { return propertyNameSpan; }

        float snapToLegalValue (float v) const
        {
            if (step > 0)
                v = minValue + step * std::floor ((v - minValue) / step + 0.5f);

            return v < minValue ? minValue : (v > maxValue ? maxValue : v);
        }

        uint32_t rampFrames;
        float value = 0;
        std::atomic<bool> changed { true };
        Annotation annotation;
        std::vector<std::string> propertyNameStrings;
        std::vector<const char*> propertyNameRawStrings;
        Span<const char*> propertyNameSpan;
        const StringDictionary& stringDictionary;

        std::function<void()> onValueUpdated;
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

    SampleRateAndBlockSize getSampleRateAndBlockSize() const
    {
        if (config.sampleRate <= 0)         throwPatchLoadError ("Illegal sample rate");
        if (config.maxFramesPerBlock <= 0)  throwPatchLoadError ("Illegal block size");

        return { config.sampleRate, (uint32_t) config.maxFramesPerBlock };
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
    std::unique_ptr<SynchronousPerformerWrapper> wrapper;

    static constexpr int64_t maxRampLength = 0x7fffffff;
};

}
