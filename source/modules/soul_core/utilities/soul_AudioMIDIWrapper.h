
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
    A wrapper to simplify the job of rendering a Performer which only needs to deal
    with a synchronous set of audio, MIDI and parameter data (i.e. standard plugin stuff).
*/
template <typename MIDIEventType>
struct AudioMIDIWrapper
{
    AudioMIDIWrapper (Performer& p)  : performer (p)
    {
    }

    ~AudioMIDIWrapper() = default;

    void reset()
    {
        totalFramesRendered = 0;
        preRenderOperations.clear();
        postRenderOperations.clear();
        numInputChannelsExpected = 0;
        numOutputChannelsExpected = 0;
        maxBlockSize = 0;
    }

    /** A lambda to create a function which will provide a non-null Value if called when the given parameter
        has been changed since the last call.
    */
    using GetNewParameterValueFn = std::function<std::function<const float*()>(const EndpointDetails& inputEndpoint)>;

    /**
    */
    using GetRampLengthForSparseStreamFn = std::function<uint32_t(const EndpointDetails&)>;

    /**
    */
    using HandleUnusedEventFn = std::function<bool(uint64_t eventTime, const std::string& endpointName, const Value&)>;

    /**
    */
    void buildRenderingPipeline (SampleRateAndBlockSize rateAndBlockSize,
                                 GetNewParameterValueFn&& getNewParameterValueFn,
                                 GetRampLengthForSparseStreamFn&& getRampLengthForSparseStreamFn,
                                 HandleUnusedEventFn&& handleUnusedEventFn)
    {
        SOUL_ASSERT (rateAndBlockSize.isValid());
        reset();
        auto& perf = performer;
        maxBlockSize = std::min (512u, rateAndBlockSize.blockSize);

        for (auto& inputEndpoint : perf.getInputEndpoints())
        {
            if (isParameterInput (inputEndpoint))
            {
                if (getNewParameterValueFn != nullptr)
                {
                    if (auto getNewValueForParamIfChanged = getNewParameterValueFn (inputEndpoint))
                    {
                        auto endpointHandle = perf.getEndpointHandle (inputEndpoint.endpointID);
                        auto type = inputEndpoint.getSingleSampleType();

                        if (isEvent (inputEndpoint.kind))
                        {
                            preRenderOperations.push_back ([&perf, endpointHandle, getNewValueForParamIfChanged, type] (RenderContext&)
                            {
                                if (auto newValue = getNewValueForParamIfChanged())
                                    perf.addInputEvent (endpointHandle, Value (*newValue).castToTypeExpectingSuccess (type));
                            });
                        }
                        else if (isStream (inputEndpoint.kind))
                        {
                            SOUL_ASSERT (getRampLengthForSparseStreamFn != nullptr);
                            auto rampFrames = getRampLengthForSparseStreamFn (inputEndpoint);

                            preRenderOperations.push_back ([&perf, endpointHandle, getNewValueForParamIfChanged, type, rampFrames] (RenderContext&)
                            {
                                if (auto newValue = getNewValueForParamIfChanged())
                                    perf.setSparseInputStreamTarget (endpointHandle, Value (*newValue).castToTypeExpectingSuccess (type), rampFrames, 0.0f);
                            });
                        }
                        else if (isValue (inputEndpoint.kind))
                        {
                            preRenderOperations.push_back ([&perf, endpointHandle, getNewValueForParamIfChanged, type] (RenderContext&)
                            {
                                if (auto newValue = getNewValueForParamIfChanged())
                                    perf.setInputValue (endpointHandle, Value (*newValue).castToTypeExpectingSuccess (type));
                            });
                        }
                    }
                }
            }
            else if (isMIDIEventEndpoint (inputEndpoint))
            {
                auto endpointHandle = perf.getEndpointHandle (inputEndpoint.endpointID);

                preRenderOperations.push_back ([&perf, endpointHandle] (RenderContext& rc)
                {
                    for (uint32_t i = 0; i < rc.midiInCount; ++i)
                        perf.addInputEvent (endpointHandle, Value ((int32_t) getPackedMIDIEvent (rc.midiIn[i])));
                });
            }
            else if (auto numChans = inputEndpoint.getNumAudioChannels())
            {
                auto endpointHandle = perf.getEndpointHandle (inputEndpoint.endpointID);
                auto& frameType = inputEndpoint.getSingleSampleType();
                auto buffer = soul::Value::zeroInitialiser (frameType.createArray (rateAndBlockSize.blockSize));
                auto startChannel = numInputChannelsExpected;
                auto numSourceChans = (uint32_t) frameType.getVectorSize();

                if (frameType.isFloatingPoint())
                {
                    auto is64Bit = frameType.isFloat64();

                    preRenderOperations.push_back ([&perf, endpointHandle, buffer, startChannel, numSourceChans, is64Bit] (RenderContext& rc) mutable
                    {
                        rc.copyInputFrames (startChannel, numSourceChans, buffer, is64Bit);
                        ignoreUnused (perf.setNextInputStreamFrames (endpointHandle, buffer));
                    });
                }
                else
                {
                    SOUL_ASSERT_FALSE;
                }

                numInputChannelsExpected += numChans;
            }
        }

        for (auto& outputEndpoint : perf.getOutputEndpoints())
        {
            if (isMIDIEventEndpoint (outputEndpoint))
            {
                auto endpointHandle = perf.getEndpointHandle (outputEndpoint.endpointID);

                postRenderOperations.push_back ([&perf, endpointHandle] (RenderContext& rc)
                {
                    perf.iterateOutputEvents (endpointHandle, [&rc] (uint32_t frameOffset, const Value& event) -> bool
                    {
                        if (rc.midiOutCount < rc.midiOutCapacity)
                            createMIDIMessage (rc.midiOut[rc.midiOutCount++], rc.frameOffset + frameOffset, (uint32_t) event.getAsInt32());

                        return true;
                    });
                });
            }
            else if (auto numChans = outputEndpoint.getNumAudioChannels())
            {
                auto endpointHandle = perf.getEndpointHandle (outputEndpoint.endpointID);
                auto& frameType = outputEndpoint.getSingleSampleType();
                auto buffer = soul::Value::zeroInitialiser (frameType.createArray (rateAndBlockSize.blockSize));
                auto startChannel = numOutputChannelsExpected;
                auto numDestChans = (uint32_t) frameType.getVectorSize();

                if (frameType.isFloat32())
                {
                    postRenderOperations.push_back ([&perf, endpointHandle, buffer, startChannel, numDestChans] (RenderContext& rc)
                    {
                        if (auto outputFrames = perf.getOutputStreamFrames (endpointHandle))
                            rc.copyOutputFrames (startChannel, numDestChans, outputFrames->getAsChannelSet32());
                        else
                            SOUL_ASSERT_FALSE;
                    });
                }
                else if (frameType.isFloat64())
                {
                    postRenderOperations.push_back ([&perf, endpointHandle, buffer, startChannel, numDestChans] (RenderContext& rc)
                    {
                        if (auto outputFrames = perf.getOutputStreamFrames (endpointHandle))
                            rc.copyOutputFrames (startChannel, numDestChans, outputFrames->getAsChannelSet64());
                        else
                            SOUL_ASSERT_FALSE;
                    });
                }
                else
                {
                    SOUL_ASSERT_FALSE;
                }

                numOutputChannelsExpected += numChans;
            }
            else if (handleUnusedEventFn != nullptr && isEvent (outputEndpoint.kind))
            {
                auto endpointHandle = perf.getEndpointHandle (outputEndpoint.endpointID);
                auto endpointName = outputEndpoint.name;

                postRenderOperations.push_back ([&perf, endpointHandle, handleUnusedEventFn, endpointName] (RenderContext& rc)
                {
                    perf.iterateOutputEvents (endpointHandle, [&] (uint32_t frameOffset, const Value& eventData) -> bool
                    {
                        return handleUnusedEventFn (rc.totalFramesRendered + frameOffset, endpointName, eventData);
                    });
                });
            }
        }
    }

    void render (DiscreteChannelSet<const float> input,
                 DiscreteChannelSet<float> output,
                 const MIDIEventType* midiIn,
                 MIDIEventType* midiOut,
                 uint32_t midiInCount,
                 uint32_t midiOutCapacity,
                 uint32_t& numMIDIOutMessages)
    {
        SOUL_ASSERT (input.numFrames == output.numFrames && maxBlockSize != 0);

        RenderContext context { totalFramesRendered, input, output, midiIn, midiOut, 0, midiInCount, 0, midiOutCapacity };

        context.iterateInBlocks (maxBlockSize, [&] (RenderContext& rc)
        {
            performer.prepare (rc.inputChannels.numFrames);

            for (auto& op : preRenderOperations)
                op (rc);

            performer.advance();

            for (auto& op : postRenderOperations)
                op (rc);
        },
        [] (const MIDIEventType& midi) { return getFrameIndex (midi); });

        numMIDIOutMessages = context.midiOutCount;
        totalFramesRendered += input.numFrames;
    }

    uint32_t getExpectedNumInputChannels() const     { return numInputChannelsExpected; }
    uint32_t getExpectedNumOutputChannels() const    { return numOutputChannelsExpected; }

    struct RenderContext
    {
        uint64_t totalFramesRendered = 0;
        DiscreteChannelSet<const float> inputChannels;
        DiscreteChannelSet<float> outputChannels;
        const MIDIEventType* midiIn;
        MIDIEventType* midiOut;
        uint32_t frameOffset = 0, midiInCount = 0, midiOutCount = 0, midiOutCapacity = 0;

        template <typename RenderBlockFn, typename GetMIDIFrameFn>
        void iterateInBlocks (uint32_t maxFramesPerBlock, RenderBlockFn&& render, GetMIDIFrameFn&& getMIDIFrame)
        {
            auto framesRemaining = inputChannels.numFrames;
            auto context = *this;

            while (framesRemaining != 0)
            {
                auto framesToDo = std::min (maxFramesPerBlock, framesRemaining);
                context.midiIn = midiIn;
                context.midiInCount = 0;

                while (midiInCount != 0)
                {
                    auto time = getMIDIFrame (*midiIn);

                    if (time > frameOffset)
                    {
                        framesToDo = std::min (framesToDo, time - frameOffset);
                        break;
                    }

                    ++midiIn;
                    --midiInCount;
                    context.midiInCount++;
                }

                context.inputChannels = inputChannels.getSlice (frameOffset, framesToDo);
                context.outputChannels = outputChannels.getSlice (frameOffset, framesToDo);

                render (context);

                frameOffset += framesToDo;
                framesRemaining -= framesToDo;
                context.totalFramesRendered += framesToDo;
                context.frameOffset += framesToDo;
            }

            midiOutCount = context.midiOutCount;
        }

        void copyInputFrames (uint32_t startChannel, uint32_t numChans, Value& buffer, bool as64Bit)
        {
            buffer.getMutableType().modifyArraySize (inputChannels.numFrames);

            if (as64Bit)
                copyChannelSetToFit (buffer.getAsChannelSet64(), inputChannels.getChannelSet (startChannel, numChans));
            else
                copyChannelSetToFit (buffer.getAsChannelSet32(), inputChannels.getChannelSet (startChannel, numChans));
        }

        template <typename ChannelSet>
        void copyOutputFrames (uint32_t startChannel, uint32_t numChans, ChannelSet source)
        {
            copyChannelSetToFit (outputChannels.getChannelSet (startChannel, numChans),
                                 source.getSlice (0, outputChannels.numFrames));
        }
    };

private:
    //==============================================================================
    Performer& performer;

    uint64_t totalFramesRendered = 0;
    std::vector<std::function<void(RenderContext&)>> preRenderOperations, postRenderOperations;
    uint32_t numInputChannelsExpected = 0, numOutputChannelsExpected = 0;
    uint32_t maxBlockSize = 0;
};

}
