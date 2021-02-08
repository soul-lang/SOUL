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

#pragma once

/**
    A simple demo component that can load SOUL patches using the API and its helper classes.
    It uses the helper classes to wrap the patch in a juce::AudioPluginInstance,
    which it then plays, displaying a default GUI for it to allow parameter twiddling.
*/
struct PatchLoaderComponent   : public juce::Component,
                                public juce::FileDragAndDropTarget,
                                private juce::Timer
{
    PatchLoaderComponent()
    {
        setSize (800, 600);

        if (juce::RuntimePermissions::isRequired (juce::RuntimePermissions::recordAudio)
              && ! juce::RuntimePermissions::isGranted (juce::RuntimePermissions::recordAudio))
        {
            juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio,
                                               [&] (bool granted) { if (granted) initialise(); });
        }
        else
        {
            initialise();
        }
    }

    ~PatchLoaderComponent()
    {
        player.setProcessor (nullptr);
        pluginEditor.reset();
        currentPlugin.reset();
        deviceManager.closeAudioDevice();
        patchFormat.reset();
    }

    void initialise()
    {
        static constexpr int numInputChannels = 2;
        static constexpr int numOutputChannels = 2;

        deviceManager.initialiseWithDefaultDevices (numInputChannels, numOutputChannels);
        deviceManager.addAudioCallback (&player);
        deviceManager.addMidiInputDeviceCallback ({}, &player);
        startTimer (1000);

        auto patchDLL = lookForSOULPatchDLL();

        if (patchDLL.exists())
        {
            auto reinitialiseCallback = [this] (soul::patch::SOULPatchAudioProcessor& patch) { patchUpdated (patch); };

            patchFormat = std::make_unique<soul::patch::SOULPatchAudioPluginFormat> (patchDLL.getFullPathName(),
                                                                                     reinitialiseCallback);

            if (patchFormat->initialisedSuccessfully())
            {
                message = "Drag-and-drop a .soulpatch file here to load it...";
            }
            else
            {
                message = "Failed to correctly load the patch DLL at " + patchDLL.getFullPathName();
            }
        }
        else
        {
            message = "Can't find the SOUL patch DLL!\n\n"
                      "You'll need to put the DLL (or a symlink) next to this executable so that it can be loaded. "
                      "(Or hard-code some app logic to make sure it gets loaded from wherever you want to keep it).";
        }

        repaint();
    }

    void timerCallback() override
    {
        auto newMidiDevices = juce::MidiInput::getAvailableDevices();

        if (newMidiDevices != lastMidiDevices)
        {
            for (auto& oldDevice : lastMidiDevices)
                if (! newMidiDevices.contains (oldDevice))
                    deviceManager.setMidiInputDeviceEnabled (oldDevice.identifier, false);

            for (auto& newDevice : newMidiDevices)
                if (! lastMidiDevices.contains (newDevice))
                    deviceManager.setMidiInputDeviceEnabled (newDevice.identifier, true);

            lastMidiDevices = newMidiDevices;
        }
    }

    bool isInterestedInFileDrag (const juce::StringArray&) override        { return true; }
    void filesDropped (const juce::StringArray& files, int, int) override  { load (files[0]); }

    void load (const juce::File& soulPatchFile)
    {
        if (patchFormat != nullptr)
        {
            if (auto device = deviceManager.getCurrentAudioDevice())
            {
                juce::PluginDescription desc;
                desc.pluginFormatName = soul::patch::SOULPatchAudioProcessor::getPluginFormatName();
                desc.fileOrIdentifier = soulPatchFile.getFullPathName();

                patchFormat->createPluginInstance (desc,
                                                   device->getCurrentSampleRate(),
                                                   device->getCurrentBufferSizeSamples(),
                                                   [this] (std::unique_ptr<juce::AudioPluginInstance> newPlugin,
                                                           const juce::String& error)
                                                   {
                                                       load (std::move (newPlugin), error);
                                                   });
            }
        }
    }

    void load (std::unique_ptr<juce::AudioPluginInstance> newPlugin, const juce::String& error)
    {
        player.setProcessor (nullptr);
        pluginEditor.reset();
        currentPlugin = std::move (newPlugin);
        player.setProcessor (currentPlugin.get());
        message = error;
        repaint();
    }

    /** This callback gets triggered whenever the patch has changed and the host needs to update
        its config - this will be called asynchronously after the patch is first instantiated when
        it finishes compiling and JITting it, and then subsequently if the files are changed on disk.
    */
    void patchUpdated (soul::patch::SOULPatchAudioProcessor& patch)
    {
        // For the sake of simplicity we'll just stop the player, get rid of the old GUI,
        // and reinitialise the processor, then we re-start it and create a new GUI
        player.setProcessor (nullptr);
        pluginEditor.reset();
        patch.reinitialise();
        player.setProcessor (currentPlugin.get());

        message = patch.getName().isNotEmpty() ? ("Loaded: " + patch.getName()) : juce::String();
        repaint();

        pluginEditor = std::unique_ptr<juce::AudioProcessorEditor> (patch.createEditorIfNeeded());
        addAndMakeVisible (pluginEditor.get());
        resized();
    }

    //==============================================================================
    void paint (juce::Graphics& g) override
    {
        auto background = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);

        g.fillAll (background);

        g.setColour (background.contrasting());
        g.setFont (16.0f);
        g.drawFittedText (message, getLocalBounds().reduced (10), juce::Justification::topLeft, 20);
    }

    void resized() override
    {
        if (pluginEditor != nullptr)
            pluginEditor->setBounds (getLocalBounds().withTrimmedTop (35));
    }

    //==============================================================================
    static juce::File lookForSOULPatchDLL()
    {
        auto possibleLocations =
        {
            juce::File::getSpecialLocation (juce::File::currentApplicationFile).getParentDirectory(),
            juce::File::getSpecialLocation (juce::File::userHomeDirectory),
            juce::File::getSpecialLocation (juce::File::userDesktopDirectory),
            juce::File::getSpecialLocation (juce::File::SpecialLocationType::userApplicationDataDirectory).getChildFile ("SOUL")
        };

        for (auto& location : possibleLocations)
        {
            auto f = location.getChildFile (soul::patch::SOULPatchLibrary::getLibraryFileName());

            if (f.exists())
                return f;
        }

        return {};
    }

    juce::AudioDeviceManager deviceManager;
    juce::AudioProcessorPlayer player;
    juce::Array<juce::MidiDeviceInfo> lastMidiDevices;
    std::unique_ptr<soul::patch::SOULPatchAudioPluginFormat> patchFormat;
    std::unique_ptr<juce::AudioPluginInstance> currentPlugin;
    std::unique_ptr<juce::AudioProcessorEditor> pluginEditor;
    juce::String message;
};
