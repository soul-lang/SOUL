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

namespace soul
{

//==============================================================================
struct RenderState
{
    RenderState (OfflineRenderOptions& o, PerformerFactory& factory, AudioFileFactory& fileFactory)
       : options (o), performer (factory.createPerformer()), audioFileFactory (fileFactory)
    {
        SOUL_ASSERT (performer != nullptr);
    }

    bool render (Program program,
                 CompileMessageList& errors,
                 BuildSettings settings,
                 std::function<bool(double)>& handleProgress)
    {
        auto framesPerBlock = settings.maxBlockSize;

        if (framesPerBlock == 0)
            framesPerBlock = 1024;
        else if (framesPerBlock > 128 * 1024)
            throwError (Errors::unsupportedBlockSize());

        if (! performer->load (errors, program))
            throwError (Errors::failedToLoadProgram());

        if (options.outputFilename.empty())
            throwError (Errors::customRuntimeError ("No output file specified"));

        if (! options.inputFilename.empty())
            setupReader (options.inputFilename);

        checkOutputFileProperties();

        if (options.outputFileProperties.numFrames == 0)
            throwError (Errors::customRuntimeError ("Must specify more than zero output samples"));

        writer = audioFileFactory.createFileWriter (options.outputFileProperties,
                                                    AudioFileFactory::createFileDataSink (options.outputFilename));

        if (! performer->link (errors, settings, nullptr))
            throwError (Errors::failedToLoadProgram());

        framesPerBlock = std::min (performer->getBlockSize(), framesPerBlock);

        scratchBuffer = choc::buffer::ChannelArrayBuffer<float> (std::max (numInputChannels, options.outputFileProperties.numChannels), framesPerBlock);
        inputFrameArray = choc::value::Value (choc::value::Type::createArrayOfVectors<float> (framesPerBlock, numInputChannels));

        uint64_t framesProcessed = 0;

        while (framesProcessed < options.outputFileProperties.numFrames)
        {
            auto framesToDo = static_cast<uint32_t> (std::min (static_cast<uint64_t> (framesPerBlock),
                                                               options.outputFileProperties.numFrames - framesProcessed));

            performer->prepare (framesToDo);
            readNextBlock (framesToDo);
            performer->advance();
            writeNextBlock (framesToDo);

            if (handleProgress != nullptr && ! handleProgress ((double) framesProcessed / (double) options.outputFileProperties.numFrames))
                return false;

            framesProcessed += framesToDo;
        }

        return true;
    }

private:
    //==============================================================================
    OfflineRenderOptions& options;
    std::unique_ptr<Performer> performer;
    std::unique_ptr<AudioFileReader> reader;
    std::unique_ptr<AudioFileWriter> writer;
    AudioFileFactory& audioFileFactory;

    uint32_t numInputChannels = 0;
    int64_t framesRead = 0;
    EndpointHandle audioInputEndpoint, audioOutputEndpoint;

    choc::buffer::ChannelArrayBuffer<float> scratchBuffer;
    choc::value::Value inputFrameArray;

    void checkOutputFileProperties()
    {
        if (options.outputFileProperties.numFrames == 0)
            if (reader != nullptr)
                options.outputFileProperties.numFrames = reader->getProperties().numFrames;

        if (auto outputID = findFirstOutputOfType (*performer, EndpointType::stream))
        {
            auto details = findDetailsForID (performer->getOutputEndpoints(), outputID);
            auto numSrcChannels = getNumAudioChannels (details);

            if (options.outputFileProperties.numChannels == 0)
                options.outputFileProperties.numChannels = numSrcChannels;
            else if (options.outputFileProperties.numChannels > 512)
                throwError (Errors::unsupportedNumChannels());

            if (options.outputFileProperties.sampleRate == 0)
            {
                options.outputFileProperties.sampleRate = 96000.0;

                if (reader != nullptr)
                    options.outputFileProperties.sampleRate = reader->getProperties().sampleRate;
            }

            if (options.outputFileProperties.sampleRate < 1)
                throwError (Errors::unsupportedSampleRate());

            audioOutputEndpoint = performer->getEndpointHandle (outputID);
        }
        else
        {
            throwError (Errors::customRuntimeError ("SOUL code contains no output stream to write to " + options.outputFilename));
        }
    }

    void setupReader (const std::string& inputFilePath)
    {
        numInputChannels = 0;

        if (auto inputID = findFirstInputOfType (*performer, EndpointType::stream))
        {
            auto details = findDetailsForID (performer->getInputEndpoints(), inputID);
            numInputChannels = details.getFrameType().getNumElements();
            audioInputEndpoint = performer->getEndpointHandle (inputID);

            reader = audioFileFactory.createFileReader (AudioFileFactory::createFileDataSource (inputFilePath));

            if (reader->getProperties().sampleRate < 1)
                throwError (Errors::cannotReadFile (inputFilePath));
        }
        else
        {
            throwError (Errors::customRuntimeError ("SOUL code contains no input stream to connect to "
                                                      + quoteName (inputFilePath)));
        }
    }

    void readNextBlock (uint32_t numFrames)
    {
        if (reader != nullptr)
        {
            auto source = scratchBuffer.getStart (numFrames).getChannelRange ({ 0, numInputChannels });
            source.clear();
            reader->read (framesRead, source);
            copyIntersectionAndClearOutside (getChannelSetFromArray (inputFrameArray), source);

            performer->setNextInputStreamFrames (audioInputEndpoint, inputFrameArray);
            framesRead += numFrames;
        }
    }

    void writeNextBlock (uint32_t numFrames)
    {
        auto source = performer->getOutputStreamFrames (audioOutputEndpoint);
        auto deinterleaved = scratchBuffer.getStart (numFrames).getChannelRange ({ 0, options.outputFileProperties.numChannels });
        copyRemappingChannels (deinterleaved, getChannelSetFromArray (source).getStart (numFrames));
        writer->append (deinterleaved);
    }
};

//==============================================================================
bool offlineRender (OfflineRenderOptions options,
                    PerformerFactory& factory, AudioFileFactory& fileFactory,
                    Program program, CompileMessageList& errors,
                    const BuildSettings& settings, std::function<bool(double)> handleProgress)
{
    try
    {
        CompileMessageHandler handler (errors);
        RenderState state (options, factory, fileFactory);
        return state.render (program, errors, settings, handleProgress);
    }
    catch (const AbortCompilationException&) {}

    return false;
}

}
