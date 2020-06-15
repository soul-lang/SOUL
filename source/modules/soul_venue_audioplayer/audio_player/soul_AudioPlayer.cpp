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
struct AudioMIDISystem  : private juce::AudioIODeviceCallback,
                          private juce::MidiInputCallback,
                          private juce::Timer
{
    AudioMIDISystem (Requirements r) : requirements (r)
    {
        if (requirements.sampleRate < 1000.0 || requirements.sampleRate > 48000.0 * 8)
            requirements.sampleRate = 0;

        if (requirements.blockSize < 1 || requirements.blockSize > 2048)
            requirements.blockSize = 0;

        constexpr uint32_t midiFIFOSize = 1024;
        inputMIDIBuffer.reserve (midiFIFOSize);
        midiFIFO.reset (midiFIFOSize);
        openAudioDevice();
        startTimerHz (3);
    }

    ~AudioMIDISystem() override
    {
        audioDevice.reset();
        midiInputs.clear();
    }

    //==============================================================================
    struct MIDIEvent
    {
        uint32_t frameIndex = 0;
        choc::midi::ShortMessage message;
    };

    struct Callback
    {
        virtual ~Callback() = default;

        virtual void render (DiscreteChannelSet<const float> input,
                             DiscreteChannelSet<float> output,
                             const MIDIEvent* midiIn,
                             MIDIEvent* midiOut,
                             uint32_t midiInCount,
                             uint32_t midiOutCapacity,
                             uint32_t& numMIDIOutMessages) = 0;

        virtual void renderStarting (double sampleRate, uint32_t blockSize) = 0;
        virtual void renderStopped() = 0;
    };

    void setCallback (Callback* newCallback)
    {
        Callback* oldCallback = nullptr;

        {
            std::lock_guard<decltype(callbackLock)> lock (callbackLock);

            if (callback != newCallback)
            {
                if (newCallback != nullptr && sampleRate != 0)
                    newCallback->renderStarting (sampleRate, blockSize);

                oldCallback = callback;
                callback = newCallback;
            }
        }

        if (oldCallback != nullptr)
            oldCallback->renderStopped();
    }

    double getSampleRate() const                { return sampleRate; }
    uint32_t getMaxBlockSize() const            { return blockSize; }

    float getCPULoad() const                    { return loadMeasurer.getCurrentLoad(); }
    int getXRunCount() const                    { return audioDevice != nullptr ? audioDevice->getXRunCount() : -1; }

    int getNumInputChannels() const             { return audioDevice != nullptr ? audioDevice->getActiveInputChannels().countNumberOfSetBits() : 0; }
    int getNumOutputChannels() const            { return audioDevice != nullptr ? audioDevice->getActiveOutputChannels().countNumberOfSetBits() : 0; }

private:
    //==============================================================================
    Requirements requirements;

    std::unique_ptr<juce::AudioIODevice> audioDevice;
    uint64_t totalFramesProcessed = 0;
    std::atomic<uint32_t> audioCallbackCount { 0 };
    uint32_t lastCallbackCount = 0;
    static constexpr uint64_t numWarmUpFrames = 15000;
    double sampleRate = 0;
    uint32_t blockSize = 0;

    juce::StringArray lastMidiDevices;
    std::vector<std::unique_ptr<juce::MidiInput>> midiInputs;
    std::chrono::system_clock::time_point lastMIDIDeviceCheckTime, lastKnownActiveCallbackTime;

    using MIDIClock = std::chrono::high_resolution_clock;

    struct IncomingMIDIEvent
    {
        MIDIClock::time_point time;
        choc::midi::ShortMessage message;
    };

    choc::fifo::SingleReaderSingleWriterFIFO<IncomingMIDIEvent> midiFIFO;
    std::vector<MIDIEvent> inputMIDIBuffer;
    MIDIClock::time_point lastMIDIBlockTime;

    CPULoadMeasurer loadMeasurer;

    std::mutex callbackLock;
    Callback* callback = nullptr;

    //==============================================================================
    void log (const std::string& text)
    {
        if (requirements.printLogMessage != nullptr)
            requirements.printLogMessage (text);
    }

    //==============================================================================
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override
    {
        sampleRate = device->getCurrentSampleRate();
        blockSize  = static_cast<uint32_t> (device->getCurrentBufferSizeSamples());

        lastCallbackCount = 0;
        audioCallbackCount = 0;
        midiFIFO.reset();

        loadMeasurer.reset();

        std::lock_guard<decltype(callbackLock)> lock (callbackLock);

        if (callback != nullptr)
            callback->renderStarting (sampleRate, blockSize);
    }

    void audioDeviceStopped() override
    {
        sampleRate = 0;
        blockSize = 0;
        loadMeasurer.reset();

        std::lock_guard<decltype(callbackLock)> lock (callbackLock);

        if (callback != nullptr)
            callback->renderStopped();
    }

    void audioDeviceIOCallback (const float** inputChannelData, int numInputChannels,
                                float** outputChannelData, int numOutputChannels,
                                int numFrames) override
    {
        loadMeasurer.startMeasurement();
        juce::ScopedNoDenormals disableDenormals;
        ++audioCallbackCount;

        for (int i = 0; i < numOutputChannels; ++i)
            juce::FloatVectorOperations::clear (outputChannelData[i], numFrames);

       #if ! JUCE_BELA
        fillMIDIInputBuffer (static_cast<uint32_t> (numFrames));
       #endif

        if (totalFramesProcessed > numWarmUpFrames)
        {
            std::lock_guard<decltype(callbackLock)> lock (callbackLock);

            if (callback != nullptr)
            {
                uint32_t numMIDIOut = 0;

                callback->render ({ inputChannelData,  (uint32_t) numInputChannels,  0, (uint32_t) numFrames },
                                  { outputChannelData, (uint32_t) numOutputChannels, 0, (uint32_t) numFrames },
                                  inputMIDIBuffer.data(), nullptr, static_cast<uint32_t> (inputMIDIBuffer.size()), 0, numMIDIOut);
            }
        }

        totalFramesProcessed += static_cast<uint64_t> (numFrames);
        loadMeasurer.stopMeasurement();

       #if JUCE_BELA
        inputMIDIBuffer.clear();
       #endif
    }

    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message) override
    {
        if (message.getRawDataSize() < 4)  // long messages are ignored for now...
        {
            auto bytes = message.getRawData();
            auto m = choc::midi::ShortMessage (bytes[0], bytes[1], bytes[2]);

           #if JUCE_BELA
            inputMIDIBuffer.push_back ({ 0, m });
           #else
            midiFIFO.push ({ MIDIClock::now(), m });
           #endif
        }
    }

    void fillMIDIInputBuffer (uint32_t numFrames)
    {
        inputMIDIBuffer.clear();
        auto now = MIDIClock::now();
        using TimeSeconds = std::chrono::duration<double, std::ratio<1, 1>>;
        auto startOfFrame = lastMIDIBlockTime;
        lastMIDIBlockTime = now;

        if (midiFIFO.getUsedSlots() != 0)
        {
            auto frameLength = TimeSeconds (1.0 / sampleRate);
            IncomingMIDIEvent e;

            while (midiFIFO.pop (e))
            {
                TimeSeconds timeSinceBufferStart = e.time - startOfFrame;
                auto frame = static_cast<int> (timeSinceBufferStart.count() / frameLength.count());

                if (frame < 0)
                {
                    if (frame < -40000)
                        break;

                    frame = 0;
                }

                auto frameIndex = static_cast<uint32_t> (frame) < numFrames ? static_cast<uint32_t> (frame)
                                                                            : static_cast<uint32_t> (numFrames - 1);
                inputMIDIBuffer.push_back ({ frameIndex, e.message });
            }
        }
    }

    void timerCallback() override
    {
        auto now = std::chrono::system_clock::now();
        checkMIDIDevices (now);
        checkForStalledProcessor (now);
    }

    void checkForStalledProcessor (std::chrono::system_clock::time_point now)
    {
        if (lastCallbackCount != audioCallbackCount)
        {
            lastCallbackCount = audioCallbackCount;
            lastKnownActiveCallbackTime = now;
            return;
        }

        if (lastCallbackCount != 0 && now > lastKnownActiveCallbackTime + std::chrono::milliseconds (2000))
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
                                                   [this] (bool granted)
                                                   {
                                                       if (! granted)
                                                           log ("Failed to get audio input permission");
                                                   });
            }

            auto getBitSetForNumChannels = [] (int num)
            {
                juce::BigInteger b;
                b.setRange (0, num, true);
                return b;
            };

            auto error = audioDevice->open (getBitSetForNumChannels (requirements.numInputChannels),
                                            getBitSetForNumChannels (requirements.numOutputChannels),
                                            requirements.sampleRate,
                                            requirements.blockSize);

            if (error.isEmpty())
            {
                log (soul::utilities::getAudioDeviceSetup (*audioDevice));
                audioDevice->start (this);
                return;
            }

            log (("Error opening audio device: " + error).toStdString());
        }

        audioDevice.reset();
        loadMeasurer.reset();
        SOUL_ASSERT_FALSE;
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

    void checkMIDIDevices (std::chrono::system_clock::time_point now)
    {
        if (now > lastMIDIDeviceCheckTime + std::chrono::seconds (2))
        {
            lastMIDIDeviceCheckTime = now;
            openMIDIDevices();
        }
    }

    void openMIDIDevices()
    {
        lastMIDIDeviceCheckTime = std::chrono::system_clock::now();
        auto devices = juce::MidiInput::getDevices();

        if (lastMidiDevices != devices)
        {
            lastMidiDevices = devices;

            for (auto& mi : midiInputs)
            {
                log (("Closing MIDI device: " + mi->getName()).toStdString());
                mi.reset();
            }

            midiInputs.clear();

            for (int i = devices.size(); --i >= 0;)
                if (auto mi = juce::MidiInput::openDevice (i, this))
                    midiInputs.emplace_back (std::move (mi));

            for (auto& mi : midiInputs)
            {
                log (("Opening MIDI device: " + mi->getName()).toStdString());
                mi->start();
            }
        }

        startTimer (2000);
    }
};

//==============================================================================
class AudioPlayerVenue   : public soul::Venue,
                           private AudioMIDISystem::Callback
{
public:
    AudioPlayerVenue (Requirements r, std::unique_ptr<PerformerFactory> factory)
        : audioSystem (std::move (r)),
          performerFactory (std::move (factory))
    {
        createDeviceEndpoints (audioSystem.getNumInputChannels(),
                               audioSystem.getNumOutputChannels());
    }

    ~AudioPlayerVenue() override
    {
        SOUL_ASSERT (activeSessions.empty());
        audioSystem.setCallback (nullptr);
        performerFactory.reset();
    }

    std::unique_ptr<Venue::Session> createSession() override
    {
        return std::make_unique<AudioPlayerSession> (*this);
    }

    std::vector<EndpointDetails> getSourceEndpoints() override    { return convertEndpointList (sourceEndpoints); }
    std::vector<EndpointDetails> getSinkEndpoints() override      { return convertEndpointList (sinkEndpoints); }

    bool connectSessionInputEndpoint (Session& session, EndpointID inputID, EndpointID venueSourceID) override
    {
        if (auto audioSession = dynamic_cast<AudioPlayerSession*> (&session))
            if (auto venueEndpoint = findEndpoint (sourceEndpoints, venueSourceID))
                return audioSession->connectInputEndpoint (*venueEndpoint, inputID);

        return false;
    }

    bool connectSessionOutputEndpoint (Session& session, EndpointID outputID, EndpointID venueSinkID) override
    {
        if (auto audioSession = dynamic_cast<AudioPlayerSession*> (&session))
            if (auto venueEndpoint = findEndpoint (sinkEndpoints, venueSinkID))
                return audioSession->connectOutputEndpoint (*venueEndpoint, outputID);

        return false;
    }

    //==============================================================================
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

        void setNextInputStreamFrames (EndpointHandle handle, const choc::value::ValueView& frameArray) override
        {
            performer->setNextInputStreamFrames (handle, frameArray);
        }

        void setSparseInputStreamTarget (EndpointHandle handle, const choc::value::ValueView& targetFrameValue, uint32_t numFramesToReachValue, float curveShape) override
        {
            performer->setSparseInputStreamTarget (handle, targetFrameValue, numFramesToReachValue, curveShape);
        }

        void setInputValue (EndpointHandle handle, const choc::value::ValueView& newValue) override
        {
            performer->setInputValue (handle, newValue);
        }

        void addInputEvent (EndpointHandle handle, const choc::value::ValueView& eventData) override
        {
            performer->addInputEvent (handle, eventData);
        }

        choc::value::ValueView getOutputStreamFrames (EndpointHandle handle) override
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

        bool link (CompileMessageList& messageList, const BuildSettings& settings) override
        {
            maxBlockSize = settings.maxBlockSize;
            buildOperationList();

            if (state == State::loaded && performer->link (messageList, settings, {}))
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
                totalFramesRendered = 0;
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
            s.cpu = venue.audioSystem.getCPULoad();
            s.sampleRate = venue.audioSystem.getSampleRate();
            s.blockSize = venue.audioSystem.getMaxBlockSize();
            s.xruns = performer->getXRuns();

            auto deviceXruns = venue.audioSystem.getXRunCount();

            if (deviceXruns > 0) // < 0 means not known
                s.xruns += (uint32_t) deviceXruns;

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

        uint64_t getTotalFramesRendered() const override                   { return totalFramesRendered; }

        bool setInputEndpointServiceCallback (EndpointID endpoint, EndpointServiceFn callback) override
        {
            if (! containsEndpoint (performer->getInputEndpoints(), endpoint))
                return false;

            inputCallbacks.push_back ({ performer->getEndpointHandle (endpoint), std::move (callback) });
            return true;
        }

        bool setOutputEndpointServiceCallback (EndpointID endpoint, EndpointServiceFn callback) override
        {
            if (! containsEndpoint (performer->getOutputEndpoints(), endpoint))
                return false;

            outputCallbacks.push_back ({ performer->getEndpointHandle (endpoint), std::move (callback) });
            return true;
        }

        void deviceStopped()
        {
        }

        bool connectInputEndpoint (const EndpointInfo& externalEndpoint, EndpointID inputID)
        {
            for (auto& details : performer->getInputEndpoints())
            {
                if (details.endpointID == inputID)
                {
                    if (isStream (details) && ! externalEndpoint.isMIDI)
                    {
                        connections.push_back ({ externalEndpoint.audioChannelIndex, -1, false, details.endpointID });
                        return true;
                    }

                    if (isEvent (details) && externalEndpoint.isMIDI)
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
                    if (isStream (details) && ! externalEndpoint.isMIDI)
                    {
                        connections.push_back ({ -1, externalEndpoint.audioChannelIndex, false, details.endpointID });
                        return true;
                    }
                }
            }

            return false;
        }

        static int getPackedMIDIEvent (const AudioMIDISystem::MIDIEvent& m)   { return (int) ((m.message.data[0] << 16) + (m.message.data[1] << 8) + m.message.data[2]); }

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
                        auto midiEvent = choc::value::createObject ("soul::midi::Message",
                                                                    "midiBytes", int32_t {});

                        preRenderOperations.push_back ([&perf, endpointHandle, midiEvent] (RenderContext& rc) mutable
                        {
                            for (uint32_t i = 0; i < rc.midiInCount; ++i)
                            {
                                midiEvent.getObjectMemberAt (0).value.set ((int32_t) getPackedMIDIEvent (rc.midiIn[i]));
                                perf.addInputEvent (endpointHandle, midiEvent);
                            }
                        });
                    }
                }
                else if (connection.audioInputStreamIndex >= 0)
                {
                    auto& details = findDetailsForID (perf.getInputEndpoints(), connection.endpointID);
                    auto& frameType = details.getFrameType();
                    auto startChannel = static_cast<uint32_t> (connection.audioInputStreamIndex);
                    auto numChans = frameType.getNumElements();

                    if (frameType.isFloat() || (frameType.isVector() && frameType.getElementType().isFloat()))
                    {
                        AllocatedChannelSet<InterleavedChannelSet<float>> interleaved (numChans, maxBlockSize);

                        preRenderOperations.push_back ([&perf, endpointHandle, startChannel, numChans, interleaved] (RenderContext& rc)
                        {
                            copyChannelSet (interleaved.channelSet, rc.inputChannels.getChannelSet (startChannel, numChans));

                            perf.setNextInputStreamFrames (endpointHandle, choc::value::create2DArrayView (interleaved.channelSet.data,
                                                                                                           interleaved.channelSet.numFrames,
                                                                                                           interleaved.channelSet.numChannels));
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
                    auto& frameType = details.getFrameType();
                    auto startChannel = static_cast<uint32_t> (connection.audioOutputStreamIndex);
                    auto numChans = frameType.getNumElements();

                    if (frameType.isFloat() || (frameType.isVector() && frameType.getElementType().isFloat()))
                    {
                        postRenderOperations.push_back ([&perf, endpointHandle, startChannel, numChans] (RenderContext& rc)
                        {
                            copyChannelSetHandlingLengthDifference (rc.outputChannels.getChannelSet (startChannel, numChans),
                                                                    getChannelSetFromArray (perf.getOutputStreamFrames (endpointHandle)));
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
                           ArrayView<const AudioMIDISystem::MIDIEvent> midiIn)
        {
            auto maxFramesPerBlock = std::min (512u, maxBlockSize);
            SOUL_ASSERT (input.numFrames == output.numFrames && maxFramesPerBlock > 0);

            RenderContext context { totalFramesRendered, input, output, midiIn.data(),
                                    nullptr, 0, (uint32_t) midiIn.size(), 0, 0 };

            context.iterateInBlocks (maxFramesPerBlock, [&] (RenderContext& rc)
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
            [] (AudioMIDISystem::MIDIEvent midi) { return midi.frameIndex; });

            totalFramesRendered += input.numFrames;
        }

        AudioPlayerVenue& venue;
        std::unique_ptr<Performer> performer;
        uint32_t maxBlockSize = 0;
        std::atomic<uint64_t> totalFramesRendered { 0 };
        StateChangeCallbackFn stateChangeCallback;

        struct EndpointCallback
        {
            EndpointHandle endpointHandle;
            EndpointServiceFn callback;
        };

        std::vector<EndpointCallback> inputCallbacks, outputCallbacks;

        struct Connection
        {
            int audioInputStreamIndex = -1, audioOutputStreamIndex = -1;
            bool isMIDI = false;
            EndpointID endpointID;
        };

        using RenderContext = AudioMIDIWrapper<AudioMIDISystem::MIDIEvent>::RenderContext;

        std::vector<Connection> connections;
        std::vector<std::function<void(RenderContext&)>> preRenderOperations;
        std::vector<std::function<void(RenderContext&)>> postRenderOperations;

        State state = State::empty;
    };

    //==============================================================================
    bool startSession (AudioPlayerSession* s)
    {
        std::lock_guard<decltype(activeSessionLock)> lock (activeSessionLock);

        if (! contains (activeSessions, s))
            activeSessions.push_back (s);

        audioSystem.setCallback (this);
        return true;
    }

    bool stopSession (AudioPlayerSession* s)
    {
        std::lock_guard<decltype(activeSessionLock)> lock (activeSessionLock);
        removeFirst (activeSessions, [=] (AudioPlayerSession* i) { return i == s; });

        if (activeSessions.empty())
            audioSystem.setCallback (nullptr);
    
        return true;
    }


private:
    //==============================================================================
    AudioMIDISystem audioSystem;
    std::unique_ptr<PerformerFactory> performerFactory;

    std::vector<EndpointInfo> sourceEndpoints, sinkEndpoints;

    std::recursive_mutex activeSessionLock;
    std::vector<AudioPlayerSession*> activeSessions;

    //==============================================================================
    void createDeviceEndpoints (int numInputChannels, int numOutputChannels)
    {
        if (numInputChannels > 0)
            addEndpoint (sourceEndpoints,
                         EndpointType::stream,
                         EndpointID::create ("defaultIn"),
                         "defaultIn",
                         getVectorType (numInputChannels),
                         0, false);

        if (numOutputChannels > 0)
            addEndpoint (sinkEndpoints,
                         EndpointType::stream,
                         EndpointID::create ("defaultOut"),
                         "defaultOut",
                         getVectorType (numOutputChannels),
                         0, false);

        auto midiMessageType = soul::createMIDIEventEndpointType();

        addEndpoint (sourceEndpoints,
                     EndpointType::event,
                     EndpointID::create ("defaultMidiIn"),
                     "defaultMidiIn",
                     midiMessageType,
                     0, true);

        addEndpoint (sinkEndpoints,
                     EndpointType::event,
                     EndpointID::create ("defaultMidiOut"),
                     "defaultMidiOut",
                     midiMessageType,
                     0, true);
    }

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

    void renderStarting (double, uint32_t) override {}
    void renderStopped() override {}

    void render (DiscreteChannelSet<const float> input,
                 DiscreteChannelSet<float> output,
                 const AudioMIDISystem::MIDIEvent* midiIn,
                 AudioMIDISystem::MIDIEvent* /*midiOut*/,
                 uint32_t midiInCount,
                 uint32_t /*midiOutCapacity*/,
                 uint32_t& /*numMIDIOutMessages*/) override
    {
        std::lock_guard<decltype(activeSessionLock)> lock (activeSessionLock);

        for (auto& s : activeSessions)
            s->processBlock (input, output, { midiIn, midiIn + midiInCount });
    }

    static soul::Type getVectorType (int size)    { return (soul::Type::createVector (soul::PrimitiveType::float32, static_cast<size_t> (size))); }

    static void addEndpoint (std::vector<EndpointInfo>& list, EndpointType endpointType,
                             EndpointID id, std::string name, Type dataType,
                             int audioChannelIndex, bool isMIDI)
    {
        EndpointInfo e;

        e.details.endpointID    = std::move (id);
        e.details.name          = std::move (name);
        e.details.endpointType  = endpointType;
        e.details.dataTypes.push_back (dataType.getExternalType());

        e.audioChannelIndex     = audioChannelIndex;
        e.isMIDI                = isMIDI;

        list.push_back (e);
    }
};

//==============================================================================
std::unique_ptr<Venue> createAudioPlayerVenue (const Requirements& requirements,
                                               std::unique_ptr<PerformerFactory> performerFactory)
{
    return std::make_unique<AudioPlayerVenue> (requirements, std::move (performerFactory));
}

}
