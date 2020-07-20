/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include <JuceHeader.h>
#include "../../../include/soul/patch/helper_classes/soul_patch_LoaderPlugin.h"


//==============================================================================
/** This subclass of SOULPatchLoaderPlugin just adds functionality to find and load
    the SOUL_PatchLoader DLL as its JIT engine.
*/
struct SOULPatchLoaderPlugin_FromDLL  : public soul::patch::SOULPatchLoaderPlugin
{
    SOULPatchLoaderPlugin_FromDLL()
    {
        library->ensureLibraryLoaded (lookForSOULPatchDLL().toStdString());
    }

    std::string getErrorMessage() override
    {
        if (library->library == nullptr)
            return std::string ("Couldn't find or load ") + soul::patch::SOULPatchLibrary::getLibraryFileName();

        return {};
    }

    soul::patch::PatchInstance::Ptr createPatchInstance (const std::string& url) override
    {
        if (library->library != nullptr)
            return soul::patch::PatchInstance::Ptr (library->library->createPatchFromFileBundle (url.c_str()));

        return {};
    }

    static juce::String lookForSOULPatchDLL()
    {
        auto dllName = soul::patch::SOULPatchLibrary::getLibraryFileName();

        auto pluginDLL = juce::File::getSpecialLocation (juce::File::SpecialLocationType::currentApplicationFile);
        auto pluginSibling = pluginDLL.getSiblingFile (dllName);

        if (pluginSibling.exists())
            return pluginSibling.getFullPathName();

       #if JUCE_MAC
        auto insideBundle = pluginDLL.getChildFile ("Contents/Resources").getChildFile (dllName);

        if (insideBundle.exists())
            return insideBundle.getFullPathName();
       #endif

        auto inAppData = juce::File::getSpecialLocation (juce::File::SpecialLocationType::userApplicationDataDirectory)
                            .getChildFile ("SOUL").getChildFile (dllName);

        if (inAppData.exists())
            return inAppData.getFullPathName();

        return dllName;
    }

    struct SharedPatchLibraryHolder
    {
        SharedPatchLibraryHolder() = default;

        void ensureLibraryLoaded (const std::string& patchLoaderLibraryPath)
        {
            if (library == nullptr)
            {
                library = std::make_unique<soul::patch::SOULPatchLibrary> (patchLoaderLibraryPath.c_str());

                if (library->loadedSuccessfully())
                    loadedPath = patchLoaderLibraryPath;
                else
                    library.reset();
            }
            else
            {
                // This class isn't sophisticated enough to be able to load multiple
                // DLLs from different locations at the same time
                jassert (loadedPath == patchLoaderLibraryPath);
            }
        }

        std::unique_ptr<soul::patch::SOULPatchLibrary> library;
        std::string loadedPath;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SharedPatchLibraryHolder)
    };

    juce::SharedResourcePointer<SharedPatchLibraryHolder> library;
};

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SOULPatchLoaderPlugin_FromDLL();
}
