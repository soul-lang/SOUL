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

//==============================================================================
struct RenderOptions
{
    /// An optional audio filename to read into the processor's input stream.
    /// Note that this is expected to be a full, absolute pathname
    std::string inputFilename;

    /// A (non-optional!) filename where the output will be written.
    /// The file must not already exist, but its parent folder must exist.
    /// Note that this is expected to be a full, absolute pathname
    std::string outputFilename;

    /// These properties are used when opening up the destination audio file.
    /// If there is an input file, then numFrames and sampleRate can be left as 0, and
    /// their values will be copied from those of the input file.
    /// If numChannels is left at 0, then the number of channels will be decided based on
    /// the processor's output endpoint types.
    AudioFileProperties outputFileProperties;
};

//==============================================================================
inline bool render (RenderOptions options,
                    soul::patch::PatchInstance& patchInstance,
                    AudioFileFactory& audioFileFactory,
                    CompileMessageList& errors,
                    std::function<bool(double)> handleProgress,
                    soul::patch::CompilerCache::Ptr compilerCache = {},
                    soul::patch::SourceFilePreprocessor::Ptr sourcePreprocessor = {},
                    soul::patch::ExternalDataProvider::Ptr externalDataProvider = {})
{
    CompileMessageHandler handler (errors);
    constexpr uint32_t framesPerBlock = 1024;

    soul::patch::PatchPlayer::RenderContext renderContext = {};
    choc::buffer::ChannelArrayBuffer<float> inputBuffer, outputBuffer;

    try
    {
        if (options.outputFilename.empty())
            throwError (Errors::customRuntimeError ("No output file specified"));

        std::unique_ptr<AudioFileReader> reader;
        AudioFileProperties readerProperties;

        if (! options.inputFilename.empty())
        {
            reader = audioFileFactory.createFileReader (AudioFileFactory::createFileDataSource (options.inputFilename));
            readerProperties = reader->getProperties();

            if (readerProperties.sampleRate < 1)
                throwError (Errors::cannotReadFile (options.inputFilename));

            if (options.outputFileProperties.numFrames == 0)
                options.outputFileProperties.numFrames = readerProperties.numFrames;

            if (options.outputFileProperties.sampleRate == 0)
                options.outputFileProperties.sampleRate = readerProperties.sampleRate;
            else if (options.outputFileProperties.sampleRate != readerProperties.sampleRate)
                throwError (Errors::customRuntimeError ("Cannot use an input file with a different sample rate to the output rate"));
        }

        if (auto player = soul::patch::PatchPlayer::Ptr (patchInstance.compileNewPlayer ({ options.outputFileProperties.sampleRate, framesPerBlock },
                                                                                         compilerCache.get(),
                                                                                         sourcePreprocessor.get(),
                                                                                         externalDataProvider.get())))
        {
            for (auto& m : player->getCompileMessages())
                errors.add (createCompileMessageFromPatchMessage (m));

            if (! errors.hasErrors())
            {
                if (reader != nullptr)
                {
                    auto inputBuses = player->getInputBuses();

                    if (inputBuses.size() == 0)
                        throwError (Errors::customRuntimeError ("SOUL code contains no input stream to connect to "
                                                                  + quoteName (options.inputFilename)));

                    renderContext.numInputChannels = inputBuses.begin()->numChannels;
                    inputBuffer.resize ({ std::max (readerProperties.numChannels, renderContext.numInputChannels), framesPerBlock });
                    inputBuffer.clear();
                    renderContext.inputChannels = inputBuffer.getView().data.channels;
                }

                if (options.outputFileProperties.numFrames == 0)
                    throwError (Errors::customRuntimeError ("Must specify more than zero output samples"));

                renderContext.numOutputChannels = 0;

                for (auto& b : player->getOutputBuses())
                    renderContext.numOutputChannels += b.numChannels;

                if (renderContext.numOutputChannels == 0)
                    throwError (Errors::customRuntimeError ("SOUL code contains no output stream to write to "
                                                               + options.outputFilename));

                if (options.outputFileProperties.numChannels == 0)
                    options.outputFileProperties.numChannels = renderContext.numOutputChannels;

                if (options.outputFileProperties.numChannels == 0 || options.outputFileProperties.numChannels > 512)
                    throwError (Errors::unsupportedNumChannels());

                if (options.outputFileProperties.sampleRate == 0)
                    options.outputFileProperties.sampleRate = 48000;

                if (options.outputFileProperties.sampleRate < 10 || options.outputFileProperties.sampleRate > 10000000)
                    throwError (Errors::unsupportedSampleRate());

                outputBuffer.resize ({ renderContext.numOutputChannels, framesPerBlock });
                outputBuffer.clear();
                renderContext.outputChannels = outputBuffer.getView().data.channels;

                auto writer = audioFileFactory.createFileWriter (options.outputFileProperties,
                                                                 AudioFileFactory::createFileDataSink (options.outputFilename));

                uint64_t framesDone = 0;

                while (framesDone < options.outputFileProperties.numFrames)
                {
                    renderContext.numFrames = static_cast<uint32_t> (std::min (static_cast<uint64_t> (framesPerBlock),
                                                                               options.outputFileProperties.numFrames - framesDone));

                    if (reader != nullptr)
                    {
                        auto source = inputBuffer.getStart (renderContext.numFrames)
                                                 .getChannelRange ({ 0, readerProperties.numChannels });
                        source.clear();
                        reader->read (renderContext.numFrames, source);
                    }

                    player->render (renderContext);

                    writer->append (outputBuffer.getStart (renderContext.numFrames));

                    if (handleProgress != nullptr
                         && ! handleProgress ((double) framesDone / (double) options.outputFileProperties.numFrames))
                        return false;

                    framesDone += renderContext.numFrames;
                }

                return true;
            }
        }
    }
    catch (const AbortCompilationException&) {}

    return false;
}

} // namespace soul::patch