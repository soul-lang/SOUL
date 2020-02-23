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
/** A FIFO for holding time-stamped event objects.

    TimestampType could be a uint64_t or a std::atomic<uint64_t> depending
    on whether atomicity is needed.
*/
template <typename TimestampType>
struct EventFIFO
{
    EventFIFO (const Type& type) : eventType (type)
    {
        auto emptyValue = Value::zeroInitialiser (eventType);

        events.resize (capacity);

        for (uint32_t i = 0; i < capacity; i++)
        {
            events[i].time  = 0;
            events[i].value = emptyValue;
        }
    }

    using TimeType = TimestampType;

    struct Event
    {
        uint64_t time;
        soul::Value value;
    };

    Event& getEvent (uint64_t pos) noexcept    { return events[pos % capacity]; }

    void pushEvent (uint64_t eventTime, const soul::Value& value) noexcept
    {
        SOUL_ASSERT (value.getType().isIdentical(eventType));

        auto& e = getEvent (writePos);
        e.time = eventTime;
        e.value.copyValue (value);

        ++writePos;
    }

    void pushEvents (uint64_t eventTime, const soul::Value* eventsToAdd, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
            pushEvent (eventTime, eventsToAdd[i]);
    }

    static constexpr uint32_t capacity = 1024;
    std::vector<Event> events;
    TimeType readPos { 0 }, writePos { 0 };
    Type eventType;
};

//==============================================================================
/** Handles the queuing of time-stamped event objects and sending them to an InputEndpoint.
    FIFOType needs to be of type EventFIFO
*/
template <typename FIFOType>
struct InputEventQueue
{
    InputEventQueue (const Type& eventType, InputSource& stream, const EndpointDetails& details, EndpointProperties endpointProperties)
        : fifo (eventType), inputStream (stream)
    {
        SOUL_ASSERT (isEvent (details.kind)); ignoreUnused (details);

        inputStream->setEventSource ([this] (uint64_t currentTime, uint32_t blockLength, callbacks::PostNextEvent postEvent)
                                     {
                                         return dispatchNextEvents (currentTime, blockLength, postEvent);
                                     },
                                     endpointProperties);
    }

    ~InputEventQueue()
    {
        inputStream->setEventSource (nullptr, {});
        inputStream.reset();
    }

    void enqueueEvent (uint32_t offset, const Value& value)
    {
        fifo.pushEvent (currentBlockTime + offset, value);
    }

    uint32_t dispatchNextEvents (uint64_t currentTime, uint32_t currentBlockSize, callbacks::PostNextEvent postEvent)
    {
        const auto blockEndTime = currentTime + currentBlockSize;
        const uint64_t writePosSnapshot = fifo.writePos;

        while (fifo.readPos < writePosSnapshot)
        {
            const auto& e = fifo.getEvent (fifo.readPos);

            if (e.time > currentTime)
                break;

            postEvent (e.value);
            ++(fifo.readPos);
        }

        if (fifo.readPos < writePosSnapshot)
        {
            auto nextEventTime = fifo.getEvent (fifo.readPos).time;

            if (nextEventTime < blockEndTime)
            {
                auto samplesToAdvance = static_cast<uint32_t> (nextEventTime - currentTime);
                currentBlockTime = currentTime + samplesToAdvance;
                return samplesToAdvance;
            }
        }

        currentBlockTime = currentTime + currentBlockSize;
        return currentBlockSize;
    }

    FIFOType fifo;
    InputSource::Ptr inputStream;
    typename FIFOType::TimeType currentBlockTime { 0 };
};


//==============================================================================
/** Reads blocks of time-stamped event objects from an OutputEndpoint.
    FIFOType needs to be of type EventFIFO
*/
template <typename FIFOType>
struct OutputEventQueue
{
    OutputEventQueue (Type eventType, OutputSink& stream, const EndpointDetails& details, EndpointProperties endpointProperties)
        : fifo (eventType), outputStream (stream)
    {
        SOUL_ASSERT (isEvent (details.kind)); ignoreUnused (details);

        outputStream->setEventSink ([this] (uint64_t eventFrameTime, const soul::Value& value)
                                    {
                                        enqueueEvent (value.getType(), value.getPackedData(), static_cast<uint32_t> (value.getPackedDataSize()), eventFrameTime);
                                    },
                                    endpointProperties);

        eventValue = Value::zeroInitialiser (eventType);
    }

    ~OutputEventQueue()
    {
        outputStream->setEventSink (nullptr, {});
        outputStream.reset();
    }

    bool enqueueEvent (const soul::Type&, const void* eventData, uint32_t eventSize, uint64_t eventFrameTime)
    {
        SOUL_ASSERT (eventSize == eventValue.getPackedDataSize());

        memcpy (eventValue.getPackedData(), eventData, eventSize);
        fifo.pushEvent (eventFrameTime, eventValue);
        return true;
    }

    template <typename HandleEvent>
    void readNextEvents (uint32_t numFrames, HandleEvent&& handleEvent)
    {
        const uint64_t blockStartTime = currentBlockTime;
        const auto blockEndTime = blockStartTime + numFrames;
        const uint64_t writePosSnapshot = fifo.writePos;

        while (fifo.readPos < writePosSnapshot)
        {
            const auto& e = fifo.getEvent (fifo.readPos);

            if (e.time > blockEndTime)
                break;

            handleEvent (e.time > blockStartTime ? (uint32_t) (e.time - blockStartTime) : 0, e.value);
            ++(fifo.readPos);
        }

        currentBlockTime = blockEndTime;
    }

    FIFOType fifo;
    OutputSink::Ptr outputStream;
    typename FIFOType::TimeType currentBlockTime { 0 };
    Value eventValue;
};

}
