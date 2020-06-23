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
    void addSource (BuildBundle& build, SourceFilePreprocessor* preprocessor)
    {
        for (auto& fileState : fileList.sourceFiles)
        {
            VirtualFile::Ptr source;

            if (preprocessor != nullptr)
                source = preprocessor->preprocessSourceFile (*fileState.file);

            if (source == nullptr)
                source = fileState.file;

            std::string readError;
            auto content = loadVirtualFileAsString (*source, readError);

            if (! readError.empty())
                throwPatchLoadError (readError);

            build.sourceFiles.push_back ({ fileState.path, std::move (content) });
        }
    }

    soul::Program compileSources (soul::CompileMessageList& messageList,
                                  const BuildSettings& settings,
                                  SourceFilePreprocessor* preprocessor)
    {
        BuildBundle build;
        addSource (build, preprocessor);
        build.settings = settings;
        auto program = Compiler::build (messageList, build);

       #if JUCE_BELA
        {
            auto wrappedBuild = build;
            wrappedBuild.sourceFiles.push_back ({ "BelaWrapper", soul::patch::BelaWrapper::build (program) });
            wrappedBuild.settings.mainProcessor = "BelaWrapper";
            program = Compiler::build (messageList, wrappedBuild);
        }
       #endif

        return program;
    }

    void compile (soul::CompileMessageList& messageList,
                  const BuildSettings& settings,
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

        auto program = compileSources (messageList, settings, preprocessor);

        if (program.isEmpty())
            return messageList.addError ("Empty program", {});

        if (! performer->load (messageList, program))
            return messageList.addError ("Failed to load program", {});

        createBuses();
        createRenderOperations (consoleHandler);
        resolveExternalVariables (externalDataProvider);

        if (! performer->link (messageList, settings, CacheConverter::create (cache).get()))
            return messageList.addError ("Failed to link", {});
    }

    void compile (const BuildSettings& settings,
                  CompilerCache* cache,
                  SourceFilePreprocessor* preprocessor,
                  ExternalDataProvider* externalDataProvider,
                  ConsoleMessageHandler* consoleHandler)
    {
        soul::CompileMessageList messageList;
        compile (messageList, settings, cache, preprocessor, externalDataProvider, consoleHandler);

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

            if (! value.isVoid())
                performer->setExternalVariable (ev.name.c_str(), value);
        }
    }

    inline choc::value::Value replaceStringsWithFileContent (const choc::value::ValueView& value,
                                                             const std::function<choc::value::Value(std::string_view)>& convertStringToValue)
    {
        if (value.isString())
            return convertStringToValue (value.getString());

        if (value.isArray())
        {
            auto v = choc::value::createEmptyArray();

            for (auto i : value)
                v.addArrayElement (replaceStringsWithFileContent (i, convertStringToValue));

            return v;
        }

        if (value.isObject())
        {
            auto v = choc::value::createObject (value.getObjectClassName());

            value.visitObjectMembers ([&] (const std::string& memberName, const choc::value::ValueView& memberValue)
            {
                v.addMember (memberName, replaceStringsWithFileContent (memberValue, convertStringToValue));
            });

            return v;
        }

        return choc::value::Value (value);
    }

    choc::value::Value resolveExternalVariable (ExternalDataProvider* externalDataProvider, const ExternalVariable& ev)
    {
        if (externalDataProvider != nullptr)
            if (auto file = externalDataProvider->getExternalFile (ev.name.c_str()))
                return AudioFileToValue::load (std::move (file), ev.annotation);

        auto externals = fileList.getExternalsList();

        if (externals.isObject() && externals.hasObjectMember (ev.name))
        {
            try
            {
                return replaceStringsWithFileContent (externals[ev.name],
                                                      [&] (std::string_view s) -> choc::value::Value
                                                      {
                                                          if (auto file = fileList.checkAndCreateVirtualFile (std::string (s)))
                                                              return AudioFileToValue::load (std::move (file), ev.annotation);

                                                          return choc::value::createString (s);
                                                      });
            }
            catch (const PatchLoadError& error)
            {
                throwPatchLoadError ("Error resolving external " + quoteName (ev.name) + ": " + error.message);
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
    static void printConsoleMessage (const std::string& endpointName, uint64_t eventTime, double sampleRate,
                                     const choc::value::ValueView& eventData, DebugHandler&& handler)
    {
        if (isConsoleEndpoint (endpointName))
            handler (eventTime, endpointName.c_str(), dump (eventData).c_str());
        else
            handler (eventTime, endpointName.c_str(), (endpointName + "  " + toStringWithDecPlaces (eventTime / sampleRate, 3)
                                                         + ": " + dump (eventData) + "\n").c_str());
    }

    void createRenderOperations (ConsoleMessageHandler* consoleHandler)
    {
        parameters.clear();
        checkSampleRateAndBlockSize();

        decltype (wrapper)::HandleUnusedEventFn handleUnusedEvents;
        auto sampleRate = config.sampleRate;

        if (consoleHandler != nullptr)
        {
            handleUnusedEvents = [consoleHandler, sampleRate] (uint64_t eventTime, const std::string& endpointName, const choc::value::ValueView& eventData) -> bool
                                 {
                                     printConsoleMessage (endpointName, eventTime, sampleRate, eventData,
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

        // we're reinterpret-casting between these types to avoid having to include choc::midi::ShortMessage in
        // the public patch API headers, so this just checks that the layout is actually the same.
        static_assert (sizeof (MIDIEvent) == sizeof (soul::patch::MIDIMessage));

        wrapper.render (choc::buffer::createChannelArrayView (rc.inputChannels, rc.numInputChannels, rc.numFrames),
                        choc::buffer::createChannelArrayView (rc.outputChannels, rc.numOutputChannels, rc.numFrames),
                        reinterpret_cast<const MIDIEvent*> (rc.incomingMIDI),
                        reinterpret_cast<MIDIEvent*> (rc.outgoingMIDI),
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

            PatchParameterProperties props (details.name, details.annotation.toExternalValue());
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
    AudioMIDIWrapper wrapper;

    static constexpr int64_t maxRampLength = 0x7fffffff;
};

}
