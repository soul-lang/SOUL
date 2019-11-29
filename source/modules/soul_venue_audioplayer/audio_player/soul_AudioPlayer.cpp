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

    bool connectSessionInputEndpoint (Session& session,
                                      InputEndpoint& inputEndpoint,
                                      EndpointID venueSourceID) override
    {
        if (audioDevice != nullptr)
            if (auto audioSession = dynamic_cast<AudioPlayerSession*> (&session))
                if (auto venueEndpoint = findEndpoint (sourceEndpoints, venueSourceID))
                    return audioSession->connectInputEndpoint (venueEndpoint->audioChannelIndex,
                                                               venueEndpoint->isMIDI, inputEndpoint);

        return false;
    }

    bool connectSessionOutputEndpoint (Session& session,
                                       OutputEndpoint& outputEndpoint,
                                       EndpointID venueSinkID) override
    {
        if (audioDevice != nullptr)
            if (auto audioSession = dynamic_cast<AudioPlayerSession*> (&session))
                if (auto venueEndpoint = findEndpoint (sinkEndpoints, venueSinkID))
                    return audioSession->connectOutputEndpoint (venueEndpoint->audioChannelIndex,
                                                                venueEndpoint->isMIDI, outputEndpoint);

        return false;
    }

    //==============================================================================
    struct AudioPlayerSession   : public Venue::Session
    {
        AudioPlayerSession (AudioPlayerVenue& v)  : venue (v)
        {
            performer = venue.performerFactory->createPerformer();

            if (venue.audioDevice != nullptr)
                updateEndpointProperties (*venue.audioDevice);
        }

        ~AudioPlayerSession() override
        {
            unload();
        }

        std::vector<InputEndpoint::Ptr>  getInputEndpoints() override   { return performer->getInputEndpoints(); }
        std::vector<OutputEndpoint::Ptr> getOutputEndpoints() override  { return performer->getOutputEndpoints(); }

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

        bool link (CompileMessageList& messageList, const LinkOptions& linkOptions) override
        {
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
            setState (State::empty);
        }



        Status getStatus() override
        {
            Status s;
            s.state = state;
            s.cpu = venue.loadMeasurer.getCurrentLoad();
            s.xruns = performer->getXRuns();
            s.sampleRate = currentEndpointProperties.sampleRate;
            s.blockSize = currentEndpointProperties.blockSize;

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

        void setStateChangeCallback (StateChangeCallbackFn f) override
        {
            stateChangeCallback = std::move (f);
        }

        void prepareToPlay (juce::AudioIODevice& device)
        {
            updateEndpointProperties (device);
        }

        void deviceStopped()
        {
            currentEndpointProperties = {};
        }

        void processBlock (const float** inputChannelData, int numInputChannels,
                           float** outputChannelData, int numOutputChannels,
                           const juce::MidiBuffer& midiEvents,
                           uint32_t numSamples)
        {
            if (midiEventQueue != nullptr && ! midiEvents.isEmpty())
            {
                juce::MidiBuffer::Iterator iterator (midiEvents);
                juce::MidiMessage message;
                int samplePosition;

                while (iterator.getNextEvent (message, samplePosition))
                    midiEventQueue->enqueueEvent ((uint32_t) samplePosition, packMIDIMessageIntoInt (message));
            }

            if (auto s = audioDeviceInputStream.get())
                s->setInputBuffer ({ inputChannelData, (uint32_t) numInputChannels, 0, numSamples });

            if (auto s = audioDeviceOutputStream.get())
                s->setOutputBuffer ({ outputChannelData, (uint32_t) numOutputChannels, 0, numSamples });

            performer->advance (numSamples);
        }

        bool connectInputEndpoint (uint32_t audioChannelIndex, bool isMIDI, InputEndpoint& inputEndpoint)
        {
            if (currentEndpointProperties.isValid())
            {
                auto kind = inputEndpoint.getDetails().kind;

                if (isStream (kind))
                {
                    if (isMIDI)
                        return false;

                    audioDeviceInputStream = std::make_unique<AudioDeviceInputStream> (inputEndpoint,
                                                                                       audioChannelIndex,
                                                                                       currentEndpointProperties);
                    return true;
                }

                if (isEvent (kind))
                {
                    if (! isMIDI)
                        return false;

                    midiEventQueue = std::make_unique<MidiEventQueueType> (inputEndpoint, currentEndpointProperties);
                    return true;
                }
            }
            else
            {
                SOUL_ASSERT_FALSE;
            }

            return false;
        }

        bool connectOutputEndpoint (uint32_t audioChannelIndex, bool isMIDI, OutputEndpoint& outputEndpoint)
        {
            if (currentEndpointProperties.isValid())
            {
                auto kind = outputEndpoint.getDetails().kind;

                if (isStream (kind))
                {
                    if (isMIDI)
                        return false;

                    audioDeviceOutputStream = std::make_unique<AudioDeviceOutputStream> (outputEndpoint,
                                                                                         audioChannelIndex,
                                                                                         currentEndpointProperties);
                    return true;
                }
            }
            else
            {
                SOUL_ASSERT_FALSE;
            }

            return false;
        }

        void updateEndpointProperties (juce::AudioIODevice& device)
        {
            currentEndpointProperties = EndpointProperties (device.getCurrentSampleRate(),
                                                            (uint32_t) device.getCurrentBufferSizeSamples());
        }

        static int packMIDIMessageIntoInt (const juce::MidiMessage& message)
        {
            uint32_t m = 0;
            auto length = message.getRawDataSize();
            SOUL_ASSERT (length < 4);
            auto* rawData = message.getRawData();

            m = ((uint32_t) rawData[0]) << 16;

            if (length > 1)
            {
                m |= (((uint32_t) rawData[1]) << 8);

                if (length > 2)
                    m |= (uint32_t) rawData[2];
            }

            return (int) m;
        }

        //==============================================================================
        struct AudioDeviceInputStream
        {
            AudioDeviceInputStream (InputEndpoint& inputToAttachTo,
                                    uint32_t startChannel, EndpointProperties properties)
                : input (inputToAttachTo), startChannelIndex (startChannel)
            {
                auto& details = inputToAttachTo.getDetails();
                auto numDestChannels = (uint32_t) details.sampleType.getVectorSize();

                if (details.sampleType.isFloat64())
                {
                    inputToAttachTo.setStreamSource ([=] (void* dest, uint32_t num) -> uint32_t
                    {
                        if (inputBufferAvailable)
                        {
                            InterleavedChannelSet<double> destChannels { static_cast<double*> (dest),
                                                                         numDestChannels, num, numDestChannels };
                            copyChannelSetToFit (destChannels, inputChannelData.getSlice (inputBufferOffset, num));
                            inputBufferOffset += num;
                            inputBufferAvailable = (inputBufferOffset < inputChannelData.numFrames);

                            return num;
                        }

                        return 0;
                    }, properties);
                }
                else if (details.sampleType.isFloat32())
                {
                    inputToAttachTo.setStreamSource ([=] (void* dest, uint32_t num) -> uint32_t
                    {
                        if (inputBufferAvailable)
                        {
                            InterleavedChannelSet<float> destChannels { static_cast<float*> (dest),
                                                                        numDestChannels, num, numDestChannels };

                            copyChannelSetToFit (destChannels, inputChannelData.getSlice (inputBufferOffset, num));
                            inputBufferOffset += num;
                            inputBufferAvailable = (inputBufferOffset < inputChannelData.numFrames);

                            return num;
                        }

                        return 0;
                    }, properties);
                }
                else if (details.sampleType.isInteger32())
                {
                    inputToAttachTo.setStreamSource ([=] (void* dest, uint32_t num) -> uint32_t
                                                     {
                                                         if (inputBufferAvailable)
                                                         {
                                                             InterleavedChannelSet<int> destChannels { static_cast<int*> (dest), numDestChannels, num, numDestChannels };

                                                             copyChannelSetToFit (destChannels, inputChannelData.getSlice (inputBufferOffset, num));
                                                             inputBufferOffset += num;
                                                             inputBufferAvailable = (inputBufferOffset < inputChannelData.numFrames);

                                                             return num;
                                                         }

                                                         return 0;
                                                     }, properties);
                }
                else
                    SOUL_ASSERT_FALSE;
            }

            ~AudioDeviceInputStream()
            {
                input->removeSource();
            }

            void setInputBuffer (DiscreteChannelSet<const float> newData)
            {
                inputChannelData = newData.getChannelSet (startChannelIndex, newData.numChannels);
                inputBufferAvailable = true;
                inputBufferOffset = 0;
            }

            InputEndpoint::Ptr input;
            uint32_t startChannelIndex;
            bool inputBufferAvailable = false;
            DiscreteChannelSet<const float> inputChannelData;
            uint32_t inputBufferOffset = 0;
        };

        //==============================================================================
        struct AudioDeviceOutputStream
        {
            AudioDeviceOutputStream (OutputEndpoint& outputToAttachTo,
                                     uint32_t startChannel, EndpointProperties properties)
                : output (outputToAttachTo), startChannelIndex (startChannel)
            {
                auto& details = outputToAttachTo.getDetails();
                auto numSrcChannels = (uint32_t) details.sampleType.getVectorSize();

                if (details.sampleType.isFloat64())
                {
                    outputToAttachTo.setStreamSink ([=] (const void* src, uint32_t num) -> uint32_t
                    {
                        if (outputBufferAvailable)
                        {
                            InterleavedChannelSet<const double> srcChannels { static_cast<const double*> (src),
                                                                              numSrcChannels, num, numSrcChannels };
                            copyChannelSetToFit (outputChannelData.getSlice (outputBufferOffset, num), srcChannels);
                            outputBufferOffset += num;
                            outputBufferAvailable = (outputBufferOffset < outputChannelData.numFrames);
                        }

                        return num;

                    }, properties);
                }
                else if (details.sampleType.isFloat32())
                {
                    outputToAttachTo.setStreamSink ([=] (const void* src, uint32_t num) -> uint32_t
                    {
                        if (outputBufferAvailable)
                        {
                            InterleavedChannelSet<const float> srcChannels { static_cast<const float*> (src),
                                                                             numSrcChannels, num, numSrcChannels };
                            copyChannelSetToFit (outputChannelData.getSlice (outputBufferOffset, num), srcChannels);
                            outputBufferOffset += num;
                            outputBufferAvailable = (outputBufferOffset < outputChannelData.numFrames);
                        }

                        return num;
                    }, properties);
                }
                else if (details.sampleType.isInteger32())
                {
                    outputToAttachTo.setStreamSink ([=] (const void* src, uint32_t num) -> uint32_t
                                                    {
                                                        if (outputBufferAvailable)
                                                        {
                                                            InterleavedChannelSet<const int> srcChannels { static_cast<const int*> (src), numSrcChannels, num, numSrcChannels };
                                                            copyChannelSetToFit (outputChannelData.getSlice (outputBufferOffset, num), srcChannels);
                                                            outputBufferOffset += num;
                                                            outputBufferAvailable = (outputBufferOffset < outputChannelData.numFrames);
                                                        }

                                                        return num;
                                                    }, properties);
                }
                else
                    SOUL_ASSERT_FALSE;
            }

            ~AudioDeviceOutputStream()
            {
                output->removeSink();
            }

            void setOutputBuffer (DiscreteChannelSet<float> newData)
            {
                outputBufferOffset = 0;
                outputChannelData = newData.getChannelSet (startChannelIndex, newData.numChannels);
                outputBufferAvailable = true;
            }

            OutputEndpoint::Ptr output;
            uint32_t startChannelIndex;
            bool outputBufferAvailable = false;
            DiscreteChannelSet<float> outputChannelData;
            uint32_t outputBufferOffset = 0;
        };

        using MidiEventQueueType = InputEventQueue<EventFIFO<int32_t, std::atomic<uint64_t>>>;

        AudioPlayerVenue& venue;
        std::unique_ptr<Performer> performer;
        EndpointProperties currentEndpointProperties;
        std::unique_ptr<AudioDeviceInputStream> audioDeviceInputStream;
        std::unique_ptr<AudioDeviceOutputStream> audioDeviceOutputStream;
        std::unique_ptr<MidiEventQueueType> midiEventQueue;
        StateChangeCallbackFn stateChangeCallback;

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

    CPULoadMeasurer loadMeasurer;

    struct EndpointInfo
    {
        EndpointDetails details;
        uint32_t audioChannelIndex = 0;
        bool isMIDI = false;
    };

    std::vector<EndpointInfo> sourceEndpoints, sinkEndpoints;

    std::recursive_mutex activeSessionLock;
    std::vector<AudioPlayerSession*> activeSessions;

    uint64_t totalSamplesProcessed = 0;
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
                                int numSamples) override
    {
        juce::ScopedNoDenormals disableDenormals;
        loadMeasurer.startMeasurement();

        ++audioCallbackCount;

        for (int i = 0; i < numOutputChannels; ++i)
            if (auto* chan = outputChannelData[i])
                juce::FloatVectorOperations::clear (chan, numSamples);

        if (midiCollector)
            midiCollector->removeNextBlockOfMessages (incomingMIDI, numSamples);

        if (totalSamplesProcessed > numWarmUpSamples)
        {
            std::lock_guard<decltype(activeSessionLock)> lock (activeSessionLock);

            for (auto& s : activeSessions)
                s->processBlock (inputChannelData, numInputChannels,
                                 outputChannelData, numOutputChannels,
                                 incomingMIDI,
                                 (uint32_t) numSamples);
        }

        incomingMIDI.clear();

        totalSamplesProcessed += static_cast<uint64_t> (numSamples);
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
                                 "defaultIn",
                                 "defaultIn",
                                 getVectorType (numInputChannels),
                                 0, false);

                if (numOutputChannels > 0)
                    addEndpoint (sinkEndpoints,
                                 EndpointKind::stream,
                                 "defaultOut",
                                 "defaultOut",
                                 getVectorType (numOutputChannels),
                                 0, false);

                addEndpoint (sourceEndpoints,
                             EndpointKind::event,
                             "defaultMidiIn",
                             "defaultMidiIn",
                             soul::PrimitiveType::int32,
                             0, true);

                addEndpoint (sinkEndpoints,
                             EndpointKind::event,
                             "defaultMidiOut",
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
                             uint32_t audioChannelIndex, bool isMIDI)
    {
        EndpointInfo e;

        e.details.endpointID    = std::move (id);
        e.details.name          = std::move (name);
        e.details.kind          = kind;
        e.details.sampleType    = sampleType;
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
