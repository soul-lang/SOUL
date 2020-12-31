
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
        incomingItemAllocator = std::make_unique<LocalChocValueAllocator<incomingItemAllocationSpace>>();
        reset (256 * 1024, 2048);
    }

    ~MultiEndpointFIFO() = default;

    void reset (uint32_t fifoSize, uint32_t maxNumIncomingItems)
    {
        fifo.reset (fifoSize);
        incomingItems.resize (maxNumIncomingItems);
    }

    bool addInputData (soul::EndpointHandle endpoint, uint64_t time,
                       const choc::value::ValueView& value)
    {
        ScratchWriter scratch;

        scratch.write (std::addressof (time), sizeof (time));
        scratch.write (std::addressof (endpoint), sizeof (endpoint));

        auto& type = value.getType();
        type.serialise (scratch);
        auto startOfValueData = scratch.dest;

        if (! type.isVoid())
            scratch.write (value.getRawData(), type.getValueDataSize());

        if (scratch.overflowed())
            return false;

        if (value.getType().usesStrings())
        {
            LocalChocValueAllocator<maxItemSize> localSpace;
            choc::value::ValueView v (choc::value::Type (std::addressof (localSpace), value.getType()),
                                      startOfValueData, value.getDictionary());

            if (! DictionaryBuilder (scratch).write (v))
                return false;
        }

        return fifo.push (scratch.space, scratch.total);
    }

    //==============================================================================
    bool prepareForReading (uint64_t startFrameNumber, uint32_t numFramesNeeded)
    {
        incomingItemAllocator->reset();
        uint32_t numItems = 0;
        bool success = true;

        dataLock = fifo.popAllAvailable ([&] (const void* data, uint32_t size)
                                         {
                                             auto d = static_cast<const uint8_t*> (data);
                                             uint64_t absoluteTime;

                                             if (numItems < incomingItems.size()
                                                  && readIncomingItem (incomingItems[numItems], { d, d + size }, startFrameNumber, absoluteTime))
                                                 ++numItems;
                                             else
                                                 success = false;
                                         });

        framesToRead = numFramesNeeded;
        totalItemsRead = numItems;
        currentFrame = 0;
        nextChunkStart = 0;
        return success;
    }

    uint32_t getNumFramesInNextChunk (uint32_t maxNumFrames)
    {
        if (currentFrame >= framesToRead)
            return 0;

        nextChunkStart = findOffsetOfNextItemAfter (incomingItems.data(), totalItemsRead, currentFrame,
                                                    std::min (framesToRead, currentFrame + maxNumFrames));
        framesThisTime = nextChunkStart - currentFrame;
        return framesThisTime;
    }

    template <typename HandleItem>
    void processNextChunk (HandleItem&& handleItem)
    {
        for (uint32_t i = 0; i < totalItemsRead; ++i)
        {
            auto& item = incomingItems[i];
            auto itemStart = item.startFrame;
            auto itemEnd = itemStart + item.numFrames;

            if (itemEnd > currentFrame && itemStart < nextChunkStart)
            {
                if (item.numFrames != 1)
                {
                    if (itemStart < currentFrame)
                    {
                        auto amountToTrim = currentFrame - itemStart;
                        item.value = item.value.getElementRange (amountToTrim, item.numFrames - amountToTrim);
                        itemStart = currentFrame;
                        item.numFrames -= amountToTrim;
                        item.startFrame += amountToTrim;
                    }

                    if (itemEnd > nextChunkStart)
                        handleItem (item.endpoint, itemStart, static_cast<const choc::value::ValueView&> (item.value.getElementRange (0, nextChunkStart - itemStart)));
                    else
                        handleItem (item.endpoint, itemStart, static_cast<const choc::value::ValueView&> (item.value));
                }
                else
                {
                    handleItem (item.endpoint, itemStart, static_cast<const choc::value::ValueView&> (item.value));
                }
            }
        }

        currentFrame = nextChunkStart;
    }

    template <typename HandleItem>
    void iterateAllPreparedItemsForHandle (EndpointHandle handle, HandleItem&& handleItem)
    {
        for (uint32_t i = 0; i < totalItemsRead; ++i)
        {
            auto& item = incomingItems[i];

            if (item.endpoint == handle)
                handleItem (item.startFrame, static_cast<const choc::value::ValueView&> (item.value));
        }
    }

    void finishReading()
    {
        for (uint32_t i = 0; i < totalItemsRead; ++i)
            incomingItems[i].value = {};

        dataLock = {};
        totalItemsRead = 0;
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
                                  uint64_t absoluteTime = 0;

                                  if (readIncomingItem (item, { d, d + size }, 0, absoluteTime))
                                      handleItem (item.endpoint, absoluteTime, item.value);
                                  else
                                      success = false;
                              },
                              []{});

        return success;
    }

    static constexpr uint32_t maxItemSize = 4096;
    static constexpr size_t incomingItemAllocationSpace = 65536;

private:
    struct IncomingStringDictionary  : public choc::value::StringDictionary
    {
        Handle getHandleForString (std::string_view) override               { SOUL_ASSERT_FALSE; return {}; }
        std::string_view getStringForHandle (Handle handle) const override  { return std::string_view (start + handle.handle); }

        const char* start = {};
    };

    struct Item
    {
        uint32_t startFrame, numFrames;
        soul::EndpointHandle endpoint;
        choc::value::ValueView value;
        IncomingStringDictionary dictionary;
    };

    struct ScratchWriter
    {
        char space[maxItemSize];
        char* dest = space;
        uint32_t total = 0;

        bool overflowed() const      { return total > sizeof (space); }

        void write (const void* source, size_t size)
        {
            total += static_cast<uint32_t> (size);

            if (! overflowed())
            {
                std::memcpy (dest, source, size);
                dest += size;
            }
        }
    };

    choc::fifo::VariableSizeFIFO fifo;

    std::unique_ptr<LocalChocValueAllocator<incomingItemAllocationSpace>> incomingItemAllocator;
    std::vector<Item> incomingItems;
    choc::fifo::VariableSizeFIFO::DataLocker dataLock;
    uint32_t framesToRead = 0, totalItemsRead = 0, currentFrame = 0, nextChunkStart = 0, framesThisTime = 0;

    struct DictionaryBuilder
    {
        DictionaryBuilder (ScratchWriter& s) : scratch (s) {}

        static constexpr uint32_t maxStrings = 128;

        ScratchWriter& scratch;
        uint32_t numStrings = 0, stringEntryOffset = 0;
        choc::value::StringDictionary::Handle oldHandles[maxStrings],
                                              newHandles[maxStrings];

        bool write (choc::value::ValueView& v)
        {
            if (v.isString())
            {
                auto oldHandle = v.getStringHandle();

                for (decltype (numStrings) i = 0; i < numStrings; ++i)
                {
                    if (oldHandles[i] == oldHandle)
                    {
                        v.set (newHandles[i]);
                        return true;
                    }
                }

                if (numStrings == maxStrings)
                    return false;

                oldHandles[numStrings] = oldHandle;
                newHandles[numStrings] = { stringEntryOffset };

                auto text = v.getString();
                auto len = text.length();
                scratch.write (text.data(), len);
                char nullTerm = 0;
                scratch.write (std::addressof (nullTerm), 1u);
                stringEntryOffset += static_cast<uint32_t> (len + 1);

                v.set (newHandles[numStrings++]);
            }
            else if (v.isArray())
            {
                for (auto element : v)
                    if (element.getType().usesStrings())
                        if (! write (element))
                            return false;
            }
            else if (v.isObject())
            {
                auto numMembers = v.size();

                for (uint32_t i = 0; i < numMembers; ++i)
                {
                    auto member = v[i];

                    if (member.getType().usesStrings())
                        if (! write (member))
                            return false;
                }
            }

            return true;
        }
    };

    template <typename DestType> static void read (choc::value::InputData& reader, DestType& d)
    {
        if (reader.start + sizeof (d) > reader.end)
            throw choc::value::Error { "Malformed data" };

        std::memcpy (std::addressof (d), reader.start, sizeof (d));
        reader.start += sizeof (d);
    }

    bool readIncomingItem (Item& item, choc::value::InputData reader, uint64_t startFrameNumber, uint64_t& absoluteTime)
    {
        try
        {
            uint64_t time;
            read (reader, time);

            if (time >= startFrameNumber)
            {
                absoluteTime = time;
                item.startFrame = static_cast<uint32_t> (time - startFrameNumber);
                read (reader, item.endpoint);

                auto type = choc::value::Type::deserialise (reader, incomingItemAllocator.get());
                auto dataSize = type.getValueDataSize();

                if (reader.start + dataSize <= reader.end)
                {
                    item.dictionary.start = reinterpret_cast<const char*> (reader.start + dataSize);
                    item.value = choc::value::ValueView (std::move (type), const_cast<uint8_t*> (reader.start), std::addressof (item.dictionary));

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

    static uint32_t findOffsetOfNextItemAfter (const Item* items, uint32_t numItems, uint32_t startFrame, uint32_t endFrame)
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
};

} // namespace soul
