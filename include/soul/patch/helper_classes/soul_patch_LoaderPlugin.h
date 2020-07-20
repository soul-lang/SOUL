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

#include "../../soul_patch.h"
#include "soul_patch_AudioProcessor.h"
#include "soul_patch_Utilities.h"
#include "soul_patch_CompilerCacheFolder.h"

namespace soul
{
namespace patch
{

//==============================================================================
/**
    This is a juce::AudioProcessor which can told to dynamically load and run different
    patches. The purpose is that you can build a native (VST/AU/etc) plugin with this
    class which can then load (and hot-reload) any SOUL patch at runtime.
*/
class SOULPatchLoaderPlugin  : public juce::AudioProcessor
{
public:
    SOULPatchLoaderPlugin() = default;

    ~SOULPatchLoaderPlugin() override
    {
        plugin.reset();
        patchInstance = nullptr;
    }

    /** To allow this utility class to be used with either the patch DLL or a static build,
        this virtual method abstracts away the loading of a patch.
    */
    virtual soul::patch::PatchInstance::Ptr createPatchInstance (const std::string& url) = 0;

    /** This allows a sub-class to provide an error message to be shown in the editor if
        it needs to report a problem.
    */
    virtual std::string getErrorMessage() = 0;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override
    {
        if (plugin != nullptr)
            plugin->prepareToPlay (sampleRate, samplesPerBlock);
    }

    void releaseResources() override
    {
        if (plugin != nullptr)
            plugin->releaseResources();
    }

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        return plugin == nullptr || plugin->isBusesLayoutSupported (layouts);
    }

    void processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) override
    {
        if (plugin != nullptr && ! isSuspended())
            return plugin->processBlock (audio, midi);

        audio.clear();
        midi.clear();
    }

    //==============================================================================
    const juce::String getName() const override                 { return "SOUL Patch Loader"; }

    juce::AudioProcessorEditor* createEditor() override         { return new Editor (*this); }
    bool hasEditor() const override                             { return true; }

    bool acceptsMidi() const override                           { return true; }
    bool producesMidi() const override                          { return false; }
    bool supportsMPE() const override                           { return true; }
    bool isMidiEffect() const override                          { return false; }
    double getTailLengthSeconds() const override                { return plugin != nullptr ? plugin->getTailLengthSeconds() : 0.0; }

    //==============================================================================
    int getNumPrograms() override                               { return 1; }
    int getCurrentProgram() override                            { return 0; }
    void setCurrentProgram (int) override                       {}
    const juce::String getProgramName (int) override            { return {}; }
    void changeProgramName (int, const juce::String&) override  {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& data) override
    {
        if (plugin != nullptr)
        {
            state.removeAllChildren (nullptr);
            state.addChild (plugin->getUpdatedState(), 0, nullptr);
        }

        juce::MemoryOutputStream out (data, false);
        state.writeToStream (out);
    }

    void setStateInformation (const void* data, int size) override
    {
        auto s = juce::ValueTree::readFromData (data, (size_t) size);

        if (s.hasType (ids.SOULPatchPlugin))
        {
            state = std::move (s);
            updatePatchState();
        }
    }

    void updatePatchState()
    {
        auto stateID  = state.getProperty (ids.patchID).toString().toStdString();
        auto stateURL = state.getProperty (ids.patchURL).toString().toStdString();

        if (patchInstance != nullptr)
        {
            std::string loadedID, loadedURL;

            if (auto desc = soul::patch::Description::Ptr (patchInstance->getDescription()))
            {
                loadedID = desc->UID;
                loadedURL = desc->URL;
            }

            if (stateID != loadedID || stateURL != loadedURL)
            {
                replaceCurrentPlugin ({});
                patchInstance = nullptr;
            }
        }

        if (patchInstance == nullptr)
            patchInstance = createPatchInstance (stateURL);

        if (patchInstance != nullptr)
        {
            if (auto desc = soul::patch::Description::Ptr (patchInstance->getDescription()))
            {
                if (std::string_view (desc->UID).empty())
                {
                    replaceCurrentPlugin ({});
                }
                else
                {
                    state.setProperty (ids.patchID, desc->UID, nullptr);

                    if (plugin == nullptr)
                    {
                        auto newPlugin = std::make_unique<soul::patch::SOULPatchAudioProcessor> (patchInstance, getCompilerCache());
                        newPlugin->askHostToReinitialise = [this] { this->childChanged(); };

                        if (state.getNumChildren() != 0)
                            newPlugin->applyNewState (state.getChild (0));

                        newPlugin->setBusesLayout (getBusesLayout());
                        newPlugin->prepareToPlay (getSampleRate(), getBlockSize());
                        replaceCurrentPlugin (std::move (newPlugin));
                    }
                    else
                    {
                        if (state.getNumChildren() != 0)
                            plugin->applyNewState (state.getChild (0));
                    }
                }
            }
        }
    }

    void setPatchURL (const std::string& newURL)
    {
        if (newURL != state.getProperty (ids.patchURL).toString().toStdString())
        {
            state = juce::ValueTree (ids.SOULPatchPlugin);
            state.setProperty (ids.patchURL, newURL.c_str(), nullptr);
            updatePatchState();
        }
    }

    void childChanged()
    {
        suspendProcessing (true);

        if (plugin != nullptr)
        {
            plugin->setBusesLayout (getBusesLayout());
            plugin->reinitialise();
            plugin->prepareToPlay (getSampleRate(), getBlockSize());
        }

        updateHostDisplay();
        suspendProcessing (false);

        if (auto ed = dynamic_cast<Editor*> (getActiveEditor()))
            ed->refreshContent();
    }

    void replaceCurrentPlugin (std::unique_ptr<soul::patch::SOULPatchAudioProcessor> newPlugin)
    {
        if (newPlugin.get() != plugin.get())
        {
            if (auto ed = dynamic_cast<Editor*> (getActiveEditor()))
                ed->clearContent();

            suspendProcessing (true);
            std::swap (plugin, newPlugin);
            suspendProcessing (false);

            if (auto ed = dynamic_cast<Editor*> (getActiveEditor()))
                ed->refreshContent();
        }
    }

    //==============================================================================
    struct Editor  : public juce::AudioProcessorEditor,
                     public juce::FileDragAndDropTarget
    {
        Editor (SOULPatchLoaderPlugin& p)  : juce::AudioProcessorEditor (p), owner (p)
        {
            setLookAndFeel (&lookAndFeel);
            refreshContent();
            juce::Font::setDefaultMinimumHorizontalScaleFactor (1.0f);
        }

        ~Editor() override
        {
            owner.editorBeingDeleted (this);
            setLookAndFeel (nullptr);
        }

        void clearContent()
        {
            setDragOver (false);
            pluginEditor.reset();
            setSize (400, 300);
            repaint();
        }

        void refreshContent()
        {
            clearContent();

            if (owner.plugin != nullptr)
                pluginEditor.reset (owner.plugin->createEditor());

            if (pluginEditor != nullptr)
            {
                addAndMakeVisible (pluginEditor.get());
                childBoundsChanged (nullptr);
            }
        }

        void childBoundsChanged (Component*) override
        {
            if (pluginEditor != nullptr)
                setSize (pluginEditor->getWidth(),
                         pluginEditor->getHeight());
        }

        void paint (juce::Graphics& g) override
        {
            auto backgroundColour = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
            g.fillAll (backgroundColour);

            if (pluginEditor == nullptr)
            {
                auto message = owner.getErrorMessage();

                if (message.empty())
                    message = "Drag-and-drop a .soulpatch file here to load it";

                g.setColour (backgroundColour.contrasting());
                g.setFont (juce::Font (19.0f, juce::Font::bold));
                g.drawFittedText (message, getLocalBounds().reduced (20), juce::Justification::centred, 5);
            }
        }

        void paintOverChildren (juce::Graphics& g) override
        {
            if (isDragOver)
                g.fillAll (juce::Colours::lightgreen.withAlpha (0.3f));
        }

        bool isInterestedInFileDrag (const juce::StringArray& files) override  { return files.size() == 1 && files[0].endsWith (soul::patch::getManifestSuffix()); }
        void fileDragEnter (const juce::StringArray&, int, int) override       { setDragOver (true); }
        void fileDragExit (const juce::StringArray&) override                  { setDragOver (false); }

        void filesDropped (const juce::StringArray& files, int, int) override
        {
            setDragOver (false);

            if (files.size() == 1)
                owner.setPatchURL (files[0].toStdString());
        }

        void setDragOver (bool b)
        {
            if (isDragOver != b)
            {
                isDragOver = b;
                repaint();
            }
        }

        SOULPatchLoaderPlugin& owner;
        std::unique_ptr<AudioProcessorEditor> pluginEditor;
        juce::LookAndFeel_V4 lookAndFeel;
        bool isDragOver = false;
    };

private:
    //==============================================================================
    soul::patch::PatchInstance::Ptr patchInstance;
    std::unique_ptr<soul::patch::SOULPatchAudioProcessor> plugin;
    juce::ValueTree state;
    soul::patch::CompilerCache::Ptr compilerCache;

    struct IDs
    {
        const juce::Identifier SOULPatchPlugin   { "SOULPatchPlugin" },
                               patchURL          { "patchURL" },
                               patchID           { "patchID" };
    };

    IDs ids;

    soul::patch::CompilerCache::Ptr getCompilerCache()
    {
        constexpr uint32_t maxNumCacheFiles = 200;

        if (compilerCache == nullptr)
        {
           #if JUCE_MAC
            auto tempFolder = juce::File ("~/Library/Caches");
           #else
            auto tempFolder = juce::File::getSpecialLocation (juce::File::SpecialLocationType::tempDirectory);
           #endif

            auto cacheFolder = tempFolder.getChildFile ("dev.soul.SOULPlugin").getChildFile ("Cache");

            if (cacheFolder.createDirectory())
                compilerCache = soul::patch::CompilerCache::Ptr (new soul::patch::CompilerCacheFolder (cacheFolder, maxNumCacheFiles));
        }

        return compilerCache;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SOULPatchLoaderPlugin)
};


} // namespace patch
} // namespace soul
