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
/** A set of lambda prototypes for functions that are used by InputEndpoint and
    OutputEndpoint to fill and empty various types of endpoints in a SOUL processor.
*/
namespace callbacks
{
    /** Used as a parameter to the FillEventBuffer callback function.
        @param eventData    the data to copy into the event object
    */
    using PostNextEvent = const std::function<void(const void* eventData)>&;

    /** This is called to provide the next block of events for an event input endpoint.

        @param totalFramesElapsed  a continuously-increasing counter of the total number of
                                   frames the processor has processed
        @param numFrames           the number of frames in the block that is being processed
        @param postEvent           a lambda that the callback should call to post its events

        @returns the maximum number of frames which can elapse before the callback will
        need to be called again. This value must be greater than zero, and if there are
        no more known future events, you may just want to return a sensible default
        value such as 1024.
    */
    using FillEventBuffer = std::function<uint32_t(uint64_t totalFramesElapsed, uint32_t numFrames, PostNextEvent postEvent)>;

    /** Request to fill the given buffer with the number of requested frames.
        @returns the number of frames actually provided
    */
    using FillStreamBuffer = std::function<uint32_t(void* frameData, uint32_t numFramesRequested)>;

    /** Called when an output endpoint has generated an event which needs to be consumed.
        @param eventData        a pointer to the raw bytes of the event value
        @param eventSize        the size of the event object, in bytes
        @param eventFrameTime   the timestamp of the event, as a count of the number of frames since processing began
        @returns true if it was successfully consumed
    */
    using ConsumeNextEvent = std::function<bool(const void* eventData, uint32_t eventSize, uint64_t eventFrameTime)>;

    /** Called to consume the given number of frames from the given buffer.
        @returns the number of frames consumed
    */
    using ConsumeStreamData = std::function<uint32_t(const void* frameData, uint32_t numFramesAvailable)>;

    /** This is called to set a target value for a sparse stream, along with the number of frames
        that should be taken to reach the given value, and the shape to use whilst moving to this value.
        @param targetValue              the value to aim for
        @param numFramesToReachValue    how many frames to take to get there
        @param curveShape               the type of curve to use to reach the target, where 0 is linear,
                                        and positive/negative values (up to 1) are logarithmically skewed
    */
    using SetSparseStreamTarget = const std::function<void(const void* targetValue, uint32_t numFramesToReachValue, float curveShape)>&;

    /** This is called to provide the next block of sparse stream data.
        @param totalFramesElapsed  a continuously-increasing counter of the total number of
                                   frames the processor has processed
        @param setTargetValue      a lambda that this callback should use if it needs to set a new
                                   target value for the stream to head towards
    */
    using FillSparseStreamBuffer = std::function<uint32_t(uint64_t totalFramesElapsed, SetSparseStreamTarget setTargetValue)>;
}

//==============================================================================
/**
*/
class InputEndpoint  : public RefCountedObject
{
public:
    InputEndpoint() {}
    virtual ~InputEndpoint() {}

    using Ptr = RefCountedPtr<InputEndpoint>;

    /** Returns the basic details about this endpoint */
    virtual const EndpointDetails& getDetails() = 0;

    /** If the endpoint is a value (i.e. EndpointKind::value) then this method will update its
        current value. This can be called at any time, including after the performer has been
        linked and is running, and should be thread-safe to be called concurrently with advance().
        For any other type of endpoint, it's not permitted to call this.
    */
    virtual bool setCurrentValue (const void* newData, uint32_t dataSize) = 0;

    /** If the endpoint is a stream (i.e. EndpointKind::stream) then this method will attach
        a callback that can provide it with the data it needs, and also define the properties
        of the stream. For any other type of endpoint, it's not permitted to call this.
        This must only be called after the performer has been loaded, and before the performer
        has been linked. Once linked, the callback and properties must not be changed without
        re-linking the program.
    */
    virtual bool setStreamSource (callbacks::FillStreamBuffer&&, EndpointProperties) = 0;

    /** If the endpoint is a stream (i.e. EndpointKind::stream) then this method will attach
        a callback that can provide it with sparse data, and also define the properties
        of the stream. For any other type of endpoint, it's not permitted to call this.
        This must only be called after the performer has been loaded, and before the performer
        has been linked. Once linked, the callback and properties must not be changed without
        re-linking the program.
    */
    virtual bool setSparseStreamSource (callbacks::FillSparseStreamBuffer&&, EndpointProperties) = 0;

    /** If the endpoint is an event stream (i.e. EndpointKind::event) then this method will attach
        a callback that can provide it with the incoming events, and also define the properties
        of the stream. For any other type of endpoint, it's not permitted to call this.
        This must only be called after the performer has been loaded, and before the performer
        has been linked. Once linked, the callback and properties must not be changed without
        re-linking the program.
    */
    virtual bool setEventSource (callbacks::FillEventBuffer&&, EndpointProperties) = 0;

    /** Remove any callback that is currently attached. */
    virtual void removeSource() = 0;

    /** Returns true if a callback is currently attached to this endpoint. */
    virtual bool isActive() = 0;
};

//==============================================================================
/**
*/
class OutputEndpoint  : public RefCountedObject
{
public:
    OutputEndpoint() {}
    virtual ~OutputEndpoint() {}

    using Ptr = RefCountedPtr<OutputEndpoint>;

    /** Returns the basic details about this endpoint */
    virtual const EndpointDetails& getDetails() = 0;

    /** If the endpoint is a value (i.e. EndpointKind::value) then this method will return its
        current value. This can be called at any time, including after the performer has been
        linked and is running, and should be thread-safe to be called concurrently with advance().
        For any other type of endpoint, it's not permitted to call this.
    */
    virtual bool getCurrentValue (void* destBuffer, uint32_t size) = 0;

    /** If the endpoint is a stream (i.e. EndpointKind::stream) then this method will attach
        a callback that will consume the data being produced, and also define the properties
        of the stream. For any other type of endpoint, it's not permitted to call this.
        This must only be called after the performer has been loaded, and before the performer
        has been linked. Once linked, the callback and properties must not be changed without
        re-linking the program.
    */
    virtual bool setStreamSink (callbacks::ConsumeStreamData&&, EndpointProperties) = 0;

    /** If the endpoint is an event stream (i.e. EndpointKind::event) then this method will attach
        a callback that will consume the events that are being produced, and also define the properties
        of the stream. For any other type of endpoint, it's not permitted to call this.
        This must only be called after the performer has been loaded, and before the performer
        has been linked. Once linked, the callback and properties must not be changed without
        re-linking the program.
    */
    virtual bool setEventSink (callbacks::ConsumeNextEvent&&, EndpointProperties) = 0;

    /** Remove any callback that is currently attached. */
    virtual void removeSink() = 0;

    /** Returns true if a sink is attached to this endpoint. */
    virtual bool isActive() = 0;
};


//==============================================================================
template <typename VenueOrPerformer>
InputEndpoint::Ptr findFirstInputOfType (VenueOrPerformer& session, EndpointKind kind)
{
    for (auto& i : session.getInputEndpoints())
        if (i->getDetails().kind == kind)
            return i;

    return {};
}

template <typename VenueOrPerformer>
OutputEndpoint::Ptr findFirstOutputOfType (VenueOrPerformer& session, EndpointKind kind)
{
    for (auto& o : session.getOutputEndpoints())
        if (o->getDetails().kind == kind)
            return o;

    return {};
}

inline bool isMIDIEventEndpoint (const EndpointDetails& details)
{
    auto isMIDIMessageStruct = [] (const Structure& s)
    {
        return s.name == "Message"
            && s.members.size() == 1
            && s.members.front().name == "midiBytes"
            && s.members.front().type.isPrimitive()
            && s.members.front().type.isInteger32();
    };

    return isEvent (details.kind)
            && details.sampleType.isStruct()
            && isMIDIMessageStruct (details.sampleType.getStructRef());
}

inline bool isMIDIEventEndpoint (InputEndpoint& i)  { return isMIDIEventEndpoint (i.getDetails()); }
inline bool isMIDIEventEndpoint (OutputEndpoint& o) { return isMIDIEventEndpoint (o.getDetails()); }

inline bool isParameterInput (const EndpointDetails& details)
{
    if (isEvent (details.kind) && ! isMIDIEventEndpoint (details))
        return true;

    if (isStream (details.kind) && details.annotation.hasValue ("name"))
        return true;

    return false;
}

inline bool isParameterInput (InputEndpoint& input)
{
    return isParameterInput (input.getDetails());
}

} // namespace soul
