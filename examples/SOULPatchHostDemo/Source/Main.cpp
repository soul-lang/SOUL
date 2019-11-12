/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic startup code for a JUCE application.

  ==============================================================================
*/

#define DONT_SET_USING_JUCE_NAMESPACE 1
#include "../JuceLibraryCode/JuceHeader.h"

#include "../../../source/API/soul_patch/API/soul_patch.h"
#include "../../../source/API/soul_patch/helper_classes/soul_patch_AudioPluginFormat.h"
#include "../../../source/API/soul_patch/helper_classes/soul_patch_Utilities.h"
#include "../../../source/API/soul_patch/helper_classes/soul_patch_CompilerCacheFolder.h"

#include "PatchLoaderComponent.h"

//==============================================================================
struct SOULPatchHostDemoApp  : public juce::JUCEApplication
{
    SOULPatchHostDemoApp() {}

    const juce::String getApplicationName() override       { return ProjectInfo::projectName; }
    const juce::String getApplicationVersion() override    { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise (const juce::String& commandLine) override  { mainWindow = std::make_unique<MainWindow>(); }
    void shutdown() override                                    { mainWindow.reset(); }
    void systemRequestedQuit() override                         { quit(); }
    void anotherInstanceStarted (const juce::String&) override  {}

    //==============================================================================
    struct MainWindow  : public juce::DocumentWindow
    {
        MainWindow()  : juce::DocumentWindow ("SOUL Patch Demo Host",
                                              juce::Desktop::getInstance().getDefaultLookAndFeel()
                                                 .findColour (ResizableWindow::backgroundColourId),
                                              juce::DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new PatchLoaderComponent(), true);

           #if JUCE_IOS || JUCE_ANDROID
            setFullScreen (true);
           #else
            setResizable (true, true);
            centreWithSize (getWidth(), getHeight());
           #endif

            setVisible (true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
START_JUCE_APPLICATION (SOULPatchHostDemoApp)
