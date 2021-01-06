
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
    MultiEndpointFIFO()
    {
        incomingItemAllocator = std::make_unique<choc::value::FixedPoolAllocator<incomingItemAllocationSpace>>();
        reset (256 * 1024, 2048);
    }

    ~MultiEndpointFIFO() = default;

    void reset (uint32_t fifoSize, uint32_t maxNumPendingItems)
    {
        fifoBatchReadOp.release();

        fifo.reset (fifoSize);
        itemPool.resize (maxNumPendingItems);

        pendingItems.clear();
        pendingItems.reserve (maxNumPendingItems);
        freeItems.clear();
        freeItems.reserve (maxNumPendingItems);

        for (auto& i : itemPool)
            freeItems.push_back (std::addressof (i));
    }

    bool addInputData (soul::EndpointHandle endpoint, uint64_t time,
                       const choc::value::ValueView& value)
    {
        ScratchWriter scratch;

        scratch.write (std::addressof (time), sizeof (time));
        scratch.write (std::addressof (endpoint), sizeof (endpoint));

        try
        {
            value.serialise (scratch);
        }
        catch (choc::value::Error)
        {
            return false;
        }

        return fifo.push (scratch.space, scratch.total);
    }

    //==============================================================================
    bool prepareForReading (uint64_t startFrameNumber, uint32_t numFramesNeeded)
    {
        incomingItemAllocator->reset();
        bool success = true;

        if (! fifoBatchReadOp.isActive())
            fifoBatchReadOp = choc::fifo::VariableSizeFIFO::BatchReadOperation (fifo);

        while (fifoBatchReadOp.pop ([&] (const void* data, uint32_t size)
                                    {
                                        auto d = static_cast<const uint8_t*> (data);

                                        if (! freeItems.empty())
                                        {
                                            auto item = freeItems.back();

                                            if (readIncomingItem (*item, { d, d + size }, startFrameNumber))
                                            {
                                                freeItems.pop_back();
                                                pendingItems.push_back (item);
                                                return;
                                            }
                                        }

                                        success = false;
                                    }))
        {
        }

        endFrame = startFrameNumber + numFramesNeeded;
        currentFrame = startFrameNumber;
        nextChunkStart = startFrameNumber;
        return success;
    }

    uint32_t getNumFramesInNextChunk (uint32_t maxNumFrames)
    {
        if (currentFrame >= endFrame)
            return 0;

        nextChunkStart = findOffsetOfNextItemAfter (currentFrame, std::min (endFrame, currentFrame + maxNumFrames));
        framesThisTime = static_cast<uint32_t> (nextChunkStart - currentFrame);
        return framesThisTime;
    }

    template <typename HandleItem>
    void processNextChunk (HandleItem&& handleItem)
    {
        for (auto* item : pendingItems)
        {
            auto itemStart = item->startFrame;
            auto numFrames = item->numFrames;
            auto itemEnd = itemStart + numFrames;

            if (itemEnd > currentFrame && itemStart < nextChunkStart)
            {
                bool keepItem = false;

                if (numFrames != 1)
                {
                    if (currentFrame > itemStart)
                    {
                        auto amountToTrim = static_cast<uint32_t> (currentFrame - itemStart);
                        item->value = item->value.getElementRange (amountToTrim, item->numFrames - amountToTrim);
                        itemStart = currentFrame;
                        item->numFrames -= amountToTrim;
                        item->startFrame += amountToTrim;
                    }

                    if (itemEnd > nextChunkStart)
                    {
                        handleItem (item->endpoint, itemStart, static_cast<const choc::value::ValueView&> (item->value.getElementRange (0, static_cast<uint32_t> (nextChunkStart - itemStart))));
                        keepItem = true;
                    }
                    else
                    {
                        handleItem (item->endpoint, itemStart, static_cast<const choc::value::ValueView&> (item->value));
                    }
                }
                else
                {
                    handleItem (item->endpoint, itemStart, static_cast<const choc::value::ValueView&> (item->value));
                }

                if (! keepItem)
                {
                    item->release();
                    freeItems.push_back (item);
                }
            }
        }

        removeIf (pendingItems, [] (const Item* i) { return i->numFrames == 0; });
        currentFrame = nextChunkStart;
    }

    template <typename HandleItem>
    void iterateAllPreparedItemsForHandle (EndpointHandle handle, HandleItem&& handleItem)
    {
        for (auto* item : pendingItems)
            if (item->endpoint == handle)
                handleItem (item->startFrame, static_cast<const choc::value::ValueView&> (item->value));
    }

    void finishReading()
    {
        if (pendingItems.empty())
            fifoBatchReadOp.release();
    }

    /** Note that these iterate functions may only be called by a single thread. */
    template <typename HandleItem>
    bool iterateAllAvailable (HandleItem&& handleItem)
    {
        bool success = true;
        incomingItemAllocator->reset();

        fifo.popAllAvailable ([&] (const void* data, uint32_t size)
                              {
                                  auto d = static_cast<const uint8_t*> (data);
                                  Item item;

                                  if (readIncomingItem (item, { d, d + size }, 0))
                                      handleItem (item.endpoint, item.startFrame, item.value);
                                  else
                                      success = false;
                              });

        return success;
    }

    static constexpr uint32_t maxItemSize = 4096;

private:
    struct SerialisedStringDictionary  : public choc::value::StringDictionary
    {
        Handle getHandleForString (std::string_view) override     { SOUL_ASSERT (false); return {}; }

        std::string_view getStringForHandle (Handle handle) const override
        {
            handle.handle--;
            if (handle.handle >= size)
                choc::value::throwError ("Malformed data");

            return std::string_view (start + handle.handle);
        }

        const char* start;
        size_t size;
    };

    struct Item
    {
        uint64_t startFrame = 0;
        uint32_t numFrames = 0;
        soul::EndpointHandle endpoint;
        choc::value::ValueView value;
        SerialisedStringDictionary dictionary;

        void release()
        {
            numFrames = 0;
            value = {};
        }
    };

    struct ScratchWriter
    {
        char space[maxItemSize];
        char* dest = space;
        uint32_t total = 0;

        void write (const void* source, size_t size)
        {
            total += static_cast<uint32_t> (size);

            if (total > sizeof (space))
                choc::value::throwError ("Out of scratch space");

            std::memcpy (dest, source, size);
            dest += size;
        }
    };

    choc::fifo::VariableSizeFIFO fifo;
    choc::fifo::VariableSizeFIFO::BatchReadOperation fifoBatchReadOp;

    static constexpr size_t incomingItemAllocationSpace = 65536;
    std::unique_ptr<choc::value::FixedPoolAllocator<incomingItemAllocationSpace>> incomingItemAllocator;

    std::vector<Item> itemPool;
    std::vector<Item*> pendingItems, freeItems;

    uint64_t currentFrame = 0, nextChunkStart = 0, endFrame = 0;
    uint32_t framesThisTime = 0;

    template <typename DestType> static void read (choc::value::InputData& reader, DestType& d)
    {
        if (reader.start + sizeof (d) > reader.end)
            choc::value::throwError ("Malformed data");

        std::memcpy (std::addressof (d), reader.start, sizeof (d));
        reader.start += sizeof (d);
    }

    static uint32_t readVariableLengthInt (choc::value::InputData& source)
    {
        uint32_t result = 0;

        for (int shift = 0;;)
        {
            if (source.end <= source.start)
                choc::value::throwError ("Malformed data");

            auto nextByte = *source.start++;

            if (shift == 28)
                if (nextByte >= 16)
                    choc::value::throwError ("Malformed data");

            if (nextByte < 128)
                return result | (static_cast<uint32_t> (nextByte) << shift);

            result |= static_cast<uint32_t> (nextByte & 0x7fu) << shift;
            shift += 7;
        }
    }

    bool readIncomingItem (Item& item, choc::value::InputData reader, uint64_t startFrameNumber)
    {
        try
        {
            uint64_t time;
            read (reader, time);

            if (time >= startFrameNumber)
            {
                item.startFrame = time;
                read (reader, item.endpoint);

                auto type = choc::value::Type::deserialise (reader, incomingItemAllocator.get());
                auto dataSize = type.getValueDataSize();
                auto dataStart = reader.start;
                reader.start += dataSize;

                if (reader.start <= reader.end)
                {
                    auto stringDataSize = reader.start < reader.end ? readVariableLengthInt (reader) : 0;

                    if (reader.start + stringDataSize > reader.end)
                        choc::value::throwError ("Malformed data");

                    item.dictionary.start = reinterpret_cast<const char*> (reader.start);
                    item.dictionary.size = stringDataSize;
                    item.value = choc::value::ValueView (std::move (type), const_cast<uint8_t*> (dataStart), std::addressof (item.dictionary));

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

    uint64_t findOffsetOfNextItemAfter (uint64_t start, uint64_t end) const
    {
        auto lowest = end;

        for (auto* i : pendingItems)
        {
            auto frame = i->startFrame;

            if (frame > start && frame < lowest)
                lowest = frame;
        }

        return lowest;
    }
};

} // namespace soul
