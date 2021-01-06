/*
    ██████ ██   ██  ██████   ██████
   ██      ██   ██ ██    ██ ██         Clean Header-Only Classes
   ██      ███████ ██    ██ ██         Copyright (C)2020 Julian Storer
   ██      ██   ██ ██    ██ ██
    ██████ ██   ██  ██████   ██████    https://github.com/julianstorer/choc

   This file is part of the CHOC C++ collection - see the github page to find out more.

   The code in this file is provided under the terms of the ISC license:

   Permission to use, copy, modify, and/or distribute this software for any purpose with
   or without fee is hereby granted, provided that the above copyright notice and this
   permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO
   THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT
   SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR
   ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
   CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
   OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef CHOC_VARIABLE_SIZE_FIFO_HEADER_INCLUDED
#define CHOC_VARIABLE_SIZE_FIFO_HEADER_INCLUDED

#include <vector>
#include <mutex>
#include "../platform/choc_SpinLock.h"

namespace choc::fifo
{

//==============================================================================
/**
    A multiple writer, single consumer FIFO which can store items as contiguous
    blocks of data with individual sizes.
    Multiple write threads may have to briefly spin-wait for each other, but the
    reader thread is not blocked by the activity of writers.

    Note that this class uses a circular buffer, but does not split individual
    items across the end of the buffer. This means that when accessing an item,
    the reader always has direct access to each item's data as a contiguous block.
    But it also means that when an item is too large to fit into empty space at the
    end of the circular buffer, that space is treated as padding and the item is
    written at the start of the buffer, so it may not always be possible to add an
    item, even if there's enough total space for it. You might want to take this
    into account by making sure the capacity is at least a few times greater than
    the largest item you need to store.
*/
struct VariableSizeFIFO
{
    VariableSizeFIFO();
    ~VariableSizeFIFO();

    /** Resets the FIFO with a given capacity in bytes.
        Note that this is not thread-safe with respect to the other methods - it must
        only be called when nothing else is pushing or popping to the FIFO.
    */
    void reset (uint32_t totalFIFOSizeBytes);

    /** Pushes a chunk of data onto the FIFO.
        If there is space in the FIFO for the given chunk, it will be added, and the
        function will return true.
        If the function returns false, then the FIFO didn't have space, and its state
        will not have been modified by the call, so a caller can try again.
        If numBytes is 0, nothing will be done, and the function will return false.
        Note that because the FIFO stores each item's data as contiguous block, then
        if the free space is split across the end of the circular buffer, then items
        are not always guaranteed to fit, even if they are smaller than the space
        returned by getFreeSpace().
    */
    bool push (const void* sourceData, uint32_t numBytes);

    /** Retrieves the first item's data chunk via a callback.
        If there are any pending items in the FIFO, the handleItem function
        provided will be called - it must be a functor or lambda with parameters which can
        accept being called as handleItem (const void* data, uint32_t size).
        The function returns true if a callback was made, or false if the FIFO was empty.
    */
    template <typename HandleItem>
    bool pop (HandleItem&& handleItem);

    /** Allows access to all the available item in the FIFO via a callback.
        If there are any pending items in the FIFO, the handleItem function will be called
        for each of them. HandleItem must be a functor or lambda which can be called as
        handleItems (const void* data, uint32_t size).
    */
    template <typename HandleItem>
    void popAllAvailable (HandleItem&&);

    /** Allows multiple items to be read from the FIFO without releasing their slots
        until the BatchReadOperation object is deleted.
    */
    struct BatchReadOperation
    {
        explicit BatchReadOperation (VariableSizeFIFO&) noexcept;

        BatchReadOperation() = default;
        BatchReadOperation (BatchReadOperation&&);
        BatchReadOperation& operator= (BatchReadOperation&&);
        ~BatchReadOperation() noexcept;

        template <typename HandleItem>
        bool pop (HandleItem&& handleItem);

        bool isActive() const       { return fifo != nullptr; }
        void release() noexcept;

    private:
        VariableSizeFIFO* fifo = nullptr;
        uint32_t newReadPos = 0;
    };

    /** Returns the number of used bytes in the FIFO. */
    uint32_t getUsedSpace() const;

    /** Returns the number of bytes free in the FIFO.
        Bear in mind that because each item needs some header bytes, and because items
        are stored contiguously, then the number of free bytes does not mean that an
        item of this size can definitely be added.
    */
    uint32_t getFreeSpace() const;

private:
    using ItemHeader = uint32_t; // 0 = skip to the end of the buffer
    static constexpr uint32_t headerSize = static_cast<uint32_t> (sizeof (ItemHeader));

    uint32_t capacity = 0;
    std::atomic<uint32_t> readPos, writePos;
    choc::threading::SpinLock writeLock;
    std::vector<char> buffer;
};


//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================

inline VariableSizeFIFO::VariableSizeFIFO()  { reset (8); }
inline VariableSizeFIFO::~VariableSizeFIFO() = default;

inline uint32_t VariableSizeFIFO::getUsedSpace() const  { auto s = readPos.load(); auto e = writePos.load(); return e >= s ? (e - s) : (capacity + 1u - (s - e)); }
inline uint32_t VariableSizeFIFO::getFreeSpace() const  { auto s = readPos.load(); auto e = writePos.load(); return e >= s ? (capacity + 1u - (e - s)) : (s - e); }

inline void VariableSizeFIFO::reset (uint32_t totalFIFOSizeBytes)
{
    readPos = 0;
    writePos = 0;
    capacity = std::max (totalFIFOSizeBytes, headerSize + 4);
    buffer.clear();
    buffer.resize (capacity + 1u);
}

inline bool VariableSizeFIFO::push (const void* sourceData, uint32_t numBytes)
{
    if (numBytes == 0)
        return false;

    auto bytesNeeded = numBytes + headerSize;

    const std::lock_guard<decltype(writeLock)> lock (writeLock);

    auto destOffset = writePos.load();
    auto dest = buffer.data() + destOffset;

    if (destOffset >= readPos)
    {
        // check whether we can fit a contiguous block at the current position
        if (destOffset + bytesNeeded > capacity)
        {
            if (bytesNeeded >= readPos) // check whether there'll be enough space after padding
                return false;

            std::fill (dest, dest + headerSize, 0); // header of 0 = skip to the end
            dest = buffer.data();
            destOffset = 0;
        }
    }
    else
    {
        if (destOffset + bytesNeeded >= readPos)
            return false;
    }

    auto header = static_cast<ItemHeader> (numBytes);
    std::memcpy (dest, std::addressof (header), headerSize);
    std::memcpy (dest + headerSize, sourceData, numBytes);
    writePos = (destOffset + bytesNeeded) % capacity;

    return true;
}

template <typename HandleItem>
bool VariableSizeFIFO::pop (HandleItem&& handleItem)
{
    for (;;)
    {
        if (readPos == writePos)
            return false;

        auto itemData = buffer.data() + static_cast<int32_t> (readPos);
        ItemHeader itemSize;
        std::memcpy (std::addressof (itemSize), itemData, headerSize);

        if (itemSize != 0)
        {
            handleItem (static_cast<void*> (itemData + headerSize), itemSize);
            readPos = (readPos + itemSize + headerSize) % capacity;
            return true;
        }

        readPos = 0;
    }
}

template <typename HandleItem>
void VariableSizeFIFO::popAllAvailable (HandleItem&& handleItem)
{
    auto originalWritePos = writePos.load();
    auto newReadPos = readPos.load();

    while (newReadPos != originalWritePos)
    {
        auto itemData = buffer.data() + static_cast<int32_t> (newReadPos);
        ItemHeader itemSize;
        std::memcpy (std::addressof (itemSize), itemData, headerSize);

        if (itemSize != 0)
        {
            handleItem (static_cast<void*> (itemData + headerSize), itemSize);
            newReadPos = (newReadPos + itemSize + headerSize) % capacity;
        }
        else
        {
            newReadPos = 0;
        }
    }

    readPos = newReadPos;
}

inline VariableSizeFIFO::BatchReadOperation::BatchReadOperation (VariableSizeFIFO& f) noexcept : fifo (std::addressof (f)) { newReadPos = f.readPos.load(); }
inline VariableSizeFIFO::BatchReadOperation::BatchReadOperation (BatchReadOperation&& other) : fifo (other.fifo), newReadPos (other.newReadPos) { other.fifo = nullptr; }
inline VariableSizeFIFO::BatchReadOperation& VariableSizeFIFO::BatchReadOperation::operator= (BatchReadOperation&& other) { release(); fifo = other.fifo; newReadPos = other.newReadPos; other.fifo = nullptr; return *this; }
inline VariableSizeFIFO::BatchReadOperation::~BatchReadOperation() noexcept  { release(); }
inline void VariableSizeFIFO::BatchReadOperation::release() noexcept    { if (fifo != nullptr) { fifo->readPos = newReadPos; fifo = nullptr; } }

template <typename HandleItem>
bool VariableSizeFIFO::BatchReadOperation::pop (HandleItem&& handleItem)
{
    SOUL_ASSERT (fifo != nullptr);
    auto originalWritePos = fifo->writePos.load();

    while (newReadPos != originalWritePos)
    {
        auto itemData = fifo->buffer.data() + static_cast<int32_t> (newReadPos);
        ItemHeader itemSize;
        std::memcpy (std::addressof (itemSize), itemData, headerSize);

        if (itemSize != 0)
        {
            handleItem (static_cast<void*> (itemData + headerSize), itemSize);
            newReadPos = (newReadPos + itemSize + headerSize) % fifo->capacity;
            return true;
        }

        newReadPos = 0;
    }

    return false;
}

} // namespace choc::fifo

#endif // CHOC_VARIABLE_SIZE_FIFO_HEADER_INCLUDED
