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

template <typename Value>
choc::value::Value createWrappedSparseStreamEventHolder (const Value& value, uint32_t numFrames)
{
    return choc::value::createObject ("_RampHolder",
                                      "rampFrames", static_cast<int32_t> (numFrames),
                                      "target", value);
}

template <typename Target>
bool applySparseStreamEventWrapper (Target& target, EndpointHandle endpoint, const choc::value::ValueView& v)
{
    if (v.isObjectWithClassName ("_RampHolder"))
    {
        target.setSparseInputStreamTarget (endpoint, v.getObjectMemberAt (1).value,
                                           static_cast<uint32_t> (v.getObjectMemberAt (0).value.getInt32()));
        return true;
    }

    return false;
}

//==============================================================================
struct MIDIEventInputList
{
    const MIDIEvent* listStart = nullptr;
    const MIDIEvent* listEnd = nullptr;

    const MIDIEvent* begin() const      { return listStart; }
    const MIDIEvent* end() const        { return listEnd; }

    MIDIEventInputList removeEventsBefore (uint32_t frameIndex)
    {
        auto i = listStart;

        while (i != listEnd && i->frameIndex < frameIndex)
            ++i;

        auto oldStart = listStart;
        listStart = i;
        return { oldStart, i };
    }
};

//==============================================================================
struct MIDIEventOutputList
{
    MIDIEvent* start = nullptr;
    uint32_t capacity = 0;

    bool addEvent (MIDIEvent e)
    {
        if (capacity == 0)
            return false;

        *start++ = e;
        --capacity;
        return true;
    }
};

//==============================================================================
struct AudioInputList
{
    AudioInputList() = default;

    void initialise (uint32_t newMaxBlockSize)
    {
        maxBlockSize = newMaxBlockSize;
        clear();
    }

    template <typename PerformerOrSession>
    void attachToAllAudioEndpoints (PerformerOrSession& p)
    {
        clear();

        for (auto& e : getInputEndpointsOfType (p, InputEndpointType::audio))
        {
            auto numChannels = getNumAudioChannels (e);
            const auto& frameType = e.getFrameType();
            SOUL_ASSERT (numChannels != 0 && numChannels == frameType.getNumElements());
            SOUL_ASSERT (frameType.isFloat() || (frameType.isVector() && frameType.getElementType().isFloat()));
            connectEndpoint (p, e.endpointID, { totalNumChannels, totalNumChannels + numChannels });
        }
    }

    void clear()
    {
        mappings.clear();
        totalNumChannels = 0;
        scratchBuffer.resize ({ 1, maxBlockSize });
    }

    template <typename PerformerOrSession>
    void connectEndpoint (PerformerOrSession& p, EndpointID endpointID, choc::buffer::ChannelRange channels)
    {
        mappings.push_back ({ p.getEndpointHandle (endpointID), channels });

        if (channels.size() > scratchBuffer.getNumChannels())
            scratchBuffer.resize ({ channels.size(), maxBlockSize });

        totalNumChannels = std::max (totalNumChannels, channels.end);
    }

    bool addToFIFO (MultiEndpointFIFO& fifo, uint64_t time, choc::buffer::ChannelArrayView<const float> inputChannels)
    {
        auto numFrames = inputChannels.getNumFrames();

        for (auto& mapping : mappings)
        {
            if (mapping.channels.size() == 1)
            {
                auto channel = inputChannels.getChannel (mapping.channels.start);

                if (! fifo.addInputData (mapping.endpoint, time,
                                         choc::value::createArrayView (const_cast<float*> (channel.data.data), numFrames)))
                    return false;
            }
            else
            {
                copy (scratchBuffer.getStart (numFrames), inputChannels.getChannelRange (mapping.channels));

                if (! fifo.addInputData (mapping.endpoint, time,
                                         choc::value::create2DArrayView (scratchBuffer.getView().data.data,
                                                                         numFrames, mapping.channels.size())))
                    return false;
            }
        }

        return true;
    }

    struct InputMapping
    {
        EndpointHandle endpoint;
        choc::buffer::ChannelRange channels;
    };

    std::vector<InputMapping> mappings;
    uint32_t maxBlockSize = 0, totalNumChannels = 0;
    choc::buffer::InterleavedBuffer<float> scratchBuffer;
};

//==============================================================================
struct AudioOutputList
{
    AudioOutputList() = default;

    template <typename PerformerOrSession>
    void initialise (PerformerOrSession& p)
    {
        clear();

        for (auto& e : getOutputEndpointsOfType (p, OutputEndpointType::audio))
        {
            auto numChannels = getNumAudioChannels (e);
            const auto& frameType = e.getFrameType();
            SOUL_ASSERT (numChannels != 0 && numChannels == frameType.getNumElements());
            SOUL_ASSERT (frameType.isFloat() || (frameType.isVector() && frameType.getElementType().isFloat()));

            connectEndpoint (p, e.endpointID, { totalNumChannels, totalNumChannels + numChannels });
        }
    }

    void clear()
    {
        mappings.clear();
        totalNumChannels = 0;
    }

    template <typename PerformerOrSession>
    void connectEndpoint (PerformerOrSession& p, EndpointID endpointID, choc::buffer::ChannelRange channels)
    {
        mappings.push_back ({ p.getEndpointHandle (endpointID), channels });
        totalNumChannels = std::max (totalNumChannels, channels.end);
    }

    template <typename PerformerOrSession>
    void handleOutputData (PerformerOrSession& p, choc::buffer::ChannelArrayView<float> outputChannels)
    {
        for (auto& mapping : mappings)
            addIntersection (outputChannels.getChannelRange (mapping.channels),
                             getChannelSetFromArray (p.getOutputStreamFrames (mapping.endpoint)));
    }

    struct OutputMapping
    {
        EndpointHandle endpoint;
        choc::buffer::ChannelRange channels;
    };

    uint32_t totalNumChannels = 0;
    std::vector<OutputMapping> mappings;
};

//==============================================================================
struct MIDIInputList
{
    MIDIInputList() = default;

    template <typename PerformerOrSession>
    void initialise (PerformerOrSession& p)
    {
        clear();

        for (auto& e : getInputEndpointsOfType (p, InputEndpointType::midi))
            connectEndpoint (p, e.endpointID);
    }

    void clear()
    {
        inputs.clear();
    }

    template <typename PerformerOrSession>
    void connectEndpoint (PerformerOrSession& p, const EndpointID& endpointID)
    {
        auto& details = findDetailsForID (p.getInputEndpoints(), endpointID);

        inputs.push_back ({ p.getEndpointHandle (endpointID),
                            choc::value::Value (details.getSingleEventType()) });
    }

    bool addToFIFO (MultiEndpointFIFO& fifo, uint64_t time, MIDIEventInputList midiEvents)
    {
        if (! inputs.empty())
        {
            for (auto e : midiEvents)
            {
                auto eventTime = time + e.frameIndex;

                for (auto& input : inputs)
                {
                    input.midiEvent.getObjectMemberAt (0).value.set (e.getPackedMIDIData());

                    if (! fifo.addInputData (input.endpoint, eventTime, input.midiEvent))
                        return false;
                }
            }
        }

        return true;
    }

    struct MIDIInput
    {
        EndpointHandle endpoint;
        choc::value::Value midiEvent;
    };

    std::vector<MIDIInput> inputs;
};

//==============================================================================
struct MIDIOutputList
{
    MIDIOutputList() = default;

    void initialise (Performer& p)
    {
        clear();

        for (auto& e : getOutputEndpointsOfType (p, OutputEndpointType::midi))
            outputs.push_back (p.getEndpointHandle (e.endpointID));
    }

    void clear()
    {
        outputs.clear();
    }

    void handleOutputData (Performer& p, uint32_t startFrame, MIDIEventOutputList& midiOut)
    {
        if (midiOut.capacity == 0)
            return;

        for (auto& output : outputs)
        {
            p.iterateOutputEvents (output, [=, &midiOut] (uint32_t frameOffset, const choc::value::ValueView& event) -> bool
            {
                return midiOut.addEvent (MIDIEvent::fromPackedMIDIData (startFrame + frameOffset,
                                                                        event["midiBytes"].getInt32()));
            });
        }
    }

    std::vector<EndpointHandle> outputs;
};

//==============================================================================
struct EventOutputList
{
    EventOutputList() = default;

    void clear()
    {
        fifo.reset (128 * 1024, 4096);
        endpointNames.clear();
    }

    template <typename PerformerOrSession>
    void initialise (PerformerOrSession& p)
    {
        for (auto& e : getOutputEndpointsOfType (p, OutputEndpointType::event))
            if (! isMIDIEventEndpoint (e))
                connectEndpoint (p, e.endpointID);
    }

    template <typename PerformerOrSession>
    bool connectEndpoint (PerformerOrSession& p, const EndpointID& endpointID)
    {
        for (auto& e : p.getOutputEndpoints())
        {
            if (e.endpointID == endpointID)
            {
                auto handle = p.getEndpointHandle (endpointID);
                outputs.push_back (handle);
                endpointNames[handle] = e.name;
                return true;
            }
        }

        return false;
    }

    template <typename PerformerOrSession>
    bool postOutputEvents (PerformerOrSession& p, uint64_t position)
    {
        bool success = true;

        for (auto& output : outputs)
        {
            p.iterateOutputEvents (output, [&] (uint32_t frameOffset, const choc::value::ValueView& event) -> bool
            {
                if (! fifo.addInputData (output, position + frameOffset, event))
                    success = false;

                return true;
            });
        }

        return success;
    }

    template <typename HandleEventFn>
    void deliverPendingEvents (HandleEventFn&& handleEvent)
    {
        fifo.iterateAllAvailable ([&] (EndpointHandle endpoint, uint64_t time, const choc::value::ValueView& value)
                                  {
                                      auto name = endpointNames.find (endpoint);
                                      SOUL_ASSERT (name != endpointNames.end());
                                      handleEvent (time, name->second, value);
                                  });
    }

    std::vector<EndpointHandle> outputs;
    MultiEndpointFIFO fifo;
    std::unordered_map<EndpointHandle, std::string> endpointNames;
};

//==============================================================================
/** Holds the values for a list of traditional float32 parameters, and efficiently
    allows them to be updated and for changed ones to be iterated.
*/
struct ParameterStateList
{
    ParameterStateList() = default;

    using GetRampLengthForSparseStreamFn = std::function<uint32_t(const EndpointDetails&)>;

    template <typename PerformerOrSession>
    void initialise (PerformerOrSession& p, GetRampLengthForSparseStreamFn&& getRampLengthForSparseStreamFn)
    {
        valueHolder = choc::value::createFloat32 (0);

        rampedValueHolder = createWrappedSparseStreamEventHolder (0.0f, 0);
        rampFramesMember = rampedValueHolder.getObjectMemberAt (0).value;
        rampTargetMember = rampedValueHolder.getObjectMemberAt (1).value;

        {
            auto params = getInputEndpointsOfType (p, InputEndpointType::parameter);

            parameters.clear();
            parameters.reserve (params.size());

            for (auto& parameterInput : params)
            {
                Parameter param;
                param.endpoint = p.getEndpointHandle (parameterInput.endpointID);

                if (isStream (parameterInput))
                {
                    SOUL_ASSERT (getRampLengthForSparseStreamFn != nullptr);
                    param.rampFrames = getRampLengthForSparseStreamFn (parameterInput);
                }

                parameters.push_back (param);
            }
        }

        std::vector<Parameter*> parameterPtrs;
        parameterPtrs.reserve (parameters.size());

        for (auto& param : parameters)
            parameterPtrs.push_back (std::addressof (param));

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

    /** Pushes events for any endpoints which have had their value
        modified by setParameter() or markAsChanged().
    */
    bool addToFIFO (MultiEndpointFIFO& fifo, uint64_t time)
    {
        while (auto p = dirtyList.popNextDirtyObject())
        {
            if (p->rampFrames == 0)
            {
                valueHolder.getViewReference().set (p->currentValue);

                if (! fifo.addInputData (p->endpoint, time, valueHolder))
                    return false;
            }
            else
            {
                rampFramesMember.set (static_cast<int32_t> (p->rampFrames));
                rampTargetMember.set (p->currentValue);

                if (! fifo.addInputData (p->endpoint, time, rampedValueHolder))
                    return false;
            }
        }

        return true;
    }

private:
    struct Parameter
    {
        choc::fifo::DirtyList<Parameter>::Handle dirtyListHandle = {};
        soul::EndpointHandle endpoint;
        float currentValue = 0;
        uint32_t rampFrames = 0;
    };

    std::vector<Parameter> parameters;
    choc::fifo::DirtyList<Parameter> dirtyList;
    choc::value::Value valueHolder, rampedValueHolder;
    choc::value::ValueView rampFramesMember, rampTargetMember;
};


//==============================================================================
struct TimelineEventEndpointList
{
    TimelineEventEndpointList() = default;

    void clear()
    {
        timeSigHandle = {};
        tempoHandle = {};
        transportHandle = {};
        positionHandle = {};

        anyChanges = false;
        sendTimeSig = false;
        sendTempo = false;
        sendTransport = false;
        sendPosition = false;
    }

    void initialise (Performer& p)
    {
        clear();

        for (auto& e : getInputEndpointsOfType (p, InputEndpointType::event))
        {
            if (e.dataTypes.size() == 1)
            {
                auto& type = e.dataTypes.front();

                if (type.isObject())
                {
                    if (TimelineEvents::isTimeSig (type))    timeSigHandle   = p.getEndpointHandle (e.endpointID);
                    if (TimelineEvents::isTempo (type))      tempoHandle     = p.getEndpointHandle (e.endpointID);
                    if (TimelineEvents::isTransport (type))  transportHandle = p.getEndpointHandle (e.endpointID);
                    if (TimelineEvents::isPosition (type))   positionHandle  = p.getEndpointHandle (e.endpointID);
                }
            }
        }
    }

    void applyNewTimeSignature (TimeSignature newTimeSig)
    {
        if (timeSigHandle.isValid())
        {
            newTimeSigValue.getObjectMemberAt (0).value.set (static_cast<int32_t> (newTimeSig.numerator));
            newTimeSigValue.getObjectMemberAt (1).value.set (static_cast<int32_t> (newTimeSig.denominator));
            sendTimeSig = true;
            anyChanges = true;
        }
    }

    void applyNewTempo (float newBPM)
    {
        if (tempoHandle.isValid())
        {
            newTempoValue.getObjectMemberAt (0).value.set (newBPM);
            sendTempo = true;
            anyChanges = true;
        }
    }

    void applyNewTransportState (TransportState newState)
    {
        if (transportHandle.isValid())
        {
            newTransportValue.getObjectMemberAt (0).value.set (static_cast<int32_t> (newState));
            sendTransport = true;
            anyChanges = true;
        }
    }

    void applyNewTimelinePosition (TimelinePosition newPosition)
    {
        if (positionHandle.isValid())
        {
            newPositionValue.getObjectMemberAt (0).value.set (newPosition.currentFrame);
            newPositionValue.getObjectMemberAt (1).value.set (newPosition.currentQuarterNote);
            newPositionValue.getObjectMemberAt (2).value.set (newPosition.lastBarStartQuarterNote);
            sendPosition = true;
            anyChanges = true;
        }
    }

    bool addToFIFO (MultiEndpointFIFO& fifo, uint64_t time)
    {
        if (anyChanges)
        {
            anyChanges = false;

            if (sendTimeSig)
            {
                sendTimeSig = false;

                if (! fifo.addInputData (timeSigHandle, time, newTimeSigValue))
                    return false;
            }

            if (sendTempo)
            {
                sendTempo = false;

                if (! fifo.addInputData (tempoHandle, time, newTempoValue))
                    return false;
            }

            if (sendTransport)
            {
                sendTransport = false;

                if (! fifo.addInputData (transportHandle, time, newTransportValue))
                    return false;
            }

            if (sendPosition)
            {
                sendPosition = false;

                if (! fifo.addInputData (positionHandle, time, newPositionValue))
                    return false;
            }
        }

        return true;
    }

    EndpointHandle timeSigHandle, tempoHandle, transportHandle, positionHandle;

    bool anyChanges = false;
    bool sendTimeSig = false;
    bool sendTempo = false;
    bool sendTransport = false;
    bool sendPosition = false;

    choc::value::Value newTimeSigValue   { TimelineEvents::createTimeSigValue() };
    choc::value::Value newTempoValue     { TimelineEvents::createTempoValue() };
    choc::value::Value newTransportValue { TimelineEvents::createTransportValue() };
    choc::value::Value newPositionValue  { TimelineEvents::createPositionValue() };
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
        audioInputList.initialise (maxInternalBlockSize);
        audioOutputList.clear();
        midiInputList.clear();
        midiOutputList.clear();
        timelineEventEndpointList.clear();
        eventOutputList.clear();
        inputFIFO.reset (256 * maxInternalBlockSize, maxInternalBlockSize * 2);
        maxBlockSize = 0;
    }

    std::vector<EndpointDetails> getAudioInputEndpoints()       { return getInputEndpointsOfType (performer, InputEndpointType::audio); }
    std::vector<EndpointDetails> getParameterEndpoints()        { return getInputEndpointsOfType (performer, InputEndpointType::parameter); }
    std::vector<EndpointDetails> getEventInputEndpoints()       { return getInputEndpointsOfType (performer, InputEndpointType::event); }

    std::vector<EndpointDetails> getAudioOutputEndpoints()      { return getOutputEndpointsOfType (performer, OutputEndpointType::audio); }
    std::vector<EndpointDetails> getEventOutputEndpoints()      { return getOutputEndpointsOfType (performer, OutputEndpointType::event); }

    void prepare (uint32_t processorMaxBlockSize,
                  ParameterStateList::GetRampLengthForSparseStreamFn&& getRampLengthForSparseStreamFn)
    {
        reset();
        auto& perf = performer;
        SOUL_ASSERT (processorMaxBlockSize > 0);
        maxBlockSize = std::min (maxInternalBlockSize, processorMaxBlockSize);

        audioInputList.attachToAllAudioEndpoints (perf);
        audioOutputList.initialise (perf);
        midiInputList.initialise (perf);
        midiOutputList.initialise (perf);
        parameterList.initialise (perf, std::move (getRampLengthForSparseStreamFn));
        timelineEventEndpointList.initialise (perf);
        eventOutputList.initialise (perf);
    }

    void render (choc::buffer::ChannelArrayView<const float> input,
                 choc::buffer::ChannelArrayView<float> output,
                 MIDIEventInputList midiIn,
                 MIDIEventOutputList& midiOut)
    {
        auto numFrames = output.getNumFrames();
        output.clear();

        if (numFrames > maxInternalBlockSize)
            return renderInChunks (input, output, midiIn, midiOut);

        SOUL_ASSERT (input.getNumFrames() == numFrames && maxBlockSize != 0);

        audioInputList.addToFIFO (inputFIFO, totalFramesRendered, input);
        midiInputList.addToFIFO (inputFIFO, totalFramesRendered, midiIn);
        parameterList.addToFIFO (inputFIFO, totalFramesRendered);
        timelineEventEndpointList.addToFIFO (inputFIFO, totalFramesRendered);
        uint32_t framesDone = 0;

        inputFIFO.prepareForReading (totalFramesRendered, numFrames);

        for (;;)
        {
            auto numFramesToDo = inputFIFO.getNumFramesInNextChunk (maxBlockSize);

            if (numFramesToDo == 0)
                break;

            performer.prepare (numFramesToDo);
            inputFIFO.processNextChunk ([&] (EndpointHandle endpoint, uint64_t /*itemStart*/, const choc::value::ValueView& value)
                                        {
                                            deliverValueToEndpoint (endpoint, value);
                                        });
            performer.advance();
            audioOutputList.handleOutputData (performer, output.getFrameRange ({ framesDone, output.size.numFrames }));
            midiOutputList.handleOutputData (performer, framesDone, midiOut);
            eventOutputList.postOutputEvents (performer, totalFramesRendered + framesDone);
            framesDone += numFramesToDo;
        }

        inputFIFO.finishReading();
        totalFramesRendered += framesDone;
    }

    uint32_t getExpectedNumInputChannels() const     { return audioInputList.totalNumChannels; }
    uint32_t getExpectedNumOutputChannels() const    { return audioOutputList.totalNumChannels; }

    bool postInputEvent (EndpointHandle endpoint, const choc::value::ValueView& value)
    {
        return inputFIFO.addInputData (endpoint, totalFramesRendered, value);
    }

    template <typename HandleEventFn>
    void deliverOutgoingEvents (HandleEventFn&& handleEvent)
    {
        eventOutputList.deliverPendingEvents (handleEvent);
    }

    Performer& performer;
    ParameterStateList parameterList;
    TimelineEventEndpointList timelineEventEndpointList;
    uint64_t totalFramesRendered = 0;

private:
    //==============================================================================
    MultiEndpointFIFO inputFIFO;

    AudioInputList   audioInputList;
    MIDIInputList    midiInputList;
    AudioOutputList  audioOutputList;
    MIDIOutputList   midiOutputList;
    EventOutputList  eventOutputList;

    uint32_t maxBlockSize = 0;

    static constexpr uint32_t maxInternalBlockSize = 512;

    void renderInChunks (choc::buffer::ChannelArrayView<const float> input,
                         choc::buffer::ChannelArrayView<float> output,
                         MIDIEventInputList midiIn,
                         MIDIEventOutputList& midiOut)
    {
        auto numFramesRemaining = output.getNumFrames();

        for (uint32_t start = 0;;)
        {
            auto framesToDo = std::min (numFramesRemaining, maxInternalBlockSize);
            auto endFrame = start + framesToDo;

            render (input.getFrameRange ({ start, endFrame }),
                    output.getFrameRange ({ start, endFrame }),
                    midiIn.removeEventsBefore (endFrame), midiOut);

            if (numFramesRemaining <= framesToDo)
                break;

            start += framesToDo;
            numFramesRemaining -= framesToDo;
        }
    }

    void deliverValueToEndpoint (EndpointHandle endpoint, const choc::value::ValueView& value)
    {
        switch (endpoint.getType())
        {
            case EndpointType::stream:
                if (! applySparseStreamEventWrapper (performer, endpoint, value))
                    performer.setNextInputStreamFrames (endpoint, value);

                break;

            case EndpointType::event:
                performer.addInputEvent (endpoint, value);
                break;

            case EndpointType::value:
                performer.setInputValue (endpoint, value);
                break;

            case EndpointType::unknown:
            default:
                SOUL_ASSERT_FALSE;
        }
    }
};

}
