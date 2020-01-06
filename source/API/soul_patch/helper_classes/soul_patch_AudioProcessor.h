/*
     _____ _____ _____ __
    |   __|     |  |  |  |
    |__   |  |  |  |  |  |__
    |_____|_____|_____|_____|

    Copyright (c) 2018 - ROLI Ltd.
*/

#pragma once

#ifndef JUCE_AUDIO_PROCESSORS_H_INCLUDED
 #error "this header is designed to be included in JUCE projects that contain the juce_audio_processors module"
#endif

#include "../API/soul_patch.h"

namespace soul
{
namespace patch
{

//==============================================================================
/**
    Wraps up a SOUL patch inside a juce::AudioPluginInstance.

    Just include this in a JUCE project and create an instance from your
    PatchInstance object.

    NOTE: Unlike a normal AudioProcessor, you also need to provide a callback
    function using the askHostToReinitialise parameter - the object will
    use its own background thread to recompile the SOUL code, and will use
    this callback to tell the host when its configuration has changed.
*/
struct SOULPatchAudioProcessor    : public juce::AudioPluginInstance,
                                    private juce::Thread,
                                    private juce::AsyncUpdater
{
    /** Creates a SOULPatchAudioProcessor from a PatchInstance.

        @param patchToLoad          the instance to load - this must not be null
        @param compilerCache        if non-null, this is a user-provided class that can store and reload
                                    cached binaries to avoid re-compiling the same code multiple times
        @param sourcePreprocessor   if non-null, this class is given a chance to pre-parse any source code
                                    before it gets passed to the soul compiler
        @param externalDataProvider if non-null, this allows the user to provide their own custom loader
                                    for external variable data
        @param millisecondsBetweenFileChangeChecks determines how often the class will re-scan the source
                                    files to see whether they've changed and might need to be re-compiled.
                                    Set this to 0 or less to disable checking.
    */
    SOULPatchAudioProcessor (soul::patch::PatchInstance::Ptr patchToLoad,
                             soul::patch::CompilerCache::Ptr compilerCache = {},
                             soul::patch::SourceFilePreprocessor::Ptr sourcePreprocessor = {},
                             soul::patch::ExternalDataProvider::Ptr externalDataProvider = {},
                             soul::patch::DebugMessageHandler::Ptr debugMessageHandler = {},
                             int millisecondsBetweenFileChangeChecks = 1000)
       : juce::Thread ("SOUL Compiler"),
         patch (std::move (patchToLoad)),
         cache (std::move (compilerCache)),
         preprocessor (std::move (sourcePreprocessor)),
         externalData (std::move (externalDataProvider)),
         debugHandler (std::move (debugMessageHandler)),
         millisecsBetweenFileChecks (millisecondsBetweenFileChangeChecks <= 0 ? -1 : millisecondsBetweenFileChangeChecks)
    {
        jassert (patch != nullptr);
        startThread (3);
    }

    ~SOULPatchAudioProcessor() override
    {
        stopThread (100000);
        player = {};
        patch = {};
    }

    //==============================================================================
    soul::patch::PatchInstance& getPatchInstance() const        { return *patch; }

    //==============================================================================
    /** This callback can be set by a host, and the processor will call it if the
        SOUL patch code has changed in a way that means the processor needs to be
        rebuilt.

        When your host gets this callback, it should stop the processor, and once
        no audio or parameter change functions are being called, it should call the
        reinitialise() method, which will cause the processor to refresh its bus layout,
        parameter list and other properties. After calling reinitialise(), the host
        can call prepareToPlay again and start playing the processor again.

        The processor has its own background thread that re-compiles new SOUL patch
        code behind the scenes while the plugin is still running, and once it has a
        new build ready, it triggers a call to this function from the message thread.
    */
    std::function<void()> askHostToReinitialise;

    //==============================================================================
    /** This method should be called by the host when the processor is not being
        run, in response to the askHostToReinitialise function. It causes a refresh
        of the buses, parameters, and other information.
    */
    void reinitialise()
    {
        auto desc = patch->getDescription();

        name = desc.name;
        description = desc.description;
        showMIDIKeyboard = desc.isInstrument;

        if (replacementPlayer != nullptr)
        {
            applyLastStateToPlayer (*replacementPlayer);
            player = std::move (replacementPlayer);
            refreshParameterList();
        }
    }

    /** Returns a string containing all the compile messages and warnings, or an empty string if
        all went well.
    */
    juce::String getCompileError() const
    {
        if (player == nullptr)
            return "No patch loaded";

        juce::StringArray errors;

        for (auto& m : player->getCompileMessages())
            errors.add (m.fullMessage);

        return errors.joinIntoString ("\n");
    }

    /** Returns true if the patch compiled with no errors and can be played */
    bool isPlayable() const
    {
        return player != nullptr && player->isPlayable();
    }

    //==============================================================================
    void fillInPluginDescription (juce::PluginDescription& d) const override
    {
        d = createPluginDescription (*patch);
    }

    static juce::PluginDescription createPluginDescription (soul::patch::PatchInstance& instance)
    {
        juce::PluginDescription d;
        auto desc = instance.getDescription();

        d.name                = desc.name;
        d.descriptiveName     = desc.description;
        d.pluginFormatName    = getPluginFormatName();
        d.category            = desc.category;
        d.manufacturerName    = desc.manufacturer;
        d.version             = desc.version;
        d.fileOrIdentifier    = instance.getLocation()->getAbsolutePath();
        d.lastFileModTime     = juce::Time (instance.getLastModificationTime());
        d.lastInfoUpdateTime  = juce::Time::getCurrentTime();
        d.uid                 = (int) desc.UID.toString<juce::String>().hash();
        d.isInstrument        = desc.isInstrument;

        return d;
    }

    static constexpr const char* getPluginFormatName()  { return "SOUL Patch"; }

    void refreshParameterList() override
    {
        ParameterTreeGroupBuilder treeBuilder;

        if (player != nullptr)
            for (auto& p : player->getParameters())
                if (! getFlagState (*p, "hidden", false))
                    treeBuilder.addParam (std::make_unique<PatchParameter> (p), p->getProperty ("group"));

        setParameterTree (std::move (treeBuilder.tree));
    }

    //==============================================================================
    const juce::String getName() const override
    {
        return name;
    }

    juce::StringArray getAlternateDisplayNames() const override
    {
        juce::StringArray s;
        s.add (name);

        if (description.isNotEmpty())
            s.add (description);

        return s;
    }

    bool isBusesLayoutSupported (const BusesLayout& layout) const override
    {
        if (player == nullptr)
            return true;

        auto inputBuses  = player->getInputBuses();
        auto outputBuses = player->getOutputBuses();

        if (layout.inputBuses.size() != (int) inputBuses.size())
            return false;

        if (layout.outputBuses.size() != (int) outputBuses.size())
            return false;

        for (int i = 0; i < (int) inputBuses.size(); ++i)
            if ((int) inputBuses[i].numChannels != layout.getNumChannels (true, i))
                return false;

        for (int i = 0; i < (int) outputBuses.size(); ++i)
            if ((int) outputBuses[i].numChannels != layout.getNumChannels (false, i))
                return false;

        return true;
    }

    static int countTotalBusChannels (Span<soul::patch::Bus> buses) noexcept
    {
        int total = 0;

        for (auto& b : buses)
            total += static_cast<int> (b.numChannels);

        return total;
    }

    //==============================================================================
    void prepareToPlay (double sampleRate, int maxBlockSize) override
    {
        const juce::ScopedLock sl (configLock);
        currentConfig = { sampleRate, (uint32_t) maxBlockSize };
        messageSpaceIn.resize (1024);
        messageSpaceOut.resize (1024);
        preprocessInputData = nullptr;
        postprocessOutputData = nullptr;
        numPatchInputChannels = 0;
        numPatchOutputChannels = 0;
        setRateAndBufferSizeDetails (sampleRate, maxBlockSize);
        midiKeyboardState.reset();

        if (player != nullptr)
        {
            numPatchInputChannels  = countTotalBusChannels (player->getInputBuses());
            numPatchOutputChannels = countTotalBusChannels (player->getOutputBuses());

            auto pluginBuses = getBusesLayout();

            // We'll do some fairly rough heuristics here to handle simple
            // stereo<->mono conversion situations

            auto monoToStereo = [] (juce::AudioBuffer<float>& b) { b.copyFrom (1, 0, b, 0, 0, b.getNumSamples()); };
            auto stereoToMono = [] (juce::AudioBuffer<float>& b) { b.addFrom  (0, 0, b, 1, 0, b.getNumSamples()); };

            if (numPatchInputChannels == 1 && pluginBuses.getMainInputChannels() == 2)
                preprocessInputData = stereoToMono;

            if (numPatchInputChannels == 2 && pluginBuses.getMainInputChannels() == 1)
                preprocessInputData = monoToStereo;

            if (numPatchOutputChannels == 1 && pluginBuses.getMainOutputChannels() == 2)
                postprocessOutputData = monoToStereo;

            if (numPatchOutputChannels == 2 && pluginBuses.getMainOutputChannels() == 1)
                postprocessOutputData = stereoToMono;
        }
    }

    void releaseResources() override
    {
        reset();
        midiKeyboardState.reset();
    }

    using juce::AudioProcessor::processBlock;

    void processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) override
    {
        auto numFrames = audio.getNumSamples();
        auto numOutputChannels = getTotalNumOutputChannels();
        outputBuffer.setSize (numOutputChannels, numFrames, false, false, true);
        outputBuffer.clear();

        if (! (isSuspended() || player == nullptr))
        {
            if (preprocessInputData != nullptr)
                preprocessInputData (audio);

            soul::patch::PatchPlayer::RenderContext rc;

            rc.inputChannels = audio.getArrayOfReadPointers();
            rc.numInputChannels = (uint32_t) juce::jmin (numPatchInputChannels, audio.getNumChannels());
            rc.outputChannels = outputBuffer.getArrayOfWritePointers();
            rc.numOutputChannels = (uint32_t) juce::jmin (numPatchOutputChannels, outputBuffer.getNumChannels());
            rc.numFrames = (uint32_t) numFrames;
            rc.incomingMIDI = std::addressof (messageSpaceIn[0]);
            rc.numMIDIMessagesIn = 0;
            rc.outgoingMIDI = std::addressof (messageSpaceOut[0]);
            rc.maximumMIDIMessagesOut = (uint32_t) messageSpaceOut.size();
            rc.numMIDIMessagesOut = 0;

            midiKeyboardState.processNextMidiBuffer (midi, 0, numFrames, true);

            if (! midi.isEmpty())
            {
                auto maxEvents = messageSpaceIn.size();

                juce::MidiBuffer::Iterator iter (midi);
                size_t i = 0;

                while (i < maxEvents)
                {
                    const juce::uint8* midiData;
                    int numBytesOfMidiData, samplePosition;

                    if (! iter.getNextEvent (midiData, numBytesOfMidiData, samplePosition))
                        break;

                    if (numBytesOfMidiData < 4)
                    {
                        auto& m = messageSpaceIn[i++];

                        m.frameIndex = (uint32_t) samplePosition;
                        m.byte0 = (uint8_t) midiData[0];
                        m.byte1 = (uint8_t) midiData[1];
                        m.byte2 = (uint8_t) midiData[2];
                    }
                }

                rc.numMIDIMessagesIn = (uint32_t) i;
                midi.clear();
            }

            auto result = player->render (rc);
            juce::ignoreUnused (result);

            if (rc.numMIDIMessagesOut != 0)
            {
                // The numMIDIMessagesOut value could be greater than the buffer size we provided,
                // which lets us know if there was an overflow, but we need to be careful not to
                // copy beyond the end
                auto numMessagesOut = std::min (rc.numMIDIMessagesOut, rc.maximumMIDIMessagesOut);

                for (uint32_t i = 0; i < numMessagesOut; ++i)
                {
                    auto message = messageSpaceOut[i];
                    uint8_t bytes[3] = { message.byte0, message.byte1, message.byte2 };
                    midi.addEvent (bytes, 3, (int) message.frameIndex);
                }
            }
        }

        for (int i = 0; i < outputBuffer.getNumChannels(); ++i)
            audio.copyFrom (i, 0, outputBuffer, i, 0, numFrames);

        if (postprocessOutputData != nullptr)
            postprocessOutputData (audio);
    }

    //==============================================================================
    bool hasEditor() const override     { return true; }

    juce::AudioProcessorEditor* createEditor() override
    {
        if (player != nullptr)
        {
            if (! player->isPlayable())
                return new ErrorDisplayEditor (*this);

            if (getParameters().size() != 0 || showMIDIKeyboard)
                return new ParameterEditor (*this);
        }

        return new EditorBase (*this);
    }

    struct EditorSize
    {
        int width = 0, height = 0;
    };

    EditorSize getStoredEditorSize (const juce::Identifier& property, EditorSize defaultSize)
    {
        auto propertyValue = lastValidState.getChildWithName (ids.EDITORS).getProperty (property);
        auto tokens = juce::StringArray::fromTokens (propertyValue.toString(), " ", {});

        if (tokens.size() == 2)
        {
            auto w = tokens[0].getIntValue();
            auto h = tokens[1].getIntValue();

            if (w > 0 && h > 0)
                return { w, h };
        }

        return defaultSize;
    }

    void storeEditorSize (const juce::Identifier& property, EditorSize newSize)
    {
        if (! lastValidState.isValid())
            lastValidState = juce::ValueTree (ids.SOULPatch);

        auto state = lastValidState.getOrCreateChildWithName (ids.EDITORS, nullptr);

        if (newSize.width > 0 || newSize.height > 0)
            state.setProperty (property, juce::String (newSize.width) + " " + juce::String (newSize.height), nullptr);
        else
            state.removeProperty (property, nullptr);
    }

    //==============================================================================
    int getNumPrograms() override                               { return 1; }
    int getCurrentProgram() override                            { return 0; }
    void setCurrentProgram (int) override                       {}
    const juce::String getProgramName (int) override            { return {}; }
    void changeProgramName (int, const juce::String&) override  {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& data) override
    {
        updateLastState();
        juce::MemoryOutputStream out (data, false);
        lastValidState.writeToStream (out);
    }

    void setStateInformation (const void* data, int size) override
    {
        auto newState = juce::ValueTree::readFromData (data, (size_t) size);

        if (isMatchingStateType (newState))
        {
            lastValidState = std::move (newState);

            if (player != nullptr)
                applyLastStateToPlayer (*player);
        }
    }

    //==============================================================================
    double getTailLengthSeconds() const override            { return 0; }
    bool acceptsMidi() const override                       { return true; }
    bool producesMidi() const override                      { return false; }
    bool supportsMPE() const override                       { return true; }
    bool isMidiEffect() const override                      { return false; }

    //==============================================================================
    void reset() override
    {
        if (player != nullptr)
            player->reset();
    }

    //==============================================================================
    void setNonRealtime (bool /*isNonRealtime*/) noexcept override {}

private:
    soul::patch::PatchInstance::Ptr patch;
    soul::patch::CompilerCache::Ptr cache;
    soul::patch::SourceFilePreprocessor::Ptr preprocessor;
    soul::patch::ExternalDataProvider::Ptr externalData;
    soul::patch::DebugMessageHandler::Ptr debugHandler;

    juce::String name, description;

    soul::patch::PatchPlayer::Ptr player, replacementPlayer;

    juce::CriticalSection configLock;
    soul::patch::PatchPlayerConfiguration currentConfig;

    juce::AudioBuffer<float> outputBuffer;
    std::vector<soul::patch::MIDIMessage> messageSpaceIn, messageSpaceOut;
    int numPatchInputChannels = 0, numPatchOutputChannels = 0;
    std::function<void(juce::AudioBuffer<float>&)> preprocessInputData, postprocessOutputData;
    const int millisecsBetweenFileChecks;

    juce::MidiKeyboardState midiKeyboardState;
    bool showMIDIKeyboard = false;

    juce::ValueTree lastValidState;

    struct IDs
    {
        const juce::Identifier SOULPatch    { "SOULPatch" },
                               id           { "id" },
                               version      { "version" },
                               PARAM        { "PARAM" },
                               value        { "value" },
                               EDITORS      { "EDITORS" };
    };

    IDs ids;

    //==============================================================================
    soul::patch::PatchPlayerConfiguration getConfigCopy() const
    {
        const juce::ScopedLock sl (configLock);
        return currentConfig;
    }

    static bool getFlagState (const soul::patch::Parameter& param, const char* flagName, bool defaultState)
    {
        if (auto flag = param.getProperty (flagName))
        {
            auto s = flag.toString<juce::String>();

            return s.equalsIgnoreCase ("true")
                || s.equalsIgnoreCase ("yes")
                || s.getIntValue() != 0;
        }

        return defaultState;
    }

    juce::File getManifestFile() const
    {
        if (auto manifest = patch->getDescription().manifestFile)
            return juce::File::getCurrentWorkingDirectory().getChildFile (manifest->getAbsolutePath());

        return {};
    }

    //==============================================================================
    void run() override
    {
        while (! threadShouldExit())
        {
            if (replacementPlayer == nullptr)
            {
                auto config = getConfigCopy();

                if (config.sampleRate != 0 && config.maxFramesPerBlock != 0)
                {
                    if (player == nullptr || player->needsRebuilding (currentConfig))
                    {
                        auto newPlayer = patch->compileNewPlayer (currentConfig, cache.get(),
                                                                  preprocessor.get(), externalData.get(),
                                                                  debugHandler.get());

                        if (threadShouldExit())
                            return;

                        replacementPlayer = newPlayer;
                        triggerAsyncUpdate();
                    }
                }
            }

            wait (millisecsBetweenFileChecks);
        }
    }

    void handleAsyncUpdate() override
    {
        if (askHostToReinitialise != nullptr)
            askHostToReinitialise();
    }

    bool isMatchingStateType (const juce::ValueTree& state) const
    {
        return state.hasType (ids.SOULPatch)
                && state[ids.id] == patch->getDescription().UID.toString<juce::String>();
    }

    void updateLastState()
    {
        if (player != nullptr)
        {
            auto desc = patch->getDescription();

            juce::ValueTree state (ids.SOULPatch);
            state.setProperty (ids.id, desc.UID, nullptr);
            state.setProperty (ids.version, desc.version, nullptr);

            auto editorState = lastValidState.getChildWithName (ids.EDITORS);

            if (editorState.isValid())
                state.addChild (editorState.createCopy(), -1, nullptr);

            for (auto& p : player->getParameters())
            {
                juce::ValueTree param (ids.PARAM);
                param.setProperty (ids.id, p->ID, nullptr);
                param.setProperty (ids.value, p->getValue(), nullptr);
                state.addChild (param, -1, nullptr);
            }

            lastValidState = std::move (state);
        }
    }

    void applyLastStateToPlayer (soul::patch::PatchPlayer& playerToApplyTo)
    {
        if (isMatchingStateType (lastValidState))
        {
            for (auto& param : playerToApplyTo.getParameters())
            {
                auto paramState = lastValidState.getChildWithProperty (ids.id, param->ID);

                if (auto* value = paramState.getPropertyPointer (ids.value))
                    param->setValue (*value);
            }
        }
    }

    //==============================================================================
    struct PatchParameter  : public juce::AudioProcessorParameterWithID
    {
        PatchParameter (soul::patch::Parameter::Ptr p)
            : AudioProcessorParameterWithID (p->ID, p->name),
              param (std::move (p)),
              unit (param->unit.toString<juce::String>()),
              textValues (parseTextValues (param->getProperty ("text"))),
              range (param->minValue, param->maxValue, param->step),
              initialValue (param->initialValue),
              numDecimalPlaces (getNumDecimalPlaces (range)),
              isBool (getFlagState (*param, "boolean", false)),
              automatable (getFlagState (*param, "automatable", true))
        {
        }

        const soul::patch::Parameter::Ptr param;
        const juce::String unit;
        const juce::StringArray textValues;
        const juce::NormalisableRange<float> range;
        const float initialValue;
        const int numDecimalPlaces;
        const bool isBool, automatable;

        juce::String getName (int maximumStringLength) const override    { return name.substring (0, maximumStringLength); }
        juce::String getLabel() const override                           { return unit; }
        Category getCategory() const override                            { return genericParameter; }
        bool isDiscrete() const override                                 { return range.interval != 0; }
        bool isBoolean() const override                                  { return isBool; }
        bool isAutomatable() const override                              { return automatable; }
        bool isMetaParameter() const override                            { return false; }
        juce::StringArray getAllValueStrings() const override            { return textValues; }

        float getDefaultValue() const override                           { return convertTo0to1 (initialValue); }
        float getValue() const override                                  { return convertTo0to1 (param->getValue()); }

        void setValue (float newValue) override
        {
            auto fullRange = convertFrom0to1 (newValue);

            if (fullRange != param->getValue())
            {
                param->setValue (fullRange);
                sendValueChangedMessageToListeners (newValue);
            }
        }

        juce::String getText (float v, int length) const override
        {
            juce::String result;

            if (textValues.isEmpty())
                result = juce::String (convertFrom0to1 (v), numDecimalPlaces);
            else if (textValues.size() == 1)
                result = preprocessText (textValues[0].toUTF8(), convertFrom0to1 (v));
            else
                result = textValues[juce::jlimit (0, textValues.size() - 1, juce::roundToInt (v * (textValues.size() - 1.0f)))];

            return length > 0 ? result.substring (0, length) : result;
        }

        float getValueForText (const juce::String& text) const override
        {
            for (int i = 0; i < textValues.size(); ++i)
                if (textValues[i] == text)
                    return i / (textValues.size() - 1.0f);

            return convertTo0to1 (text.upToLastOccurrenceOf (text, false, false).getFloatValue());
        }

        int getNumSteps() const override
        {
            if (! textValues.isEmpty() && std::abs (textValues.size() - (range.end - range.start)) < 0.01f)
                return textValues.size() - 1;

            if (range.interval > 0)
                return static_cast<int> ((range.end - range.start) / range.interval) + 1;

            return AudioProcessor::getDefaultNumParameterSteps();
        }

    private:
        float convertTo0to1 (float v) const    { return range.convertTo0to1 (range.snapToLegalValue (v)); }
        float convertFrom0to1 (float v) const  { return range.snapToLegalValue (range.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, v))); }

        static int getNumDecimalPlaces (juce::NormalisableRange<float> r)
        {
            int places = 7;

            if (r.interval != 0.0f)
            {
                if (juce::approximatelyEqual (std::abs (r.interval - (int) r.interval), 0.0f))
                    return 0;

                auto v = std::abs (juce::roundToInt (r.interval * pow (10, places)));

                while ((v % 10) == 0 && places > 0)
                {
                    --places;
                    v /= 10;
                }
            }

            return places;
        }

        static juce::StringArray parseTextValues (String::Ptr text)
        {
            if (text != nullptr)
                return juce::StringArray::fromTokens (text.toString<juce::String>().unquoted(), "|", {});

            return {};
        }

        static juce::String preprocessText (juce::CharPointer_UTF8 text, float value)
        {
            juce::MemoryOutputStream result;

            while (! text.isEmpty())
            {
                auto c = text.getAndAdvance();

                if (c != '%')
                {
                    result << juce::String::charToString (c);
                    continue;
                }

                auto format = text;
                bool addSignChar = false;

                if (*format == '+')
                {
                    addSignChar = true;
                    ++format;
                }

                bool isPadded = (*format == '0');
                int numDigits = 0;

                while (format.isDigit())
                    numDigits = numDigits * 10 + (format.getAndAdvance() - '0');

                bool isFloat = (*format == 'f');
                bool isInt   = (*format == 'd');

                if (! (isInt || isFloat))
                {
                    result << '%';
                    continue;
                }

                if (addSignChar && value >= 0)
                    result << '+';

                if (isInt)
                {
                    juce::String s ((int64_t) (value + 0.5f));
                    result << (isPadded ? s.paddedLeft ('0', numDigits) : s);
                }
                else if (numDigits <= 0)
                {
                    result << value;
                }
                else if (isPadded)
                {
                    result << juce::String (value, numDigits);
                }
                else
                {
                    juce::String s (value);
                    auto afterDot = s.fromLastOccurrenceOf (".", false, false);

                    if (afterDot.containsOnly ("0123456789"))
                        if (afterDot.length() > numDigits)
                            s = s.dropLastCharacters (afterDot.length() - numDigits);

                    result << s;
                }

                text = ++format;
            }

            return result.toString();
        }
    };

    //==============================================================================
    struct ParameterTreeGroupBuilder
    {
        std::map<juce::String, juce::AudioProcessorParameterGroup*> groups;
        juce::AudioProcessorParameterGroup tree;

        void addParam (std::unique_ptr<PatchParameter> newParam, const String::Ptr& group)
        {
            if (group != nullptr)
                getOrCreateGroup (tree, {}, group).addChild (std::move (newParam));
            else
                tree.addChild (std::move (newParam));
        }

        juce::AudioProcessorParameterGroup& getOrCreateGroup (juce::AudioProcessorParameterGroup& targetTree,
                                                              const juce::String& parentPath,
                                                              const juce::String& subPath)
        {
            auto fullPath = parentPath + "/" + subPath;
            auto& targetGroup = groups[fullPath];

            if (targetGroup != nullptr)
                return *targetGroup;

            auto slash = subPath.indexOfChar ('/');

            if (slash < 0)
            {
                auto newGroup = std::make_unique<juce::AudioProcessorParameterGroup> (fullPath, subPath, "/");
                targetGroup = newGroup.get();
                targetTree.addChild (std::move (newGroup));
                return *targetGroup;
            }

            auto firstPathPart = subPath.substring (0, slash);
            auto& parentGroup = getOrCreateGroup (targetTree, parentPath, firstPathPart);
            return getOrCreateGroup (parentGroup, parentPath + "/" + firstPathPart, subPath.substring (slash + 1));
        }
    };

    //==============================================================================
    struct EditorBase  : public juce::AudioProcessorEditor
    {
        EditorBase (SOULPatchAudioProcessor& p) : juce::AudioProcessorEditor (p), patch (p)
        {
            setLookAndFeel (&lookAndFeel);
            setSize (300, 150);
            setResizeLimits (200, 100, 400, 200);
        }

        ~EditorBase() override
        {
            patch.editorBeingDeleted (this);
            setLookAndFeel (nullptr);
        }

        void paint (juce::Graphics& g) override
        {
            auto background = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
            g.fillAll (background);

            if (getNumChildComponents() == 0)
            {
                g.setColour (background.contrasting());
                g.setFont (16.0f);
                g.drawFittedText (patch.getName(), getLocalBounds().reduced (6), juce::Justification::centred, 2);
            }
        }

        SOULPatchAudioProcessor& patch;
        juce::LookAndFeel_V4 lookAndFeel;
    };

    //==============================================================================
    struct ParameterEditor  : public EditorBase
    {
        ParameterEditor (SOULPatchAudioProcessor& p)
            : EditorBase (p), editor (p),
              midiKeyboard (p.midiKeyboardState, juce::MidiKeyboardComponent::Orientation::horizontalKeyboard)
        {
            addAndMakeVisible (editor);

            if (p.showMIDIKeyboard)
                addAndMakeVisible (midiKeyboard);

            auto size = patch.getStoredEditorSize ("defaultView", { 600, 400 });
            setSize (size.width, size.height);

            setResizeLimits (400, 150, 2000, 2000);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (6);

            if (midiKeyboard.isVisible())
                midiKeyboard.setBounds (r.removeFromBottom (std::min (80, r.getHeight() / 4)));

            editor.setBounds (r);
            patch.storeEditorSize ("defaultView", { getWidth(), getHeight() });
        }

        juce::GenericAudioProcessorEditor editor;
        juce::MidiKeyboardComponent midiKeyboard;
    };

    //==============================================================================
    struct ErrorDisplayEditor  : public EditorBase
    {
        ErrorDisplayEditor (SOULPatchAudioProcessor& p) : EditorBase (p)
        {
            textEditor.setMultiLine (true);
            textEditor.setReadOnly (true);
            textEditor.setColour (juce::TextEditor::backgroundColourId, {});
            textEditor.setColour (juce::TextEditor::outlineColourId, {});
            textEditor.setColour (juce::TextEditor::focusedOutlineColourId, {});
            textEditor.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 15.0f, juce::Font::plain));
            textEditor.setText (getErrorText());
            addAndMakeVisible (textEditor);

            auto patchFolder = patch.getManifestFile().getParentDirectory();
            goToFolderButton.setEnabled (patchFolder.isDirectory());
            goToFolderButton.onClick = [=] { patchFolder.startAsProcess(); };
            addAndMakeVisible (goToFolderButton);

            setSize (700, 300);
            setResizeLimits (400, 150, 1000, 500);
        }

        juce::String getErrorText() const
        {
            juce::String error;
            auto manifestFile = patch.getManifestFile();

            error << "Error compiling SOUL patch:\n\n"
                  << (manifestFile != juce::File() ? manifestFile.getFullPathName() : "<unknown file>") << "\n\n"
                  << patch.getCompileError();

            return error;
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (6);
            goToFolderButton.setBounds (r.removeFromBottom (24).removeFromLeft (200));
            textEditor.setBounds (r);
        }

        juce::TextEditor textEditor;
        juce::TextButton goToFolderButton { "Open folder containing patch" };
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SOULPatchAudioProcessor)
};


} // namespace patch
} // namespace soul
