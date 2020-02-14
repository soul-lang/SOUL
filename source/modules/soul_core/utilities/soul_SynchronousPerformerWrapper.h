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
/**
    Wraps up the endpoints of a Performer so that it can be rendered using a single
    synchronous call to provide all the audio and MIDI i/o.
*/
struct SynchronousPerformerWrapper
{
    SynchronousPerformerWrapper (Performer& p)  : performer (p)
    {
    }

    ~SynchronousPerformerWrapper()
    {
        detach();
    }

    void attach (EndpointProperties properties)
    {
        detach();

        for (auto& i : performer.getInputEndpoints())
        {
            if (auto numChans = i.getNumAudioChannels())
            {
                if (! isParameterInput (i))
                {
                    sources.push_back (std::make_unique<InputBufferSliceSource> (*performer.getInputSource (i.endpointID),
                                                                                 i, totalNumInputChannels, numChans, properties));
                    totalNumInputChannels += numChans;
                }
            }

            if (isMIDIEventEndpoint (i))
                midiInputQueues.push_back (std::make_unique<MidiInEventQueueType> (*performer.getInputSource (i.endpointID), i, properties));
        }

        for (auto& o : performer.getOutputEndpoints())
        {
            if (auto numChans = o.getNumAudioChannels())
            {
                sinks.push_back (std::make_unique<OutputBufferSliceSink> (*performer.getOutputSink (o.endpointID),
                                                                          o, totalNumOutputChannels, numChans, properties));
                totalNumOutputChannels += numChans;
            }

            if (isMIDIEventEndpoint (o) && midiOutputQueue == nullptr)
                midiOutputQueue = std::make_unique<MidiOutEventQueueType> (*performer.getOutputSink (o.endpointID), o, properties);
        }
    }

    void detach()
    {
        sources.clear();
        sinks.clear();
        midiInputQueues.clear();
        midiOutputQueue.reset();
        totalNumInputChannels = 0;
        totalNumOutputChannels = 0;
    }

    template <typename MIDIEventType>
    void render (DiscreteChannelSet<const float> input,
                 DiscreteChannelSet<float> output,
                 const MIDIEventType* midiInStart,
                 const MIDIEventType* midiInEnd,
                 MIDIEventType* midiOut,
                 uint32_t midiOutCapacity,
                 uint32_t& numMIDIOutMessages)
    {
        SOUL_ASSERT (input.numFrames == output.numFrames);

        for (auto& queue : midiInputQueues)
            for (auto midi = midiInStart; midi != midiInEnd; ++midi)
                queue->enqueueEvent (getFrameIndex (*midi), (int) getPackedMIDIEvent (*midi));

        if (input.numChannels != 0)
            for (auto& s : sources)
                s->prepareBuffer (input);

        for (auto& s : sinks)
            s->prepareBuffer (output);

        auto blockSize = performer.getBlockSize();
        auto framesToRender = output.numFrames;

        while (framesToRender > 0)
        {
            auto framesThisBlock = std::min (blockSize, framesToRender);

            performer.prepare (framesThisBlock);
            performer.advance();

            framesToRender -= framesThisBlock;
        }

        numMIDIOutMessages = 0;

        if (midiOutputQueue != nullptr)
            midiOutputQueue->readNextEvents (midiOutCapacity,
                                            [midiOut, midiOutCapacity, &numMIDIOutMessages] (uint32_t frameOffset, int32_t packedData)
                                            {
                                                if (numMIDIOutMessages < midiOutCapacity)
                                                    createMIDIMessage (midiOut[numMIDIOutMessages], frameOffset, (uint32_t) packedData);

                                                ++numMIDIOutMessages;
                                            });
    }

    uint32_t getExpectedNumInputChannels() const     { return totalNumInputChannels; }
    uint32_t getExpectedNumOutputChannels() const    { return totalNumOutputChannels; }

private:
    struct InputBufferSliceSource
    {
        InputBufferSliceSource (InputSource& inputToAttachTo, const EndpointDetails& details,
                                uint32_t startChannel, uint32_t numChannels,
                                EndpointProperties properties)
            : input (inputToAttachTo),
              sliceStartChannel (startChannel),
              sliceNumChannels (numChannels)
        {
            if (details.getSingleSampleType().isFloat64())
            {
                inputToAttachTo.setStreamSource ([=] (void* dest, uint32_t requestedFrames) -> uint32_t
                {
                    if (isBufferAvailable)
                    {
                        auto providedFrames = std::min (requestedFrames, currentBuffer.getAvailableSamples (bufferOffset));

                        InterleavedChannelSet<double> destChannels { static_cast<double*> (dest),
                                                                     numChannels, providedFrames, numChannels };

                        copyChannelSetToFit (destChannels, currentBuffer.getSlice (bufferOffset, providedFrames));
                        bufferOffset += providedFrames;
                        isBufferAvailable = (bufferOffset < currentBuffer.numFrames);

                        return providedFrames;
                    }

                    return 0;
                }, properties);
            }
            else
            {
                inputToAttachTo.setStreamSource ([=] (void* dest, uint32_t requestedFrames) -> uint32_t
                {
                    if (isBufferAvailable)
                    {
                        auto providedFrames = std::min (requestedFrames, currentBuffer.getAvailableSamples (bufferOffset));

                        InterleavedChannelSet<float> destChannels { static_cast<float*> (dest),
                                                                    numChannels, providedFrames, numChannels };

                        copyChannelSetToFit (destChannels, currentBuffer.getSlice (bufferOffset, providedFrames));
                        bufferOffset += providedFrames;
                        isBufferAvailable = (bufferOffset < currentBuffer.numFrames);

                        return providedFrames;
                    }

                    return 0;
                }, properties);
            }
        }

        ~InputBufferSliceSource()
        {
            input->removeSource();
        }

        void prepareBuffer (DiscreteChannelSet<const float> completeChannelSet)
        {
            currentBuffer = completeChannelSet.getChannelSet (sliceStartChannel, sliceNumChannels);
            isBufferAvailable = true;
            bufferOffset = 0;
        }

        const InputSource::Ptr input;
        const uint32_t sliceStartChannel, sliceNumChannels;
        bool isBufferAvailable = false;
        DiscreteChannelSet<const float> currentBuffer;
        uint32_t bufferOffset = 0;
    };

    //==============================================================================
    struct OutputBufferSliceSink
    {
        OutputBufferSliceSink (OutputSink& outputToAttachTo, const EndpointDetails& details,
                               uint32_t startChannel, uint32_t numChannels,
                               EndpointProperties properties)
            : output (outputToAttachTo),
              sliceStartChannel (startChannel),
              sliceNumChannels (numChannels)
        {
            if (details.getSingleSampleType().isFloat64())
            {
                outputToAttachTo.setStreamSink ([=] (const void* src, uint32_t numFrames) -> uint32_t
                {
                    if (isBufferAvailable)
                    {
                        InterleavedChannelSet<const double> srcChannels { static_cast<const double*> (src),
                                                                          numChannels, numFrames, numChannels };
                        copyChannelSetToFit (currentBuffer.getSlice (bufferOffset, numFrames), srcChannels);
                        bufferOffset += numFrames;
                        isBufferAvailable = (bufferOffset < currentBuffer.numFrames);
                    }

                    return numFrames;
                }, properties);
            }
            else
            {
                outputToAttachTo.setStreamSink ([=] (const void* src, uint32_t numFrames) -> uint32_t
                {
                    if (isBufferAvailable)
                    {
                        InterleavedChannelSet<const float> srcChannels { static_cast<const float*> (src),
                                                                         numChannels, numFrames, numChannels };
                        copyChannelSetToFit (currentBuffer.getSlice (bufferOffset, numFrames), srcChannels);
                        bufferOffset += numFrames;
                        isBufferAvailable = (bufferOffset < currentBuffer.numFrames);
                    }

                    return numFrames;
                }, properties);
            }
        }

        ~OutputBufferSliceSink()
        {
            output->removeSink();
        }

        void prepareBuffer (DiscreteChannelSet<float> completeChannelSet)
        {
            currentBuffer = completeChannelSet.getChannelSet (sliceStartChannel, sliceNumChannels);
            isBufferAvailable = true;
            bufferOffset = 0;
        }

        const OutputSink::Ptr output;
        const uint32_t sliceStartChannel, sliceNumChannels;
        bool isBufferAvailable = false;
        DiscreteChannelSet<float> currentBuffer;
        uint32_t bufferOffset = 0;
    };

    //==============================================================================
    Performer& performer;

    std::vector<std::unique_ptr<InputBufferSliceSource>> sources;
    std::vector<std::unique_ptr<OutputBufferSliceSink>> sinks;

    using MidiInEventQueueType = InputEventQueue<EventFIFO<int32_t, uint64_t>>;
    using MidiOutEventQueueType = OutputEventQueue<EventFIFO<int32_t, uint64_t>>;

    std::vector<std::unique_ptr<MidiInEventQueueType>> midiInputQueues;
    std::unique_ptr<MidiOutEventQueueType> midiOutputQueue;

    uint32_t totalNumInputChannels = 0, totalNumOutputChannels = 0;
};

}
