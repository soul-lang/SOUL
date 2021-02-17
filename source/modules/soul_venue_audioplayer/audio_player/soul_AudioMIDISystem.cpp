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
struct MIDIInputCollector::Pimpl  : private juce::Timer,
                                    private juce::MidiInputCallback
{
    Pimpl (Requirements::PrintLogMessageFn l) : log (std::move (l))
    {
        constexpr uint32_t midiFIFOSize = 1024;
        inputMIDIBuffer.reserve (midiFIFOSize);
        midiFIFO.reset (midiFIFOSize);
        startTimer (2000);
    }

    ~Pimpl() override
    {
        midiInputs.clear();
    }

    using MIDIClock = std::chrono::high_resolution_clock;

    struct IncomingMIDIEvent
    {
        MIDIClock::time_point time;
        choc::midi::ShortMessage message;
    };

    Requirements::PrintLogMessageFn log;
    juce::StringArray lastMidiDevices;
    std::vector<std::unique_ptr<juce::MidiInput>> midiInputs;
    choc::fifo::SingleReaderSingleWriterFIFO<IncomingMIDIEvent> midiFIFO;
    std::vector<MIDIEvent> inputMIDIBuffer;
    MIDIClock::time_point lastMIDIBlockTime;

    void timerCallback() override
    {
        scanForDevices();
    }

    void scanForDevices()
    {
        auto devices = juce::MidiInput::getDevices();

        if (lastMidiDevices != devices)
        {
            lastMidiDevices = devices;

            for (auto& mi : midiInputs)
            {
                if (log != nullptr)
                    log (("Closing MIDI device: " + mi->getName()).toStdString());

                mi.reset();
            }

            midiInputs.clear();

            for (int i = devices.size(); --i >= 0;)
                if (auto mi = juce::MidiInput::openDevice (i, this))
                    midiInputs.emplace_back (std::move (mi));

            for (auto& mi : midiInputs)
            {
                if (log != nullptr)
                    log (("Opening MIDI device: " + mi->getName()).toStdString());

                mi->start();
            }
        }
    }

    void clear()
    {
        midiFIFO.reset();
        inputMIDIBuffer.clear();
    }

    MIDIEventInputList getNextBlock (double sampleRate, uint32_t numFrames)
    {
       #if ! JUCE_BELA
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
       #endif

        return { inputMIDIBuffer.data(), inputMIDIBuffer.data() + inputMIDIBuffer.size() };
    }

    void splitIntoShortMessages (const juce::MidiMessage& message, std::function<void(const choc::midi::ShortMessage&)> publish)
    {
        auto bytes = message.getRawData();
        auto length = message.getRawDataSize();

        for (int i = 0; i < length; i += 3)
        {
            auto availableBytes = length - i;

            auto m = choc::midi::ShortMessage (bytes[i],
                                               availableBytes > 1 ? bytes[i + 1] : 0,
                                               availableBytes > 2 ? bytes[i + 2] : 0);

            publish (m);
        }
    }

    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message) override
    {
        splitIntoShortMessages (message, [&] (const choc::midi::ShortMessage& m)
                                {
                                   #if JUCE_BELA
                                    inputMIDIBuffer.push_back ({ 0, m });
                                   #else
                                    midiFIFO.push ({ MIDIClock::now(), m });
                                   #endif
                                });
    }
};

MIDIInputCollector::MIDIInputCollector (Requirements::PrintLogMessageFn l) : pimpl (std::make_unique<Pimpl> (std::move (l))) {}
MIDIInputCollector::~MIDIInputCollector() = default;

void MIDIInputCollector::clearFIFO()    { pimpl->clear(); }

MIDIEventInputList MIDIInputCollector::getNextBlock (double sampleRate, uint32_t numFrames)
{
    return pimpl->getNextBlock (sampleRate, numFrames);
}


//==============================================================================
struct AudioMIDISystem::Pimpl  : private juce::AudioIODeviceCallback,
                                 private juce::Timer
{
    Pimpl (Requirements r) : requirements (std::move (r))
    {
        if (requirements.sampleRate < 1000.0 || requirements.sampleRate > 48000.0 * 8)
            requirements.sampleRate = 0;

        if (requirements.blockSize < 1 || requirements.blockSize > 2048)
            requirements.blockSize = 0;

        midiInputCollector = std::make_unique<MIDIInputCollector> (requirements.printLogMessage);
        openAudioDevice();
        startTimerHz (2);
    }

    ~Pimpl() override
    {
        audioDevice.reset();
        midiInputCollector.reset();
    }

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

    //==============================================================================
    Requirements requirements;

    std::unique_ptr<juce::AudioIODevice> audioDevice;
    uint64_t totalFramesProcessed = 0;
    std::atomic<uint32_t> audioCallbackCount { 0 };
    uint32_t lastCallbackCount = 0;
    static constexpr uint64_t numWarmUpFrames = 15000;
    double sampleRate = 0;
    uint32_t blockSize = 0;

    std::chrono::system_clock::time_point lastKnownActiveCallbackTime;

    std::unique_ptr<MIDIInputCollector> midiInputCollector;

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
        midiInputCollector->clearFIFO();

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

        auto midiInput = midiInputCollector->getNextBlock (sampleRate, static_cast<uint32_t> (numFrames));

        if (totalFramesProcessed > numWarmUpFrames)
        {
            std::lock_guard<decltype(callbackLock)> lock (callbackLock);

            if (callback != nullptr)
                callback->render (choc::buffer::createChannelArrayView (inputChannelData,  (uint32_t) numInputChannels,  (uint32_t) numFrames),
                                  choc::buffer::createChannelArrayView (outputChannelData, (uint32_t) numOutputChannels, (uint32_t) numFrames),
                                  midiInput);
        }

        totalFramesProcessed += static_cast<uint64_t> (numFrames);
        loadMeasurer.stopMeasurement();

       #if JUCE_BELA
        midiInputCollector->clearFIFO();
       #endif
    }

    void timerCallback() override
    {
        checkForStalledProcessor();
    }

    void checkForStalledProcessor()
    {
        auto now = std::chrono::system_clock::now();

        if (lastCallbackCount != audioCallbackCount)
        {
            lastCallbackCount = audioCallbackCount;
            lastKnownActiveCallbackTime = now;
            return;
        }

        if (lastCallbackCount != 0 && now > lastKnownActiveCallbackTime + std::chrono::milliseconds (2000))
        {
           #if ! SOUL_DEBUG
            log ("Fatal error! run() function took too long to execute.\n"
                 "Process terminating...");

            std::terminate();
           #endif
        }
    }

    void openAudioDevice()
    {
        tryToCreateDeviceType ([] { return juce::AudioIODeviceType::createAudioIODeviceType_CoreAudio(); });
        tryToCreateDeviceType ([] { return juce::AudioIODeviceType::createAudioIODeviceType_iOSAudio(); });
        tryToCreateDeviceType ([] { return juce::AudioIODeviceType::createAudioIODeviceType_ASIO(); });
        tryToCreateDeviceType ([] { return juce::AudioIODeviceType::createAudioIODeviceType_WASAPI (juce::WASAPIDeviceMode::sharedLowLatency); });
        tryToCreateDeviceType ([] { return juce::AudioIODeviceType::createAudioIODeviceType_WASAPI (juce::WASAPIDeviceMode::shared); });
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
                log (soul::utilities::getAudioDeviceDescription (*audioDevice));
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
};

//==============================================================================
AudioMIDISystem::AudioMIDISystem (Requirements r)  : pimpl (std::make_unique<Pimpl> (std::move (r))) {}
AudioMIDISystem::~AudioMIDISystem() = default;

void AudioMIDISystem::setCallback (Callback* c)             { pimpl->setCallback (c); }

double AudioMIDISystem::getSampleRate() const               { return pimpl->sampleRate; }
uint32_t AudioMIDISystem::getMaxBlockSize() const           { return pimpl->blockSize; }

float AudioMIDISystem::getCPULoad() const                   { return pimpl->loadMeasurer.getCurrentLoad(); }
int AudioMIDISystem::getXRunCount() const                   { return pimpl->audioDevice != nullptr ? pimpl->audioDevice->getXRunCount() : -1; }

int AudioMIDISystem::getNumInputChannels() const            { return pimpl->audioDevice != nullptr ? pimpl->audioDevice->getActiveInputChannels().countNumberOfSetBits() : 0; }
int AudioMIDISystem::getNumOutputChannels() const           { return pimpl->audioDevice != nullptr ? pimpl->audioDevice->getActiveOutputChannels().countNumberOfSetBits() : 0; }

}
