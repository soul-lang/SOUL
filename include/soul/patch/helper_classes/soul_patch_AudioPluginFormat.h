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
#include "soul_patch_Utilities.h"
#include "soul_patch_AudioProcessor.h"

namespace soul
{
namespace patch
{

//==============================================================================
/**
    Provides a juce::AudioPluginFormat which can scan and load SOUL patches.
*/
class SOULPatchAudioPluginFormat  : public juce::AudioPluginFormat
{
public:
    /** Creates the format object.
        @param patchLoaderLibraryPath   a full path to the .dll or .so file containing the patch library
        @param reinitialiseProcessor    a lambda which can re-initialise a SOULPatchAudioProcessor when
                                        its code or dependencies are modified
        @param compilerCache            an optional cache object which the compiler can use to avoid
                                        expensive recompilations of the same source code
        @param sourcePreprocessor       an optional pre-processor stage that can be used to extend the
                                        SOUL source syntax
    */
    SOULPatchAudioPluginFormat (juce::String patchLoaderLibraryPath,
                                std::function<void(SOULPatchAudioProcessor&)> reinitialiseProcessor,
                                CompilerCache::Ptr compilerCache = {},
                                SourceFilePreprocessor::Ptr sourcePreprocessor = {},
                                SOULPatchAudioProcessor::CreatePatchGUIEditorFn createCustomGUI = {})
       : reinitialiseCallback (std::move (reinitialiseProcessor)),
         cache (std::move (compilerCache)),
         preprocessor (std::move (sourcePreprocessor)),
         createCustomGUIFn (std::move (createCustomGUI))
    {
        // a callback must be supplied, because the processors aren't in a useable state when first
        // created - a host will have to wait for this callback before actually using them
        jassert (reinitialiseCallback != nullptr);

        library->ensureLibraryLoaded (patchLoaderLibraryPath);
    }

    ~SOULPatchAudioPluginFormat() override = default;

    /** Checks whether the library loaded correctly. */
    bool initialisedSuccessfully() const
    {
        return library->library != nullptr;
    }

    static juce::String getFormatName()         { return SOULPatchAudioProcessor::getPluginFormatName(); }
    juce::String getName() const override       { return getFormatName(); }

    void createPluginInstance (const juce::PluginDescription& desc,
                               double /*initialSampleRate*/, int /*initialBufferSize*/,
                               PluginCreationCallback callback) override
    {
        std::unique_ptr<SOULPatchAudioProcessor> result;

        if (auto instance = library->createInstance (desc))
        {
            auto p = std::make_unique<SOULPatchAudioProcessor> (instance, cache, preprocessor, nullptr);

            auto* rawPatchPointer = p.get();
            auto reinitCallback = reinitialiseCallback;
            p->askHostToReinitialise = [reinitCallback, rawPatchPointer] { reinitCallback (*rawPatchPointer); };

            p->createCustomGUI = createCustomGUIFn;

            p->reinitialise();
            callback (std::move (p), {});
            return;
        }

        callback ({}, "Unable to load SOUL patch file");
    }

    void findAllTypesForFile (juce::OwnedArray<juce::PluginDescription>& results,
                              const juce::String& fileOrIdentifier) override
    {
        if (auto instance = library->createInstance (fileOrIdentifier))
            results.add (new juce::PluginDescription (SOULPatchAudioProcessor::createPluginDescription (*instance)));
    }

    bool fileMightContainThisPluginType (const juce::String& fileOrIdentifier) override
    {
        if (juce::File::createFileWithoutCheckingPath (fileOrIdentifier).hasFileExtension (".soulpatch"))
            return true;

        auto file = juce::File::getCurrentWorkingDirectory().getChildFile (fileOrIdentifier);

        if (file.isDirectory())
            return file.getNumberOfChildFiles (juce::File::findFiles, "*.soulpatch") != 0;

        return false;
    }

    juce::String getNameOfPluginFromIdentifier (const juce::String& fileOrIdentifier) override
    {
        if (auto instance = library->createInstance (fileOrIdentifier))
            return Description::Ptr (instance->getDescription())->name;

        return fileOrIdentifier;
    }

    bool pluginNeedsRescanning (const juce::PluginDescription& description) override
    {
        if (auto instance = library->createInstance (description))
            return juce::Time (instance->getLastModificationTime()) != description.lastFileModTime;

        return false;
    }

    juce::StringArray searchPathsForPlugins (const juce::FileSearchPath& directoriesToSearch, bool recursive, bool) override
    {
        juce::StringArray results;

        for (int j = 0; j < directoriesToSearch.getNumPaths(); ++j)
            recursivePatchSearch (results, directoriesToSearch[j], recursive);

        return results;
    }

    bool doesPluginStillExist (const juce::PluginDescription& desc) override
    {
        if (juce::File::createFileWithoutCheckingPath (desc.fileOrIdentifier).hasFileExtension (".soulpatch"))
            return juce::File::createFileWithoutCheckingPath (desc.fileOrIdentifier).exists();

        return fileMightContainThisPluginType (desc.fileOrIdentifier);
    }

    juce::FileSearchPath getDefaultLocationsToSearch() override
    {
        juce::FileSearchPath path;

       #if JUCE_WINDOWS
        path.add (juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory)
                    .getChildFile ("Common Files\\SOULPatches"));
       #elif JUCE_MAC
        path.add (juce::File ("/Library/Audio/Plug-Ins/SOULPatches"));
        path.add (juce::File ("~/Library/Audio/Plug-Ins/SOULPatches"));
       #endif

        return path;
    }

    bool canScanForPlugins() const override     { return true; }
    bool isTrivialToScan() const override       { return true; }
    bool requiresUnblockedMessageThreadDuringCreation (const juce::PluginDescription&) const override  { return false; }

private:
    std::function<void(SOULPatchAudioProcessor&)> reinitialiseCallback;
    CompilerCache::Ptr cache;
    SourceFilePreprocessor::Ptr preprocessor;
    SOULPatchAudioProcessor::CreatePatchGUIEditorFn createCustomGUIFn;

    static void recursivePatchSearch (juce::StringArray& results, const juce::File& dir, bool recursive)
    {
        for (auto i : juce::RangedDirectoryIterator (dir, false, "*", juce::File::findFilesAndDirectories))
        {
            auto f = i.getFile();

            if (f.hasFileExtension (".soulpatch"))
            {
                if (! f.isDirectory())
                    results.add (f.getFullPathName());
            }
            else
            {
                if (recursive && f.isDirectory())
                    recursivePatchSearch (results, f, true);
            }
        }
    }

    //==============================================================================
    struct LibraryHolder
    {
        LibraryHolder() = default;

        void ensureLibraryLoaded (const juce::String& patchLoaderLibraryPath)
        {
            if (library == nullptr)
            {
                library = std::make_unique<SOULPatchLibrary> (patchLoaderLibraryPath.toRawUTF8());

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

        std::unique_ptr<SOULPatchLibrary> library;
        juce::String loadedPath;

        PatchInstance::Ptr createInstance (const juce::String& fileOrIdentifier)
        {
            return library->createPatchFromFileBundle (juce::File::getCurrentWorkingDirectory()
                                                         .getChildFile (fileOrIdentifier)
                                                         .getFullPathName().toRawUTF8());
        }

        PatchInstance::Ptr createInstance (const juce::PluginDescription& desc)
        {
            if (desc.pluginFormatName == SOULPatchAudioProcessor::getPluginFormatName())
                return createInstance (desc.fileOrIdentifier);

            return {};
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LibraryHolder)
    };

    juce::SharedResourcePointer<LibraryHolder> library;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SOULPatchAudioPluginFormat)
};


} // namespace patch
} // namespace soul
