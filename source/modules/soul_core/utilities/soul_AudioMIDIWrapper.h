
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
/** Holds the values for a list of traditional float32 parameters, and efficiently
    allows them to be updated and for changed ones to be iterated.
*/
struct ParameterStateList
{
    ParameterStateList() = default;

    /** Initialises the list with an array of per-parameter update actions. */
    void initialise (ArrayView<std::function<void(float)>> parameterUpdateActions)
    {
        parameters.clear();
        parameters.reserve (parameterUpdateActions.size());

        for (auto& action : parameterUpdateActions)
            parameters.push_back ({ action, {}, 0 });

        std::vector<Parameter*> parameterPtrs;
        parameterPtrs.reserve (parameters.size());

        for (auto& p : parameters)
            parameterPtrs.push_back (std::addressof (p));

        auto dirtyHandles = dirtyList.initialise (parameterPtrs);

        for (size_t i = 0; i < parameters.size(); ++i)
            parameters[i].dirtyListHandle = dirtyHandles[i];
    }

    /** Sets the current value for a parameter, and if the value has changed, marks it as
        needing an update.
    */
    void setParameter (uint32_t parameterIndex, float newValue)
    {
        SOUL_ASSERT (parameterIndex < parameters.size());
        auto& param = parameters[parameterIndex];

        if (param.currentValue != newValue)
        {
            param.currentValue = newValue;
            dirtyList.markAsDirty (param.dirtyListHandle);
        }
    }

    /** Forces the parameter to be marked as needing an update. */
    void markAsChanged (uint32_t parameterIndex)
    {
        SOUL_ASSERT (parameterIndex < parameters.size());
        dirtyList.markAsDirty (parameters[parameterIndex].dirtyListHandle);
    }

    /** Calls the action function for any parameters which have had their value
        modified by setParameter() or markAsChanged().
    */
    void performActionsForUpdatedParameters()
    {
        while (auto p = dirtyList.popNextDirtyObject())
            p->action (p->currentValue);
    }

private:
    struct Parameter
    {
        std::function<void(float)> action;
        choc::fifo::DirtyList<Parameter>::Handle dirtyListHandle;
        float currentValue = 0;
    };

    std::vector<Parameter> parameters;
    choc::fifo::DirtyList<Parameter> dirtyList;
};

//==============================================================================
/**
*/
struct InputEventQueue
{
    InputEventQueue() = default;

    using HandleEventFn = std::function<void(EndpointHandle, const choc::value::ValueView&)>;

    void initialise (size_t maxQueueSize, HandleEventFn handleEvent)
    {
        fifoSlots.resize (maxQueueSize);
        processEvent = handleEvent;
    }

    void clear()
    {
        fifoPosition.reset();
    }

    bool pushEvent (EndpointHandle endpointHandle, const choc::value::ValueView& eventValue)
    {
        if (auto slot = fifoPosition.lockSlotForWriting())
        {
            auto& event = fifoSlots[slot.index];
            event.endpointHandle = endpointHandle;
            event.event = eventValue;
            fifoPosition.unlock (slot);
            return true;
        }

        return false;
    }

    void processQueuedEvents (size_t maxNumToProcess)
    {
        for (;;)
        {
            if (auto slot = fifoPosition.lockSlotForReading())
            {
                auto& event = fifoSlots[slot.index];
                processEvent (event.endpointHandle, event.event);
                fifoPosition.unlock (slot);

                if (maxNumToProcess-- != 0)
                    continue;
            }

            break;
        }
    }

private:
    //==============================================================================
    struct Event
    {
        EndpointHandle endpointHandle;
        choc::value::Value event;
    };

    std::vector<Event> fifoSlots;
    choc::fifo::FIFOReadWritePosition<uint32_t, std::atomic<uint32_t>> fifoPosition;
    HandleEventFn processEvent;
};

//==============================================================================
/**
    A wrapper to simplify the job of rendering a Performer which only needs to deal
    with a synchronous set of audio, MIDI and parameter data (i.e. standard plugin stuff).
*/
struct AudioMIDIWrapper
{
    AudioMIDIWrapper (Performer& p)  : performer (p) {}
    ~AudioMIDIWrapper() = default;

    void reset()
    {
        totalFramesRendered = 0;
        preRenderOperations.clear();
        postRenderOperations.clear();
        inputEventQueue.clear();
        numInputChannelsExpected = 0;
        numOutputChannelsExpected = 0;
        maxBlockSize = 0;
    }

    std::vector<EndpointDetails> getAudioInputEndpoints()       { return getInputEndpoints (InputEndpointType::audio); }
    std::vector<EndpointDetails> getParameterEndpoints()        { return getInputEndpoints (InputEndpointType::parameter); }
    std::vector<EndpointDetails> getEventInputEndpoints()       { return getInputEndpoints (InputEndpointType::event); }

    std::vector<EndpointDetails> getAudioOutputEndpoints()      { return getOutputEndpoints (OutputEndpointType::audio); }
    std::vector<EndpointDetails> getEventOutputEndpoints()      { return getOutputEndpoints (OutputEndpointType::event); }

    /**
    */
    using GetRampLengthForSparseStreamFn = std::function<uint32_t(const EndpointDetails&)>;

    /**
    */
    using HandleUnusedEventFn = std::function<bool(uint64_t eventTime, const std::string& endpointName, const choc::value::ValueView&)>;

    /**
    */
    void buildRenderingPipeline (uint32_t processorMaxBlockSize,
                                 GetRampLengthForSparseStreamFn&& getRampLengthForSparseStreamFn,
                                 HandleUnusedEventFn&& handleUnusedEventFn)
    {
        SOUL_ASSERT (processorMaxBlockSize > 0);
        reset();
        auto& perf = performer;
        maxBlockSize = std::min (512u, processorMaxBlockSize);

        inputEventQueue.initialise (1024, [&perf] (EndpointHandle endpointHandle, const choc::value::ValueView& value)
        {
            perf.addInputEvent (endpointHandle, value);
        });

        {
            std::vector<std::function<void(float)>> parameterUpdateActions;

            for (auto& parameterInput : getParameterEndpoints())
            {
                auto endpointHandle = perf.getEndpointHandle (parameterInput.endpointID);
                auto floatValue = choc::value::createFloat32 (0);

                if (isEvent (parameterInput))
                {
                    parameterUpdateActions.push_back ([&perf, endpointHandle, floatValue] (float newValue) mutable
                    {
                        floatValue.getViewReference().set (newValue);
                        perf.addInputEvent (endpointHandle, floatValue);
                    });
                }
                else if (isStream (parameterInput))
                {
                    SOUL_ASSERT (getRampLengthForSparseStreamFn != nullptr);
                    auto rampFrames = getRampLengthForSparseStreamFn (parameterInput);

                    parameterUpdateActions.push_back ([&perf, endpointHandle, floatValue, rampFrames] (float newValue) mutable
                    {
                        floatValue.getViewReference().set (newValue);
                        perf.setSparseInputStreamTarget (endpointHandle, floatValue, rampFrames);
                    });
                }
                else if (isValue (parameterInput))
                {
                    parameterUpdateActions.push_back ([&perf, endpointHandle, floatValue] (float newValue) mutable
                    {
                        floatValue.getViewReference().set (newValue);
                        perf.setInputValue (endpointHandle, floatValue);
                    });
                }
            }

            parameterList.initialise (parameterUpdateActions);
        }

        for (auto& midiInput : getInputEndpoints (InputEndpointType::midi))
        {
            auto endpointHandle = perf.getEndpointHandle (midiInput.endpointID);
            choc::value::Value midiEvent (midiInput.getSingleEventType());

            preRenderOperations.push_back ([&perf, endpointHandle, midiEvent] (RenderContext& rc) mutable
            {
                for (uint32_t i = 0; i < rc.midiInCount; ++i)
                {
                    midiEvent.getObjectMemberAt (0).value.set (rc.midiIn[i].getPackedMIDIData());
                    perf.addInputEvent (endpointHandle, midiEvent);
                }
            });
        }

        for (auto& audioInput : getInputEndpoints (InputEndpointType::audio))
        {
            auto numSourceChans = audioInput.getNumAudioChannels();
            SOUL_ASSERT (numSourceChans != 0);
            auto endpointHandle = perf.getEndpointHandle (audioInput.endpointID);
            auto& frameType = audioInput.getFrameType();
            auto startChannel = numInputChannelsExpected;
            auto numChans = frameType.getNumElements();

            if (frameType.isFloat() || (frameType.isVector() && frameType.getElementType().isFloat()))
            {
                if (numChans == 1)
                {
                    preRenderOperations.push_back ([&perf, endpointHandle, startChannel] (RenderContext& rc)
                    {
                        auto channel = rc.inputChannels.getChannel (startChannel);
                        perf.setNextInputStreamFrames (endpointHandle, choc::value::createArrayView (const_cast<float*> (channel.data.data),
                                                                                                     channel.getNumFrames()));
                    });
                }
                else
                {
                    choc::buffer::InterleavedBuffer<float> interleaved (numChans, maxBlockSize);

                    preRenderOperations.push_back ([&perf, endpointHandle, startChannel, numChans, interleaved] (RenderContext& rc)
                    {
                        auto numFrames = rc.inputChannels.getNumFrames();

                        copy (interleaved.getStart (numFrames),
                              rc.inputChannels.getChannelRange ({ startChannel, startChannel + numChans }));

                        perf.setNextInputStreamFrames (endpointHandle, choc::value::create2DArrayView (interleaved.getView().data.data,
                                                                                                       numFrames, interleaved.getNumChannels()));
                    });
                }
            }
            else
            {
                SOUL_ASSERT_FALSE;
            }

            numInputChannelsExpected += numSourceChans;
        }

        for (auto& outputEndpoint : perf.getOutputEndpoints())
        {
            if (isMIDIEventEndpoint (outputEndpoint))
            {
                auto endpointHandle = perf.getEndpointHandle (outputEndpoint.endpointID);

                postRenderOperations.push_back ([&perf, endpointHandle] (RenderContext& rc)
                {
                    perf.iterateOutputEvents (endpointHandle, [&rc] (uint32_t frameOffset, const choc::value::ValueView& event) -> bool
                    {
                        if (rc.midiOutCount < rc.midiOutCapacity)
                            rc.midiOut[rc.midiOutCount++] = MIDIEvent::fromPackedMIDIData (rc.frameOffset + frameOffset,
                                                                                           event["midiBytes"].getInt32());

                        return true;
                    });
                });
            }
            else if (auto numChans = outputEndpoint.getNumAudioChannels())
            {
                auto endpointHandle = perf.getEndpointHandle (outputEndpoint.endpointID);
                auto& frameType = outputEndpoint.getFrameType();
                auto startChannel = numOutputChannelsExpected;
                numOutputChannelsExpected += numChans;

                if (frameType.isFloat() || (frameType.isVector() && frameType.getElementType().isFloat()))
                {
                    postRenderOperations.push_back ([&perf, endpointHandle, startChannel, numChans] (RenderContext& rc)
                    {
                        copyIntersectionAndClearOutside (rc.outputChannels.getChannelRange ({ startChannel, startChannel + numChans }),
                                                         getChannelSetFromArray (perf.getOutputStreamFrames (endpointHandle)));
                    });
                }
                else
                {
                    SOUL_ASSERT_FALSE;
                }
            }
            else if (handleUnusedEventFn != nullptr && isEvent (outputEndpoint))
            {
                auto endpointHandle = perf.getEndpointHandle (outputEndpoint.endpointID);
                auto endpointName = outputEndpoint.name;

                postRenderOperations.push_back ([&perf, endpointHandle, handleUnusedEventFn, endpointName] (RenderContext& rc)
                {
                    perf.iterateOutputEvents (endpointHandle, [&] (uint32_t frameOffset, const choc::value::ValueView& eventData) -> bool
                    {
                        return handleUnusedEventFn (rc.totalFramesRendered + frameOffset, endpointName, eventData);
                    });
                });
            }
        }
    }

    void render (choc::buffer::ChannelArrayView<const float> input,
                 choc::buffer::ChannelArrayView<float> output,
                 const MIDIEvent* midiIn,
                 MIDIEvent* midiOut,
                 uint32_t midiInCount,
                 uint32_t midiOutCapacity,
                 uint32_t& numMIDIOutMessages)
    {
        SOUL_ASSERT (input.getNumFrames() == output.getNumFrames() && maxBlockSize != 0);

        RenderContext context { totalFramesRendered, input, output, midiIn, midiOut, 0, midiInCount, 0, midiOutCapacity };

        context.iterateInBlocks (maxBlockSize, [&] (RenderContext& rc)
        {
            performer.prepare (rc.inputChannels.getNumFrames());

            for (auto& op : preRenderOperations)
                op (rc);

            parameterList.performActionsForUpdatedParameters();
            inputEventQueue.processQueuedEvents (256);

            performer.advance();

            for (auto& op : postRenderOperations)
                op (rc);
        });

        numMIDIOutMessages = context.midiOutCount;
        totalFramesRendered += input.getNumFrames();
    }

    uint32_t getExpectedNumInputChannels() const     { return numInputChannelsExpected; }
    uint32_t getExpectedNumOutputChannels() const    { return numOutputChannelsExpected; }

    struct RenderContext
    {
        uint64_t totalFramesRendered = 0;
        choc::buffer::ChannelArrayView<const float> inputChannels;
        choc::buffer::ChannelArrayView<float> outputChannels;
        const MIDIEvent* midiIn;
        MIDIEvent* midiOut;
        uint32_t frameOffset = 0, midiInCount = 0, midiOutCount = 0, midiOutCapacity = 0;

        template <typename RenderBlockFn>
        void iterateInBlocks (uint32_t maxFramesPerBlock, RenderBlockFn&& render)
        {
            auto framesRemaining = inputChannels.getNumFrames();
            auto context = *this;

            while (framesRemaining != 0)
            {
                auto framesToDo = std::min (maxFramesPerBlock, framesRemaining);
                context.midiIn = midiIn;
                context.midiInCount = 0;

                while (midiInCount != 0)
                {
                    auto time = midiIn->frameIndex;

                    if (time > frameOffset)
                    {
                        framesToDo = std::min (framesToDo, time - frameOffset);
                        break;
                    }

                    ++midiIn;
                    --midiInCount;
                    context.midiInCount++;
                }

                context.inputChannels  = inputChannels.getFrameRange ({ frameOffset, frameOffset + framesToDo });
                context.outputChannels = outputChannels.getFrameRange ({ frameOffset, frameOffset + framesToDo });

                render (context);

                frameOffset += framesToDo;
                framesRemaining -= framesToDo;
                context.totalFramesRendered += framesToDo;
                context.frameOffset += framesToDo;
            }

            midiOutCount = context.midiOutCount;
        }
    };

    ParameterStateList parameterList;
    InputEventQueue inputEventQueue;

private:
    //==============================================================================
    Performer& performer;

    uint64_t totalFramesRendered = 0;
    std::vector<std::function<void(RenderContext&)>> preRenderOperations;
    std::vector<std::function<void(RenderContext&)>> postRenderOperations;
    uint32_t numInputChannelsExpected = 0, numOutputChannelsExpected = 0;
    uint32_t maxBlockSize = 0;

    enum class InputEndpointType
    {
        audio,
        parameter,
        midi,
        event,
        other
    };

    static InputEndpointType getInputEndpointType (const EndpointDetails& details)
    {
        if (isParameterInput (details))         return InputEndpointType::parameter;
        if (isMIDIEventEndpoint (details))      return InputEndpointType::midi;
        if (details.getNumAudioChannels() != 0) return InputEndpointType::audio;
        if (isEvent (details))                  return InputEndpointType::event;

        return InputEndpointType::other;
    }

    std::vector<EndpointDetails> getInputEndpoints (InputEndpointType type)
    {
        std::vector<EndpointDetails> results;

        for (auto& e : performer.getInputEndpoints())
            if (getInputEndpointType (e) == type)
                results.push_back (e);

        return results;
    }

    enum class OutputEndpointType
    {
        audio,
        midi,
        event,
        other
    };

    static OutputEndpointType getOutputEndpointType (const EndpointDetails& details)
    {
        if (isMIDIEventEndpoint (details))      return OutputEndpointType::midi;
        if (details.getNumAudioChannels() != 0) return OutputEndpointType::audio;
        if (isEvent (details))                  return OutputEndpointType::event;

        return OutputEndpointType::other;
    }

    std::vector<EndpointDetails> getOutputEndpoints (OutputEndpointType type)
    {
        std::vector<EndpointDetails> results;

        for (auto& e : performer.getOutputEndpoints())
            if (getOutputEndpointType (e) == type)
                results.push_back (e);

        return results;
    }
};

}
