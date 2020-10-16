
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

#pragma once

namespace soul
{

//==============================================================================
/**
    Manages a FIFO containing a set of data chunks being sent to or from endpoints.
*/
struct MultiEndpointFIFO
{
    MultiEndpointFIFO()      { reset(); }
    ~MultiEndpointFIFO() = default;

     void reset()
    {
        fifo.reset (fifoSize);
        incomingItems.resize (maxIncomingItems);
    }

    bool addInputData (soul::EndpointHandle endpoint,
                       uint64_t time,
                       const choc::value::ValueView& value)
    {
        ScratchWriter scratch;

        scratch.write (std::addressof (time), sizeof (time));
        scratch.write (std::addressof (endpoint), sizeof (endpoint));

        auto& type = value.getType();
        type.serialise (scratch);

        if (! type.isVoid())
            scratch.write (value.getRawData(), type.getValueDataSize());

        return ! scratch.overflowed()
                && fifo.push (scratch.space, scratch.total);
    }

    template <typename Handler>
    void iterateChunks (uint32_t numFrames, uint64_t startFrameNumber, Handler&& handler)
    {
        uint32_t numItems = 0;

        fifo.popAllAvailable ([&] (const void* data, uint32_t size)
                              {
                                  auto d = static_cast<const uint8_t*> (data);

                                  if (numItems < maxIncomingItems)
                                      if (readIncomingItem (incomingItems[numItems], { d, d + size }, startFrameNumber))
                                          ++numItems;
                              },
                              [&]
                              {
                                  processChunks (incomingItems.data(), numItems, numFrames, handler);
                              });
    }

private:
    static constexpr uint32_t maxItemSize = 8192;
    static constexpr uint32_t fifoSize = 256 * 1024;
    static constexpr uint32_t maxIncomingItems = 2048;

    choc::fifo::VariableSizeFIFO fifo;

    struct Item
    {
        uint32_t startFrame, numFrames;
        soul::EndpointHandle endpoint;
        choc::value::ValueView value;
    };

    std::vector<Item> incomingItems;

    struct ScratchWriter
    {
        char space[maxItemSize];
        char* dest = space;
        uint32_t total = 0;

        bool overflowed() const      { return total > sizeof (space); }

        void write (const void* source, size_t size)
        {
            total += size;

            if (! overflowed())
            {
                std::memcpy (dest, source, size);
                dest += size;
            }
        }
    };

    template <typename DestType> void read (choc::value::InputData& reader, DestType& d)
    {
        if (reader.start + sizeof (d) > reader.end)
            throw choc::value::Error { nullptr };

        std::memcpy (std::addressof (d), reader.start, sizeof (d));
        reader.start += sizeof (d);
    }

    bool readIncomingItem (Item& item, choc::value::InputData reader, uint64_t startFrameNumber)
    {
        try
        {
            uint64_t time;
            read (reader, time);

            if (time >= startFrameNumber)
            {
                item.startFrame = static_cast<uint32_t> (time - startFrameNumber);
                read (reader, item.endpoint);

                auto type = choc::value::Type::deserialise (reader);

                if (reader.start + type.getValueDataSize() <= reader.end)
                {
                    item.value = choc::value::ValueView (std::move (type), const_cast<uint8_t*> (reader.start), nullptr);

                    if (item.value.isArray())
                        item.numFrames = item.value.getType().getNumElements();
                    else
                        item.numFrames = 1;

                    return true;
                }
            }
        }
        catch (const choc::value::Error&) {}

        return false;
    }

    uint32_t findOffsetOfNextItemAfter (const Item* items, uint32_t numItems, uint32_t startFrame, uint32_t endFrame)
    {
        auto lowest = endFrame;

        for (uint32_t i = 0; i < numItems; ++i)
        {
            auto frame = items[i].startFrame;

            if (frame < lowest && frame > startFrame)
                lowest = frame;
        }

        return lowest;
    }

    template <typename Handler>
    void processChunks (Item* items, uint32_t numItems, uint32_t totalFrames, Handler&& handler)
    {
        for (uint32_t frame = 0; frame < totalFrames;)
        {
            auto nextChunkStart = findOffsetOfNextItemAfter (items, numItems, frame, totalFrames);

            for (uint32_t i = 0; i < numItems; ++i)
            {
                auto& item = items[i];
                auto itemStart = item.startFrame;
                auto itemEnd = itemStart + item.numFrames;

                if (itemEnd > frame && itemStart < nextChunkStart)
                {
                    if (item.numFrames != 1)
                    {
                        if (itemStart < frame)
                        {
                            auto amountToTrim = frame - itemStart;
                            item.value = item.value.getElementRange (amountToTrim, item.numFrames - amountToTrim);
                            itemStart = frame;
                            item.numFrames -= amountToTrim;
                        }

                        if (itemEnd > nextChunkStart)
                            handler (item.endpoint, itemStart, item.value.getElementRange (0, nextChunkStart - itemStart));
                        else
                            handler (item.endpoint, itemStart, item.value);
                    }
                    else
                    {
                        handler (item.endpoint, itemStart, item.value);
                    }
                }
            }

            frame = nextChunkStart;
        }
    }
};

} // namespace soul
