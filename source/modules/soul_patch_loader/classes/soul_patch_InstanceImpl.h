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

/// Implementation of the PatchInstance interface.
struct PatchInstanceImpl final  : public RefCountHelper<PatchInstance, PatchInstanceImpl>
{
    PatchInstanceImpl (std::unique_ptr<soul::PerformerFactory> factory, const BuildSettings& settings, VirtualFile::Ptr f)
        : defaultPerformerFactory (std::move (factory)), buildSettings (settings), manifestFile (std::move (f))
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
            auto message = formatErrorMessage ("error", e.description, e.filename, e.line, e.column);
            description = fileList.createDescriptionWithMessage (message);
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

    PatchPlayer::Ptr compilePlayer (soul::PerformerFactory& performerFactory,
                                    const PatchPlayerConfiguration& config,
                                    CompilerCache* cache,
                                    SourceFilePreprocessor* preprocessor,
                                    ExternalDataProvider* externalDataProvider)
    {
        PatchPlayer::Ptr patch;

        try
        {
            refreshFileList();

            auto patchImpl = new PatchPlayerImpl (fileList, config, performerFactory.createPerformer());
            patch = PatchPlayer::Ptr (patchImpl);

            buildSettings.sampleRate = config.sampleRate;
            buildSettings.maxBlockSize = config.maxFramesPerBlock;

            patchImpl->compile (buildSettings, cache, preprocessor, externalDataProvider);
        }
        catch (const PatchLoadError& e)
        {
            auto patchImpl = new PatchPlayerImpl (fileList, config, performerFactory.createPerformer());
            patch = PatchPlayer::Ptr (patchImpl);

            CompilationMessage cm;
            cm.severity = makeString (std::string ("error"));
            cm.description = makeString (e.description);
            cm.filename = makeString (e.filename);
            cm.line = e.line;
            cm.column = e.column;
            cm.isError = true;

            patchImpl->compileMessages.push_back (cm);
            patchImpl->updateCompileMessageStatus();
        }

        return patch;
    }

    PatchPlayer* compileNewPlayer (const PatchPlayerConfiguration& config,
                                   CompilerCache* cache,
                                   SourceFilePreprocessor* preprocessor,
                                   ExternalDataProvider* externalDataProvider) override
    {
        auto patch = compilePlayer (*defaultPerformerFactory, config, cache, preprocessor, externalDataProvider);
        return patch.incrementAndGetPointer();
    }

    //==============================================================================
    struct LinkedProgramImpl  : public RefCountHelper<LinkedProgram, LinkedProgramImpl>
    {
        virtual ~LinkedProgramImpl() = default;

        Span<CompilationMessage> getCompileMessages() const override    { return compileMessagesSpan; }
        const char* getHEARTCode() const override                       { return heartCode.c_str(); }

        std::vector<CompilationMessage> compileMessages;
        Span<CompilationMessage> compileMessagesSpan;
        std::string heartCode;
    };

    LinkedProgram* getLinkedProgram (const PatchPlayerConfiguration& config,
                                     CompilerCache* cache,
                                     SourceFilePreprocessor* preprocessor,
                                     ExternalDataProvider* externalDataProvider) override
    {
        auto linkedProgram = new LinkedProgramImpl();
        auto linkedProgramPtr = LinkedProgram::Ptr (linkedProgram);

        struct NonLinkingPerformerFactory  : public soul::PerformerFactory
        {
            NonLinkingPerformerFactory() = default;

            std::unique_ptr<soul::Performer> createPerformer() override
            {
                return std::make_unique<NonLinkingPerformer> (*this);
            }

            struct NonLinkingPerformer  : public soul::PerformerWrapper
            {
                NonLinkingPerformer (NonLinkingPerformerFactory& f)
                    : PerformerWrapper (soul::llvm::createPerformer()), factory (f)
                {}

                bool load (soul::CompileMessageList& list, const soul::Program& p) noexcept override
                {
                    factory.loadedProgram = p;
                    return PerformerWrapper::load (list, p);
                }

                bool link (soul::CompileMessageList& messageList, const soul::BuildSettings& settings, soul::LinkerCache*) noexcept override
                {
                    try
                    {
                        CompileMessageHandler handler (messageList);
                        auto settingsCopy = settings;
                        factory.loadedProgram = transformations::prepareProgramForCodeGen (factory.loadedProgram, settingsCopy);
                        return true;
                    }
                    catch (AbortCompilationException) {}

                    return false;
                }

                NonLinkingPerformerFactory& factory;
            };

            soul::Program loadedProgram;
        };

        NonLinkingPerformerFactory nonLinkingPerformerFactory;

        if (auto player = compilePlayer (nonLinkingPerformerFactory, config, cache, preprocessor, externalDataProvider))
        {
            auto playerImpl = dynamic_cast<PatchPlayerImpl*> (player.get());
            SOUL_ASSERT (playerImpl != nullptr);

            if (! nonLinkingPerformerFactory.loadedProgram.isEmpty())
                linkedProgram->heartCode = nonLinkingPerformerFactory.loadedProgram.toHEART();

            linkedProgram->compileMessages = playerImpl->compileMessages;
            linkedProgram->compileMessagesSpan = makeSpan (linkedProgram->compileMessages);
        }

        return linkedProgramPtr.incrementAndGetPointer();
    }

    //==============================================================================
    std::unique_ptr<soul::PerformerFactory> defaultPerformerFactory;
    BuildSettings buildSettings;
    VirtualFile::Ptr manifestFile;
    FileList fileList;
    Description::Ptr description;
};

} // namespace soul::patch
