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
template <typename EventObject, typename TimestampType>
struct EventFIFO
{
    EventFIFO()
    {
        events.resize (capacity);
    }

    using EventType = EventObject;
    using TimeType = TimestampType;

    struct Event
    {
        uint64_t time;
        EventType value;
    };

    Event& getEvent (uint64_t pos) noexcept    { return events[pos % capacity]; }

    void pushEvent (uint64_t eventTime, EventType value) noexcept
    {
        auto& e = getEvent (writePos);
        e.time = eventTime;
        e.value = value;
        ++writePos;
    }

    void pushEvents (uint64_t eventTime, const EventType* p, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
            pushEvent (eventTime, p[i]);
    }

    static constexpr uint32_t capacity = 1024;
    std::vector<Event> events;
    TimeType readPos { 0 }, writePos { 0 };
};

//==============================================================================
/** Handles the queuing of time-stamped event objects and sending them to an InputEndpoint.
    FIFOType needs to be of type EventFIFO
*/
template <typename FIFOType>
struct InputEventQueue
{
    InputEventQueue (InputEndpoint& stream, EndpointProperties endpointProperties)
        : inputStream (stream)
    {
        SOUL_ASSERT (isEvent (stream.getDetails().kind));

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

    void enqueueEvent (uint32_t offset, typename FIFOType::EventType value)
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

            postEvent (std::addressof (e.value));
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

    InputEndpoint::Ptr inputStream;
    FIFOType fifo;
    typename FIFOType::TimeType currentBlockTime { 0 };
};


//==============================================================================
/** Reads blocks of time-stamped event objects from an OutputEndpoint.
    FIFOType needs to be of type EventFIFO
*/
template <typename FIFOType>
struct OutputEventQueue
{
    OutputEventQueue (OutputEndpoint& stream, EndpointProperties endpointProperties)
        : outputStream (stream)
    {
        SOUL_ASSERT (isEvent (stream.getDetails().kind));

        outputStream->setEventSink ([this] (const void* eventData, uint32_t eventSize, uint64_t eventFrameTime)
                                    {
                                        return enqueueEvent (eventData, eventSize, eventFrameTime);
                                    },
                                    endpointProperties);
    }

    ~OutputEventQueue()
    {
        outputStream->setEventSink (nullptr, {});
        outputStream.reset();
    }

    bool enqueueEvent (const void* eventData, uint32_t eventSize, uint64_t eventFrameTime)
    {
        typename FIFOType::EventType value;
        SOUL_ASSERT (eventSize == sizeof (value));
        memcpy (std::addressof (value), eventData, sizeof (value));
        fifo.pushEvent (eventFrameTime, value);
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

    OutputEndpoint::Ptr outputStream;
    FIFOType fifo;
    typename FIFOType::TimeType currentBlockTime { 0 };
};

}
