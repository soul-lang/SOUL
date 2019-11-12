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
/** An atomic FIFO for posting and receiving time-stamped event objects.
*/
template <class EventType>
struct EventQueue
{
    EventQueue (InputEndpoint::Ptr stream, EndpointProperties endpointProperties)
        : inputStream (stream), blockSize (endpointProperties.blockSize)
    {
        SOUL_ASSERT (isEvent (inputStream->getDetails().kind));
        queue.resize (queueLength);

        inputStream->setEventSource ([this] (uint64_t currentTime, uint32_t blockLength, callbacks::PostNextEvent postEvent)
                                     {
                                         return dispatchNextEvents (currentTime, blockLength, postEvent);
                                     },
                                     endpointProperties);
    }

    ~EventQueue()
    {
        inputStream->setEventSource (nullptr, {});
        inputStream.reset();
    }

    struct Event
    {
        uint64_t time;
        EventType value;
    };

    void enqueueEvent (uint32_t offset, EventType value)
    {
        auto& e = getEvent (writePos);
        e.time = time + offset;
        e.value = value;
        ++writePos;
    }

    void enqueueEvents (uint32_t offset, const EventType* p, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
            enqueueEvent (offset, p[i]);
    }

    uint32_t dispatchNextEvents (uint64_t currentTime, uint32_t currentBlockSize, callbacks::PostNextEvent postEvent)
    {
        auto blockEndTime = currentTime + currentBlockSize;

        // Catch the writePos as we want to dispatch any events present up to this point only
        auto writePosSnapshot = writePos.load();

        // Dispatch any events for this time
        while (readPos < writePosSnapshot)
        {
            const auto& e = getEvent (readPos);

            if (e.time > currentTime)
                break;

            postEvent (std::addressof (e.value));
            ++readPos;
        }

        if (readPos < writePosSnapshot)
        {
            auto nextEventTime = getEvent (readPos).time;

            if (nextEventTime < blockEndTime)
            {
                auto samplesToAdvance = static_cast<uint32_t> (nextEventTime - currentTime);

                time = currentTime + samplesToAdvance;
                return samplesToAdvance;
            }
        }

        time = currentTime + currentBlockSize;
        return currentBlockSize;
    }

    InputEndpoint::Ptr inputStream;
    std::atomic<uint64_t> time { 0 };
    std::vector<Event> queue;

private:
    uint32_t blockSize;

    static constexpr uint32_t queueLength = 1024;
    std::atomic<uint64_t> readPos { 0 }, writePos { 0 };

    Event& getEvent (uint64_t pos) noexcept    { return queue[pos % queueLength]; }
};

}
