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

namespace soul::audioplayer
{

//==============================================================================
class AudioPlayerVenue   : public soul::Venue,
                           private juce::AudioIODeviceCallback,
                           private juce::MidiInputCallback,
                           private juce::Timer
{
public:
    AudioPlayerVenue (Requirements r, std::unique_ptr<PerformerFactory> factory)
        : requirements (std::move (r)),
          performerFactory (std::move (factory))
    {
        if (requirements.sampleRate < 1000.0 || requirements.sampleRate > 48000.0 * 8)
            requirements.sampleRate = 0;

        if (requirements.blockSize < 1 || requirements.blockSize > 2048)
            requirements.blockSize = 0;

       #if ! JUCE_BELA
        // With BELA, the midi is handled within the audio thread, so we don't have any timestamp offsets or inter-thread coordination to do
        midiCollector = std::make_unique<juce::MidiMessageCollector>();
        midiCollector->reset (44100);
       #endif

        openAudioDevice();
        startTimerHz (3);
    }

    ~AudioPlayerVenue() override
    {
        SOUL_ASSERT (activeSessions.empty());
        performerFactory.reset();
        audioDevice.reset();
        midiInputs.clear();
        midiCollector.reset();
    }

    std::unique_ptr<Venue::Session> createSession() override
    {
        return std::make_unique<AudioPlayerSession> (*this);
    }

    std::vector<EndpointDetails> getSourceEndpoints() override    { return convertEndpointList (sourceEndpoints); }
    std::vector<EndpointDetails> getSinkEndpoints() override      { return convertEndpointList (sinkEndpoints); }

    bool connectSessionInputEndpoint (Session& session, EndpointID inputID, EndpointID venueSourceID) override
    {
        if (audioDevice != nullptr)
            if (auto audioSession = dynamic_cast<AudioPlayerSession*> (&session))
                if (auto venueEndpoint = findEndpoint (sourceEndpoints, venueSourceID))
                    return audioSession->connectInputEndpoint (*venueEndpoint, inputID);

        return false;
    }

    bool connectSessionOutputEndpoint (Session& session, EndpointID outputID, EndpointID venueSinkID) override
    {
        if (audioDevice != nullptr)
            if (auto audioSession = dynamic_cast<AudioPlayerSession*> (&session))
                if (auto venueEndpoint = findEndpoint (sinkEndpoints, venueSinkID))
                    return audioSession->connectOutputEndpoint (*venueEndpoint, outputID);

        return false;
    }

    //==============================================================================
    struct MIDIEvent
    {
        MIDIEvent (const MIDIEvent&) = default;

        MIDIEvent (uint32_t index, uint8_t byte0, uint8_t byte1, uint8_t byte2)
            : frameIndex (index), packedData ((int) ((byte0 << 16) + (byte1 << 8) + (byte2)))
        {
        }

        uint32_t frameIndex;
        int packedData;
    };

    static int getPackedMIDIEvent (const MIDIEvent& m)   { return m.packedData; }

    struct EndpointInfo
    {
        EndpointDetails details;
        int audioChannelIndex = 0;
        bool isMIDI = false;
    };

    //==============================================================================
    struct AudioPlayerSession   : public Venue::Session
    {
        AudioPlayerSession (AudioPlayerVenue& v)  : venue (v)
        {
            performer = venue.performerFactory->createPerformer();

            if (venue.audioDevice != nullptr)
                updateDeviceProperties (*venue.audioDevice);
        }

        ~AudioPlayerSession() override
        {
            unload();
        }

        soul::ArrayView<const soul::EndpointDetails> getInputEndpoints() override             { return performer->getInputEndpoints(); }
        soul::ArrayView<const soul::EndpointDetails> getOutputEndpoints() override            { return performer->getOutputEndpoints(); }

        bool load (CompileMessageList& messageList, const Program& p) override
        {
            unload();

            if (performer->load (messageList, p))
            {
                setState (State::loaded);
                return true;
            }

            return false;
        }

        EndpointHandle getEndpointHandle (const EndpointID& endpointID) override  { return performer->getEndpointHandle (endpointID); }

        uint32_t setNextInputStreamFrames (EndpointHandle handle, const Value& frameArray) override
        {
            return performer->setNextInputStreamFrames (handle, frameArray);
        }

        void setSparseInputStreamTarget (EndpointHandle handle, const Value& targetFrameValue, uint32_t numFramesToReachValue, float curveShape) override
        {
            performer->setSparseInputStreamTarget (handle, targetFrameValue, numFramesToReachValue, curveShape);
        }

        void setInputValue (EndpointHandle handle, const Value& newValue) override
        {
            performer->setInputValue (handle, newValue);
        }

        void addInputEvent (EndpointHandle handle, const Value& eventData) override
        {
            performer->addInputEvent (handle, eventData);
        }

        const Value* getOutputStreamFrames (EndpointHandle handle) override
        {
            return performer->getOutputStreamFrames (handle);
        }

        void iterateOutputEvents (EndpointHandle handle, Performer::HandleNextOutputEventFn fn) override
        {
            performer->iterateOutputEvents (handle, std::move (fn));
        }

        bool isEndpointActive (const EndpointID& e) override
        {
            return performer->isEndpointActive (e);
        }

        bool link (CompileMessageList& messageList, const LinkOptions& linkOptions) override
        {
            buildOperationList();

            if (state == State::loaded && performer->link (messageList, linkOptions, {}))
            {
                setState (State::linked);
                return true;
            }

            return false;
        }

        bool isRunning() override
        {
            return state == State::running;
        }

        bool start() override
        {
            if (state == State::linked)
            {
                SOUL_ASSERT (performer->isLinked());

                if (venue.startSession (this))
                    setState (State::running);
            }

            return isRunning();
        }

        void stop() override
        {
            if (isRunning())
            {
                venue.stopSession (this);
                setState (State::linked);
            }
        }

        void unload() override
        {
            stop();
            performer->unload();
            preRenderOperations.clear();
            postRenderOperations.clear();
            inputCallbacks.clear();
            outputCallbacks.clear();
            connections.clear();
            setState (State::empty);
        }

        Status getStatus() override
        {
            Status s;
            s.state = state;
            s.cpu = venue.loadMeasurer.getCurrentLoad();
            s.xruns = performer->getXRuns();
            s.sampleRate = currentRateAndBlockSize.sampleRate;
            s.blockSize = currentRateAndBlockSize.blockSize;

            if (venue.audioDevice != nullptr)
            {
                auto deviceXruns = venue.audioDevice->getXRunCount();

                if (deviceXruns > 0) // < 0 means not known
                    s.xruns += (uint32_t) deviceXruns;
            }

            return s;
        }

        void setState (State newState)
        {
            if (state != newState)
            {
                state = newState;

                if (stateChangeCallback != nullptr)
                    stateChangeCallback (state);
            }
        }

        void setStateChangeCallback (StateChangeCallbackFn f) override     { stateChangeCallback = std::move (f); }

        bool addInputEndpointFIFOCallback (EndpointID endpoint, InputEndpointFIFOChangedFn callback) override
        {
            if (! containsEndpoint (performer->getInputEndpoints(), endpoint))
                return false;

            inputCallbacks.push_back ({ performer->getEndpointHandle (endpoint), std::move (callback) });
            return true;
        }

        bool addOutputEndpointFIFOCallback (EndpointID endpoint, OutputEndpointFIFOChangedFn callback) override
        {
            if (! containsEndpoint (performer->getOutputEndpoints(), endpoint))
                return false;

            outputCallbacks.push_back ({ performer->getEndpointHandle (endpoint), std::move (callback) });
            return true;
        }

        void prepareToPlay (juce::AudioIODevice& device)
        {
            updateDeviceProperties (device);
        }

        void deviceStopped()
        {
            currentRateAndBlockSize = {};
        }

        bool connectInputEndpoint (const EndpointInfo& externalEndpoint, EndpointID inputID)
        {
            for (auto& details : performer->getInputEndpoints())
            {
                if (details.endpointID == inputID)
                {
                    if (isStream (details.kind) && ! externalEndpoint.isMIDI)
                    {
                        connections.push_back ({ externalEndpoint.audioChannelIndex, -1, false, details.endpointID });
                        return true;
                    }

                    if (isEvent (details.kind) && externalEndpoint.isMIDI)
                    {
                        connections.push_back ({ -1, -1, true, details.endpointID });
                        return true;
                    }
                }
            }

            return false;
        }

        bool connectOutputEndpoint (const EndpointInfo& externalEndpoint, EndpointID outputID)
        {
            for (auto& details : performer->getOutputEndpoints())
            {
                if (details.endpointID == outputID)
                {
                    if (isStream (details.kind) && ! externalEndpoint.isMIDI)
                    {
                        connections.push_back ({ -1, externalEndpoint.audioChannelIndex, false, details.endpointID });
                        return true;
                    }
                }
            }

            return false;
        }

        void buildOperationList()
        {
            preRenderOperations.clear();
            postRenderOperations.clear();

            for (auto& connection : connections)
            {
                auto& perf = *performer;
                auto endpointHandle = performer->getEndpointHandle (connection.endpointID);

                if (connection.isMIDI)
                {
                    if (isMIDIEventEndpoint (findDetailsForID (perf.getInputEndpoints(), connection.endpointID)))
                    {
                        preRenderOperations.push_back ([&perf, endpointHandle] (RenderContext& rc)
                        {
                            for (uint32_t i = 0; i < rc.midiInCount; ++i)
                                perf.addInputEvent (endpointHandle, Value ((int32_t) rc.midiIn[i].packedData));
                        });
                    }
                }
                else if (connection.audioInputStreamIndex >= 0)
                {
                    auto& details = findDetailsForID (perf.getInputEndpoints(), connection.endpointID);
                    auto& frameType = details.getSingleSampleType();
                    auto buffer = soul::Value::zeroInitialiser (frameType.createArray (currentRateAndBlockSize.blockSize));
                    auto startChannel = (uint32_t) connection.audioInputStreamIndex;
                    auto numSourceChans = (uint32_t) frameType.getVectorSize();

                    if (frameType.isFloatingPoint())
                    {
                        auto is64Bit = frameType.isFloat64();

                        preRenderOperations.push_back ([&perf, endpointHandle, buffer, startChannel, numSourceChans, is64Bit] (RenderContext& rc) mutable
                        {
                            rc.copyInputFrames (startChannel, numSourceChans, buffer, is64Bit);
                            ignoreUnused (perf.setNextInputStreamFrames (endpointHandle, buffer));
                        });
                    }
                    else
                    {
                        SOUL_ASSERT_FALSE;
                    }
                }
                else if (connection.audioOutputStreamIndex >= 0)
                {
                    auto& details = findDetailsForID (perf.getOutputEndpoints(), connection.endpointID);
                    auto& frameType = details.getSingleSampleType();
                    auto buffer = soul::Value::zeroInitialiser (frameType.createArray (currentRateAndBlockSize.blockSize));
                    auto startChannel = (uint32_t) connection.audioOutputStreamIndex;
                    auto numDestChans = (uint32_t) frameType.getVectorSize();

                    if (frameType.isFloatingPoint())
                    {
                        auto is64Bit = frameType.isFloat64();

                        postRenderOperations.push_back ([&perf, endpointHandle, buffer, startChannel, numDestChans, is64Bit] (RenderContext& rc)
                        {
                            if (auto outputFrames = perf.getOutputStreamFrames (endpointHandle))
                                rc.copyOutputFrames (startChannel, numDestChans, *outputFrames, is64Bit);
                            else
                                SOUL_ASSERT_FALSE;
                        });
                    }
                    else
                    {
                        SOUL_ASSERT_FALSE;
                    }
                }
            }
        }

        void processBlock (soul::DiscreteChannelSet<const float> input,
                           soul::DiscreteChannelSet<float> output,
                           ArrayView<const MIDIEvent> midiIn)
        {
            auto maxBlockSize = std::min (512u, currentRateAndBlockSize.blockSize);
            SOUL_ASSERT (input.numFrames == output.numFrames && maxBlockSize != 0);

            RenderContext context { totalFramesRendered, input, output, midiIn.data(),
                                    nullptr, 0, (uint32_t) midiIn.size(), 0, 0 };

            context.iterateInBlocks (maxBlockSize, [&] (RenderContext& rc)
            {
                performer->prepare (rc.inputChannels.numFrames);

                for (auto& op : preRenderOperations)
                    op (rc);

                for (auto& c : inputCallbacks)
                    c.callback (*this, c.endpointHandle);

                performer->advance();

                for (auto& op : postRenderOperations)
                    op (rc);

                for (auto& c : outputCallbacks)
                    c.callback (*this, c.endpointHandle);
            },
            [] (MIDIEvent midi) { return midi.frameIndex; });

            totalFramesRendered += input.numFrames;
        }

        void updateDeviceProperties (juce::AudioIODevice& device)
        {
            currentRateAndBlockSize = SampleRateAndBlockSize (device.getCurrentSampleRate(),
                                                              (uint32_t) device.getCurrentBufferSizeSamples());
        }

        AudioPlayerVenue& venue;
        std::unique_ptr<Performer> performer;
        SampleRateAndBlockSize currentRateAndBlockSize;
        uint64_t totalFramesRendered = 0;
        StateChangeCallbackFn stateChangeCallback;

        struct InputCallback
        {
            EndpointHandle endpointHandle;
            InputEndpointFIFOChangedFn callback;
        };

        struct OutputCallback
        {
            EndpointHandle endpointHandle;
            OutputEndpointFIFOChangedFn callback;
        };

        std::vector<InputCallback> inputCallbacks;
        std::vector<OutputCallback> outputCallbacks;

        struct Connection
        {
            int audioInputStreamIndex = -1, audioOutputStreamIndex = -1;
            bool isMIDI = false;
            EndpointID endpointID;
        };

        using RenderContext = AudioMIDIWrapper<MIDIEvent>::RenderContext;

        std::vector<Connection> connections;
        std::vector<std::function<void(RenderContext&)>> preRenderOperations, postRenderOperations;

        State state = State::empty;
    };

    //==============================================================================
    bool startSession (AudioPlayerSession* s)
    {
        std::lock_guard<decltype(activeSessionLock)> lock (activeSessionLock);

        if (! contains (activeSessions, s))
            activeSessions.push_back (s);

        return true;
    }

    bool stopSession (AudioPlayerSession* s)
    {
        std::lock_guard<decltype(activeSessionLock)> lock (activeSessionLock);
        removeFirst (activeSessions, [=] (AudioPlayerSession* i) { return i == s; });
        return true;
    }


private:
    //==============================================================================
    Requirements requirements;
    std::unique_ptr<PerformerFactory> performerFactory;

    std::unique_ptr<juce::AudioIODevice> audioDevice;
    juce::StringArray lastMidiDevices;
    std::vector<std::unique_ptr<juce::MidiInput>> midiInputs;

    std::unique_ptr<juce::MidiMessageCollector> midiCollector;
    juce::MidiBuffer incomingMIDI;
    std::vector<MIDIEvent> incomingMIDIEvents;

    CPULoadMeasurer loadMeasurer;

    std::vector<EndpointInfo> sourceEndpoints, sinkEndpoints;

    std::recursive_mutex activeSessionLock;
    std::vector<AudioPlayerSession*> activeSessions;

    uint64_t totalFramesProcessed = 0;
    std::atomic<uint32_t> audioCallbackCount { 0 };
    static constexpr uint64_t numWarmUpSamples = 15000;

    uint32_t lastCallbackCount = 0;
    juce::Time lastMIDIDeviceCheck, lastCallbackCountChange;

    void log (const juce::String& text)
    {
        if (requirements.printLogMessage != nullptr)
            requirements.printLogMessage (text.toRawUTF8());
    }

    //==============================================================================
    const EndpointInfo* findEndpoint (ArrayView<EndpointInfo> endpoints, const EndpointID& endpointID) const
    {
        for (auto& e : endpoints)
            if (e.details.endpointID == endpointID)
                return std::addressof (e);

        return {};
    }

    static std::vector<EndpointDetails> convertEndpointList (ArrayView<EndpointInfo> sourceList)
    {
        std::vector<EndpointDetails> result;

        for (auto& e : sourceList)
            result.push_back (e.details);

        return result;
    }

    //==============================================================================
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override
    {
        lastCallbackCount = 0;
        audioCallbackCount = 0;

        if (midiCollector)
            midiCollector->reset (device->getCurrentSampleRate());

        incomingMIDI.ensureSize (1024);
        incomingMIDIEvents.reserve (1024);

        loadMeasurer.reset();

        std::lock_guard<decltype(activeSessionLock)> lock (activeSessionLock);

        for (auto& s : activeSessions)
            s->prepareToPlay (*device);
    }

    void audioDeviceStopped() override
    {
        {
            std::lock_guard<decltype(activeSessionLock)> lock (activeSessionLock);

            for (auto& s : activeSessions)
                s->deviceStopped();
        }

        loadMeasurer.reset();
    }

    void audioDeviceIOCallback (const float** inputChannelData, int numInputChannels,
                                float** outputChannelData, int numOutputChannels,
                                int numFrames) override
    {
        juce::ScopedNoDenormals disableDenormals;
        loadMeasurer.startMeasurement();

        ++audioCallbackCount;

        for (int i = 0; i < numOutputChannels; ++i)
            if (auto* chan = outputChannelData[i])
                juce::FloatVectorOperations::clear (chan, numFrames);

        if (midiCollector)
        {
            midiCollector->removeNextBlockOfMessages (incomingMIDI, numFrames);

            incomingMIDIEvents.clear();

            juce::MidiMessage message;
            int frameOffset = 0;

            for (juce::MidiBuffer::Iterator iterator (incomingMIDI); iterator.getNextEvent (message, frameOffset);)
            {
                auto* rawData = message.getRawData();
                incomingMIDIEvents.push_back ({ (uint32_t) frameOffset, rawData[0], rawData[1], rawData[2] });
            }
        }

        if (totalFramesProcessed > numWarmUpSamples)
        {
            std::lock_guard<decltype(activeSessionLock)> lock (activeSessionLock);

            for (auto& s : activeSessions)
                s->processBlock ({ inputChannelData,  (uint32_t) numInputChannels,  0, (uint32_t) numFrames },
                                 { outputChannelData, (uint32_t) numOutputChannels, 0, (uint32_t) numFrames },
                                 incomingMIDIEvents);
        }

        incomingMIDI.clear();

        totalFramesProcessed += static_cast<uint64_t> (numFrames);
        loadMeasurer.stopMeasurement();
    }

    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message) override
    {
        if (message.getRawDataSize() < 4)  // long messages are ignored for now...
        {
            if (midiCollector)
                midiCollector->addMessageToQueue (message);
            else
                incomingMIDI.addEvent (message, 0);
        }
    }

    void timerCallback() override
    {
        checkMIDIDevices();
        checkForStalledProcessor();
    }

    void checkForStalledProcessor()
    {
        auto now = juce::Time::getCurrentTime();

        if (lastCallbackCount != audioCallbackCount)
        {
            lastCallbackCount = audioCallbackCount;
            lastCallbackCountChange = now;
        }

        if (lastCallbackCount != 0 && now > lastCallbackCountChange + juce::RelativeTime::seconds (2))
        {
            log ("Fatal error! run() function took too long to execute.\n"
                 "Process terminating...");

            std::terminate();
        }
    }

    void openAudioDevice()
    {
        tryToCreateDeviceType ([] { return juce::AudioIODeviceType::createAudioIODeviceType_CoreAudio(); });
        tryToCreateDeviceType ([] { return juce::AudioIODeviceType::createAudioIODeviceType_iOSAudio(); });
        tryToCreateDeviceType ([] { return juce::AudioIODeviceType::createAudioIODeviceType_ASIO(); });
        tryToCreateDeviceType ([] { return juce::AudioIODeviceType::createAudioIODeviceType_WASAPI (false); });
        tryToCreateDeviceType ([] { return juce::AudioIODeviceType::createAudioIODeviceType_WASAPI (true); });
        tryToCreateDeviceType ([] { return juce::AudioIODeviceType::createAudioIODeviceType_DirectSound(); });
        tryToCreateDeviceType ([] { return juce::AudioIODeviceType::createAudioIODeviceType_Bela(); });
        tryToCreateDeviceType ([] { return juce::AudioIODeviceType::createAudioIODeviceType_Oboe(); });
        tryToCreateDeviceType ([] { return juce::AudioIODeviceType::createAudioIODeviceType_OpenSLES(); });
        tryToCreateDeviceType ([] { return juce::AudioIODeviceType::createAudioIODeviceType_ALSA(); });

        if (audioDevice != nullptr)
        {
            if (requirements.numInputChannels > 0)
            {
                juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio,
                                                   [] (bool granted)
                                                   {
                                                       if (! granted)
                                                           SOUL_ASSERT_FALSE;
                                                   });
            }

            auto error = audioDevice->open (getBitSetForNumChannels (requirements.numInputChannels),
                                            getBitSetForNumChannels (requirements.numOutputChannels),
                                            requirements.sampleRate,
                                            requirements.blockSize);

            if (error.isEmpty())
            {
                auto numInputChannels  = audioDevice->getActiveInputChannels().countNumberOfSetBits();
                auto numOutputChannels = audioDevice->getActiveOutputChannels().countNumberOfSetBits();

                if (numInputChannels > 0)
                    addEndpoint (sourceEndpoints,
                                 EndpointKind::stream,
                                 EndpointID::create ("defaultIn"),
                                 "defaultIn",
                                 getVectorType (numInputChannels),
                                 0, false);

                if (numOutputChannels > 0)
                    addEndpoint (sinkEndpoints,
                                 EndpointKind::stream,
                                 EndpointID::create ("defaultOut"),
                                 "defaultOut",
                                 getVectorType (numOutputChannels),
                                 0, false);

                addEndpoint (sourceEndpoints,
                             EndpointKind::event,
                             EndpointID::create ("defaultMidiIn"),
                             "defaultMidiIn",
                             soul::PrimitiveType::int32,
                             0, true);

                addEndpoint (sinkEndpoints,
                             EndpointKind::event,
                             EndpointID::create ("defaultMidiOut"),
                             "defaultMidiOut",
                             soul::PrimitiveType::int32,
                             0, true);

                log (soul::utilities::getAudioDeviceSetup (*audioDevice));
                audioDevice->start (this);

                return;
            }
        }

        audioDevice.reset();
        loadMeasurer.reset();
        SOUL_ASSERT_FALSE;
    }

    static soul::Type getVectorType (int size)    { return (soul::Type::createVector (soul::PrimitiveType::float32, static_cast<size_t> (size))); }

    static void addEndpoint (std::vector<EndpointInfo>& list, EndpointKind kind,
                             EndpointID id, std::string name, Type sampleType,
                             int audioChannelIndex, bool isMIDI)
    {
        EndpointInfo e;

        e.details.endpointID    = std::move (id);
        e.details.name          = std::move (name);
        e.details.kind          = kind;
        e.details.sampleTypes.push_back (sampleType);
        e.details.strideBytes   = 0;

        e.audioChannelIndex     = audioChannelIndex;
        e.isMIDI                = isMIDI;

        list.push_back (e);
    }

    void tryToCreateDeviceType (std::function<juce::AudioIODeviceType*()> createDeviceType)
    {
        if (audioDevice == nullptr)
        {
            if (auto type = std::unique_ptr<juce::AudioIODeviceType> (createDeviceType()))
            {
                type->scanForDevices();

                juce::String outputDevice, inputDevice;

                if (requirements.numOutputChannels > 0)
                    outputDevice = type->getDeviceNames(false) [type->getDefaultDeviceIndex(false)];

                if (requirements.numInputChannels > 0)
                    inputDevice = type->getDeviceNames(true) [type->getDefaultDeviceIndex(true)];

                audioDevice.reset (type->createDevice (outputDevice, inputDevice));
            }
        }
    }

    static juce::BigInteger getBitSetForNumChannels (int num)
    {
        juce::BigInteger b;
        b.setRange (0, num, true);
        return b;
    }

    void checkMIDIDevices()
    {
        auto now = juce::Time::getCurrentTime();

        if (now > lastMIDIDeviceCheck + juce::RelativeTime::seconds (2))
        {
            lastMIDIDeviceCheck = now;
            openMIDIDevices();
        }
    }

    void openMIDIDevices()
    {
        lastMIDIDeviceCheck = juce::Time::getCurrentTime();

        auto devices = juce::MidiInput::getDevices();

        if (lastMidiDevices != devices)
        {
            lastMidiDevices = devices;

            for (auto& mi : midiInputs)
            {
                log ("Closing MIDI device: " + mi->getName());
                mi.reset();
            }

            midiInputs.clear();

            for (int i = devices.size(); --i >= 0;)
                if (auto mi = juce::MidiInput::openDevice (i, this))
                    midiInputs.emplace_back (std::move (mi));

            for (auto& mi : midiInputs)
            {
                log ("Opening MIDI device: " + mi->getName());
                mi->start();
            }
        }

        startTimer (2000);
    }
};

//==============================================================================
std::unique_ptr<Venue> createAudioPlayerVenue (const Requirements& requirements,
                                               std::unique_ptr<PerformerFactory> performerFactory)
{
    return std::make_unique<AudioPlayerVenue> (requirements, std::move (performerFactory));
}

}
