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

}
