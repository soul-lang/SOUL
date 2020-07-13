/*
     _____ _____ _____ __
    |   __|     |  |  |  |
    |__   |  |  |  |  |  |__
    |_____|_____|_____|_____|

    Copyright (c) 2018 - ROLI Ltd.
*/

#pragma once

#ifndef JUCE_CORE_H_INCLUDED
 #error "this header is designed to be included in JUCE projects that contain the juce_core module"
#endif

#include "../../soul_patch.h"

#if __clang__
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#endif

namespace soul
{
namespace patch
{

//==============================================================================
/**
    Implements a simple CompilerCache that stores the cached object code chunks
    as files in a folder.
*/
struct CompilerCacheFolder final  : public CompilerCache
{
    /** Creates a cache in the given folder (which must exist!) */
    CompilerCacheFolder (juce::File cacheFolder, uint32_t maxNumFilesToCache)
       : folder (std::move (cacheFolder)), maxNumFiles (maxNumFilesToCache)
    {
        purgeOldestFiles (maxNumFiles);
    }

    ~CompilerCacheFolder() = default;

    void storeItemInCache (const char* key, const void* sourceData, uint64_t size) override
    {
        juce::ScopedLock sl (lock);
        auto file = getFileForKey (key);
        file.replaceWithData (sourceData, (size_t) size);
        purgeOldestFiles (maxNumFiles);
    }

    uint64_t readItemFromCache (const char* key, void* destAddress, uint64_t destSize) override
    {
        juce::ScopedLock sl (lock);
        auto file = getFileForKey (key);
        auto fileSize = (uint64_t) file.getSize();

        if (fileSize == 0)
            return 0;

        if (destAddress == nullptr || destSize < fileSize)
            return fileSize;

        auto readEntireFile = [&]() -> bool
        {
            juce::FileInputStream fin (file);
            return fin.openedOk() && fin.read (destAddress, (int) fileSize) == (int) fileSize;
        };

        if (! readEntireFile())
            return 0;

        file.setLastModificationTime (juce::Time::getCurrentTime());
        return fileSize;
    }

    bool purgeOldestFiles (uint32_t maxNumFilesToRetain) const
    {
        juce::ScopedLock sl (lock);

        struct FileAndDate
        {
            juce::File file;
            juce::Time modificationTime;

            bool operator< (const FileAndDate& other) const noexcept     { return modificationTime < other.modificationTime; }
        };

        std::vector<FileAndDate> files;
        juce::Time modificationTime;
        bool anyFailed = false;

        for (auto i : juce::RangedDirectoryIterator (folder, false, getFilePrefix() + "*", juce::File::findFiles))
            files.push_back ({ i.getFile(), modificationTime });

        if (files.size() > maxNumFilesToRetain)
        {
            std::sort (files.begin(), files.end());

            for (size_t i = 0; i < files.size() - maxNumFilesToRetain; ++i)
                if (! files[i].file.deleteFile())
                    anyFailed = true;
        }

        return ! anyFailed;
    }

    static std::string getFilePrefix()                       { return "soul_patch_cache_"; }
    static std::string getFileName (const char* cacheKey)    { return getFilePrefix() + cacheKey; }
    juce::File getFileForKey (const char* cacheKey) const    { return folder.getChildFile (getFileName (cacheKey)); }

    int addRef() noexcept override   { return ++refCount; }
    int release() noexcept override  { auto newCount = --refCount; if (newCount == 0) delete this; return newCount; }

private:
    std::atomic<int> refCount { 1 };
    juce::File folder;
    uint32_t maxNumFiles;
    juce::CriticalSection lock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompilerCacheFolder)
};


} // namespace patch
} // namespace soul

#if __clang__
 #pragma clang diagnostic pop
#endif
