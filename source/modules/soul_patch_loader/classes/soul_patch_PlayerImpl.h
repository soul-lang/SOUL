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
struct PatchPlayerImpl final  : public RefCountHelper<PatchPlayer, PatchPlayerImpl>
{
    PatchPlayerImpl (FileList f, PatchPlayerConfiguration c, std::unique_ptr<soul::Performer> p)
        : fileList (std::move (f)), config (c), performer (std::move (p)), wrapper (*performer)
    {
    }

    ~PatchPlayerImpl()
    {
        if (performer != nullptr)
        {
            performer->unload();
            performer.release();
        }
    }

    Span<CompilationMessage> getCompileMessages() const override    { return compileMessagesSpan; }
    bool isPlayable() const override                                { return ! anyErrors; }
    Description* getDescription() const override                    { return fileList.createDescription(); }

    bool needsRebuilding (const PatchPlayerConfiguration& newConfig) override
    {
        return config != newConfig || fileList.hasChanged();
    }

    //==============================================================================
    Span<Bus> getInputBuses() const override                            { return inputBusesSpan; }
    Span<Bus> getOutputBuses() const override                           { return outputBusesSpan; }
    Span<Parameter::Ptr> getParameters() const override                 { return parameterSpan; }
    Span<EndpointDescription> getInputEventEndpoints() const override   { return inputEventEndpointSpan; }
    Span<EndpointDescription> getOutputEventEndpoints() const override  { return outputEventEndpointSpan; }
    uint32_t getLatencySamples() const override                         { return latency; }

    EndpointDescription getEndpointDetails (const char* endpointID) const override
    {
        for (auto& e : endpointHolders)
            if (e.desc.ID.toString<std::string>() == endpointID)
                return e.desc;

        for (auto& e : endpointHolders)
            if (e.desc.name.toString<std::string>() == endpointID)
                return e.desc;

        EndpointDescription desc;
        desc.handle = std::numeric_limits<decltype (desc.handle)>::max();
        desc.type = soul::EndpointType::unknown;
        desc.annotation = {};
        desc.valueTypes = {};
        desc.numValueTypes = 0;
        return desc;
    }

    //==============================================================================
    void addSource (BuildBundle& build, SourceFilePreprocessor* preprocessor)
    {
        for (auto& fileState : fileList.sourceFiles)
        {
            VirtualFile::Ptr source;

            if (preprocessor != nullptr)
                source = VirtualFile::Ptr (preprocessor->preprocessSourceFile (*fileState.file));

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
                  ExternalDataProvider* externalDataProvider)
    {
        if (performer == nullptr)
            return messageList.addError ("Failed to initialise JIT engine", {});

        auto program = compileSources (messageList, settings, preprocessor);

        if (program.isEmpty())
        {
            if (! messageList.hasErrors())
                messageList.addError ("Empty program", {});

            return;
        }

        if (! performer->load (messageList, program))
            return messageList.addError ("Failed to load program", {});

        createBusesAndEventEndpoints();
        createRenderOperations();
        resolveExternalVariables (externalDataProvider);

        if (! performer->link (messageList, settings, CacheConverter::create (cache).get()))
            if (! messageList.hasErrors())
                messageList.addError ("Failed to link", {});

        latency = performer->getLatency();
    }

    void compile (const BuildSettings& settings,
                  CompilerCache* cache,
                  SourceFilePreprocessor* preprocessor,
                  ExternalDataProvider* externalDataProvider)
    {
        soul::CompileMessageList messageList;
        compile (messageList, settings, cache, preprocessor, externalDataProvider);

        compileMessages.reserve (messageList.messages.size());

        for (auto& m : messageList.messages)
        {
            CompilationMessage cm;
            auto sourceLine = m.getAnnotatedSourceLine();

            if (sourceLine.empty())
                cm.fullMessage = makeString (m.getFullDescription());
            else
                cm.fullMessage = makeString (m.getFullDescription() + "\n" + sourceLine);

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

            value.visitObjectMembers ([&] (std::string_view memberName, const choc::value::ValueView& memberValue)
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
                return AudioFileToValue::load (VirtualFile::Ptr (file), ev.annotation);

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

    void createBusesAndEventEndpoints()
    {
        inputBuses.clear();
        inputEventEndpoints.clear();
        outputEventEndpoints.clear();
        endpointHolders.clear();
        outputBuses.clear();

        for (auto& audioInput : wrapper.getAudioInputEndpoints())
            inputBuses.push_back ({ makeString (audioInput.name), getNumAudioChannels (audioInput) });

        for (auto& audioOutput : wrapper.getAudioOutputEndpoints())
            outputBuses.push_back ({ makeString (audioOutput.name), getNumAudioChannels (audioOutput) });

        for (auto& e : performer->getInputEndpoints())
            getEndpointDescription (e);

        for (auto& e : performer->getOutputEndpoints())
            getEndpointDescription (e);

        for (auto& eventInput : wrapper.getEventInputEndpoints())
            inputEventEndpoints.push_back (getEndpointDescription (eventInput));

        for (auto& eventOutput : wrapper.getEventOutputEndpoints())
            outputEventEndpoints.push_back (getEndpointDescription (eventOutput));

        inputBusesSpan  = makeSpan (inputBuses);
        outputBusesSpan = makeSpan (outputBuses);
        inputEventEndpointSpan  = makeSpan (inputEventEndpoints);
        outputEventEndpointSpan = makeSpan (outputEventEndpoints);
    }

    struct EndpointDescriptionHolder
    {
        EndpointDescriptionHolder (const EndpointDetails& e, EndpointHandle patchHandle, soul::EndpointHandle performerHandle)
            : handle (performerHandle)
        {
            desc.handle = patchHandle;
            desc.ID = makeString (e.endpointID.toString());
            desc.name = makeString (e.name);
            desc.type = e.endpointType;

            annotationData = serialise (e.annotation.toExternalValue());
            desc.annotation = { annotationData.data(), static_cast<uint32_t> (annotationData.size()) };

            for (auto& t : e.dataTypes)
                typeData.push_back (serialise (t));

            for (auto& t : typeData)
                types.push_back ({ t.data(), static_cast<uint32_t> (t.size()) });

            desc.valueTypes = types.data();
            desc.numValueTypes = static_cast<uint32_t> (types.size());
        }

        template <typename ValueOrType>
        static std::vector<char> serialise (const ValueOrType& v)
        {
            struct Writer
            {
                std::vector<char> s;
                void write (const void* data, size_t size)  { s.insert (s.end(), static_cast<const char*> (data), static_cast<const char*> (data) + size); }
            };

            Writer w;
            v.serialise (w);
            return w.s;
        }

        soul::EndpointHandle handle;
        EndpointDescription desc;
        std::vector<SerialisedType> types;
        std::vector<std::vector<char>> typeData;
        std::vector<char> annotationData;
    };

    EndpointDescription getEndpointDescription (const EndpointDetails& e)
    {
        auto handle = performer->getEndpointHandle (e.endpointID);

        for (auto& desc : endpointHolders)
            if (desc.handle == handle)
                return desc.desc;

        auto index = endpointHolders.size();
        endpointHolders.push_back (EndpointDescriptionHolder (e, static_cast<EndpointHandle> (index), handle));
        return endpointHolders.back().desc;
    }

    void createRenderOperations()
    {
        parameters.clear();
        checkSampleRateAndBlockSize();

        wrapper.prepare ((uint32_t) config.maxFramesPerBlock,
                         [] (const EndpointDetails& endpoint) -> uint32_t
                         {
                             return checkRampLength (endpoint.annotation.getValue ("rampFrames"));
                         });

        auto paramEndpoints = wrapper.getParameterEndpoints();
        parameters.reserve (paramEndpoints.size());
        uint32_t index = 0;

        for (auto& p : paramEndpoints)
            parameters.push_back (Parameter::Ptr (new ParameterImpl (p, wrapper.parameterList, index++)));

        parameterSpan = makeSpan (parameters);
    }

    //==============================================================================
    void reset() override
    {
        performer->reset();

        for (auto& p : parameters)
            static_cast<ParameterImpl&>(*p).markAsDirty();
    }

    bool sendInputEvent (EndpointHandle handle, const choc::value::ValueView& event) override
    {
        if (handle < endpointHolders.size())
            return wrapper.postInputEvent (endpointHolders[handle].handle, event);

        return false;
    }

    RenderResult render (RenderContext& rc) override
    {
        if (anyErrors)
            return RenderResult::noProgramLoaded;

        if (rc.numInputChannels != wrapper.getExpectedNumInputChannels()
             || rc.numOutputChannels != wrapper.getExpectedNumOutputChannels())
            return RenderResult::wrongNumberOfChannels;

        auto midiInStart = reinterpret_cast<const MIDIEvent*> (rc.incomingMIDI);
        auto midiOutStart = reinterpret_cast<MIDIEvent*> (rc.outgoingMIDI);
        auto midiOut = MIDIEventOutputList { midiOutStart, rc.maximumMIDIMessagesOut };

        wrapper.render (choc::buffer::createChannelArrayView (rc.inputChannels, rc.numInputChannels, rc.numFrames),
                        choc::buffer::createChannelArrayView (rc.outputChannels, rc.numOutputChannels, rc.numFrames),
                        { midiInStart, midiInStart + rc.numMIDIMessagesIn },
                        midiOut);

        rc.numMIDIMessagesOut = static_cast<uint32_t> (midiOut.start - midiOutStart);
        return RenderResult::ok;
    }

    void handleOutgoingEvents (void* userContext,
                               HandleOutgoingEventFn* handleEvent,
                               HandleConsoleMessageFn* handleConsoleMessage) override
    {
        wrapper.deliverOutgoingEvents ([=] (uint64_t frameIndex, const std::string& endpointName, const choc::value::ValueView& eventData)
        {
            if (isConsoleEndpoint (endpointName))
            {
                if (handleConsoleMessage != nullptr)
                    handleConsoleMessage (userContext, frameIndex, dump (eventData).c_str());
            }
            else if (handleEvent != nullptr)
            {
                handleEvent (userContext, frameIndex, endpointName.c_str(), eventData);
            }
        });
    }

    void applyNewTimeSignature (TimeSignature newTimeSig) override
    {
        wrapper.timelineEventEndpointList.applyNewTimeSignature (newTimeSig);
    }

    void applyNewTempo (float newBPM) override
    {
        wrapper.timelineEventEndpointList.applyNewTempo (newBPM);
    }

    void applyNewTransportState (TransportState newState) override
    {
        wrapper.timelineEventEndpointList.applyNewTransportState (newState);
    }

    void applyNewTimelinePosition (TimelinePosition newPosition) override
    {
        wrapper.timelineEventEndpointList.applyNewTimelinePosition (newPosition);
    }

    //==============================================================================
    struct ParameterImpl final  : public RefCountHelper<Parameter, ParameterImpl>
    {
        ParameterImpl (const EndpointDetails& details, ParameterStateList& list, uint32_t index)
            : paramList (list), paramIndex (index), annotation (details.annotation)
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
            paramList.setParameter (paramIndex, value);
            markAsDirty();
        }

        float getValue() const override
        {
            return value;
        }

        void setValue (float newValue) override
        {
            value = snapToLegalValue (newValue);
            paramList.setParameter (paramIndex, value);
        }

        void markAsDirty()
        {
            paramList.markAsChanged (paramIndex);
        }

        String* getProperty (const char* propertyName) const override
        {
            if (annotation.hasValue (propertyName))
                return makeStringPtr (annotation.getString (propertyName));

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
        ParameterStateList& paramList;
        const uint32_t paramIndex = 0;
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
    std::vector<EndpointDescription> inputEventEndpoints, outputEventEndpoints;
    std::vector<EndpointDescriptionHolder> endpointHolders;
    std::vector<Parameter::Ptr> parameters;

    Span<Bus> inputBusesSpan = {}, outputBusesSpan = {};
    Span<Parameter::Ptr> parameterSpan = {};
    Span<EndpointDescription> inputEventEndpointSpan, outputEventEndpointSpan;
    uint32_t latency = 0;

    PatchPlayerConfiguration config;
    std::unique_ptr<soul::Performer> performer;
    AudioMIDIWrapper wrapper;

    static constexpr int64_t maxRampLength = 0x7fffffff;
};

}
