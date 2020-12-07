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

namespace soul::patch
{

/**
    Implementation of the PatchInstance interface.
*/
struct PatchInstanceImpl final  : public RefCountHelper<PatchInstance, PatchInstanceImpl>
{
    PatchInstanceImpl (std::unique_ptr<soul::PerformerFactory> factory, const BuildSettings& settings, VirtualFile::Ptr f)
        : performerFactory (std::move (factory)), buildSettings (settings), manifestFile (std::move (f))
    {
        fileList.initialiseFromManifestFile (manifestFile);
    }

    void refreshFileList()
    {
        fileList.refresh();
        description = fileList.createDescriptionPtr();
    }

    void silentRefreshFileList()
    {
        try
        {
            refreshFileList();
        }
        catch (const PatchLoadError& e)
        {
            description = fileList.createDescriptionWithMessage (e.message);
        }
    }

    VirtualFile* getLocation() override
    {
        return manifestFile.incrementAndGetPointer();
    }

    Description* getDescription() override
    {
        silentRefreshFileList(); // ignore error for now - can report this later on trying to compile

        if (description != nullptr)
            description->addRef();

        return description.get();
    }

    int64_t getLastModificationTime() override
    {
        silentRefreshFileList();
        return fileList.getMostRecentModificationTime();
    }

    PatchPlayer* compileNewPlayer (const PatchPlayerConfiguration& config,
                                   CompilerCache* cache,
                                   SourceFilePreprocessor* preprocessor,
                                   ExternalDataProvider* externalDataProvider) override
    {
        PatchPlayer::Ptr patch;

        try
        {
            refreshFileList();

            auto patchImpl = new PatchPlayerImpl (fileList, config, performerFactory->createPerformer());
            patch = PatchPlayer::Ptr (patchImpl);

            buildSettings.sampleRate = config.sampleRate;
            buildSettings.maxBlockSize = config.maxFramesPerBlock;

            patchImpl->compile (buildSettings, cache, preprocessor, externalDataProvider);
        }
        catch (const PatchLoadError& e)
        {
            auto patchImpl = new PatchPlayerImpl (fileList, config, performerFactory->createPerformer());
            patch = PatchPlayer::Ptr (patchImpl);

            CompilationMessage cm;
            cm.fullMessage = makeString (e.message);
            cm.description = cm.fullMessage;
            cm.isError = true;

            patchImpl->compileMessages.push_back (cm);
            patchImpl->updateCompileMessageStatus();
        }

        return patch.incrementAndGetPointer();
    }

    std::unique_ptr<soul::PerformerFactory> performerFactory;
    BuildSettings buildSettings;
    VirtualFile::Ptr manifestFile;
    FileList fileList;
    Description::Ptr description;
};

} // namespace soul::patch
