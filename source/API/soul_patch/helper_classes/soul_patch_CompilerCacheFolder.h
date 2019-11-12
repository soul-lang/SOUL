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

#include "../API/soul_patch.h"

namespace soul
{
namespace patch
{

//==============================================================================
/**
    Implements a simple CompilerCache that stores the cached object code chunks
    as files in a folder.
*/
struct CompilerCacheFolder   : public CompilerCache
{
    /** Creates a cache in the given folder (which must exist!) */
    CompilerCacheFolder (juce::File cacheFolder, uint32_t maxNumFilesToCache)
       : folder (std::move (cacheFolder)), maxNumFiles (maxNumFilesToCache)
    {
        purgeOldestFiles (maxNumFiles);
    }

    ~CompilerCacheFolder() override = default;

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

        for (juce::DirectoryIterator i (folder, false, getFilePrefix() + "*", juce::File::findFiles);
             i.next ({}, {}, {}, &modificationTime, {}, {});)
        {
            files.push_back ({ i.getFile(), modificationTime });
        }

        if (files.size() > maxNumFilesToRetain)
        {
            std::sort (files.begin(), files.end());

            for (size_t i = 0; i < files.size() - maxNumFilesToRetain; ++i)
                if (! files[i].file.deleteFile())
                    anyFailed = true;
        }

        return ! anyFailed;
    }

    static juce::String getFilePrefix()                      { return "soul_patch_cache_"; }
    static juce::String getFileName (const char* cacheKey)   { return getFilePrefix() + cacheKey; }
    juce::File getFileForKey (const char* cacheKey) const    { return folder.getChildFile (getFileName (cacheKey)); }

    void addRef() noexcept override         { ++refCount; }
    void release() noexcept override        { if (--refCount == 0) delete this; }

private:
    std::atomic<int> refCount { 0 };
    juce::File folder;
    uint32_t maxNumFiles;
    juce::CriticalSection lock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompilerCacheFolder)
};


} // namespace patch
} // namespace soul
