//
//    ██████ ██   ██  ██████   ██████
//   ██      ██   ██ ██    ██ ██            ** Clean Header-Only Classes **
//   ██      ███████ ██    ██ ██
//   ██      ██   ██ ██    ██ ██           https://github.com/Tracktion/choc
//    ██████ ██   ██  ██████   ██████
//
//   CHOC is (C)2021 Tracktion Corporation, and is offered under the terms of the ISC license:
//
//   Permission to use, copy, modify, and/or distribute this software for any purpose with or
//   without fee is hereby granted, provided that the above copyright notice and this permission
//   notice appear in all copies. THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
//   WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
//   AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
//   CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
//   WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
//   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#ifndef CHOC_FIFO_READ_WRITE_POSITION_HEADER_INCLUDED
#define CHOC_FIFO_READ_WRITE_POSITION_HEADER_INCLUDED

#include <vector>
#include <atomic>

#undef max
#undef min

namespace choc::fifo
{

//==============================================================================
/**
    Manages the read and write positions for a FIFO (but not the storage of
    objects in a FIFO).
*/
template <typename IndexType, typename AtomicType>
struct FIFOReadWritePosition
{
    FIFOReadWritePosition();
    ~FIFOReadWritePosition() = default;

    //==============================================================================
    static constexpr IndexType invalidIndex = std::numeric_limits<IndexType>::max();

    //==============================================================================
    /** Resets the positions and initialises the number of items in the FIFO. */
    void reset (size_t numItems);

    /** Resets the FIFO positions, keeping the current size. */
    void reset();

    /** Returns the total number of items that the FIFO has been set up to hold. */
    IndexType getTotalCapacity() const                   { return capacity; }
    /** Returns the number of items in the FIFO. */
    IndexType getUsedSlots() const                       { return getUsed (readPos, writePos); }
    /** Returns the number of free slots in the FIFO. */
    IndexType getFreeSlots() const                       { return getFree (readPos, writePos); }

    //==============================================================================
    struct WriteSlot
    {
        /** Returns true if a free slot was successfully obtained. */
        operator bool() const       { return index != invalidIndex; }

        /** The index of the slot that should be written to. */
        IndexType index;

    private:
        friend struct FIFOReadWritePosition;
        IndexType newEnd;
    };

    /** Attempts to get a slot into which the next item can be pushed.
        The WriteSlot object that is returned must be checked for validity by using its
        cast to bool operator - if the FIFO is full, it will be invalid. If it's valid
        then the caller must read what it needs from the slot at the index provided, and
        then immediately afterwards call unlock() to release the slot.
    */
    WriteSlot lockSlotForWriting();

    /** This must be called immediately after writing an item into the slot provided by
        lockSlotForWriting().
    */
    void unlock (WriteSlot);

    //==============================================================================
    struct ReadSlot
    {
        /** Returns true if a readable slot was successfully obtained. */
        operator bool() const       { return index != invalidIndex; }

        /** The index of the slot that should be read. */
        IndexType index;

    private:
        friend struct FIFOReadWritePosition;
        IndexType newStart;
    };

    /** Attempts to get a slot from which the first item can be read.
        The ReadSlot object that is returned must be checked for validity by using its
        cast to bool operator - if the FIFO is empty, it will be invalid. If it's valid
        then the caller must read what it needs from the slot at the index provided, and
        then immediately afterwards call unlock() to release the slot.
    */
    ReadSlot lockSlotForReading();

    /** This must be called immediately after reading an item from the slot provided by
        lockSlotForReading().
    */
    void unlock (ReadSlot);


private:
    //==============================================================================
    uint32_t capacity = 0;
    AtomicType readPos, writePos;

    uint32_t getUsed (uint32_t s, uint32_t e) const   { return e >= s ? (e - s) : (capacity + 1u - (s - e)); }
    uint32_t getFree (uint32_t s, uint32_t e) const   { return e >= s ? (capacity + 1u - (e - s)) : (s - e); }
    uint32_t increment (uint32_t i) const             { return i != capacity ? i + 1u : 0; }

    FIFOReadWritePosition (const FIFOReadWritePosition&) = delete;
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

template <typename IndexType, typename AtomicType>
FIFOReadWritePosition<IndexType, AtomicType>::FIFOReadWritePosition()   { reset (1); }

template <typename IndexType, typename AtomicType> void FIFOReadWritePosition<IndexType, AtomicType>::reset (size_t size)
{
    capacity = static_cast<uint32_t> (size);
    reset();
}

template <typename IndexType, typename AtomicType> void FIFOReadWritePosition<IndexType, AtomicType>::reset()
{
    readPos = 0;
    writePos = 0;
}

template <typename IndexType, typename AtomicType> typename FIFOReadWritePosition<IndexType, AtomicType>::WriteSlot FIFOReadWritePosition<IndexType, AtomicType>::lockSlotForWriting()
{
    WriteSlot slot;
    slot.index = writePos.load();
    slot.newEnd = increment (slot.index);

    if (slot.newEnd == readPos)
        slot.index = invalidIndex;

    return slot;
}

template <typename IndexType, typename AtomicType> void FIFOReadWritePosition<IndexType, AtomicType>::unlock (WriteSlot slot)
{
    writePos = slot.newEnd;
}

template <typename IndexType, typename AtomicType> typename FIFOReadWritePosition<IndexType, AtomicType>::ReadSlot FIFOReadWritePosition<IndexType, AtomicType>::lockSlotForReading()
{
    ReadSlot slot;
    slot.index = readPos.load();

    if (slot.index == writePos)
    {
        slot.index = invalidIndex;
        slot.newStart = slot.index;
    }
    else
    {
        slot.newStart = increment (slot.index);
    }

    return slot;
}

template <typename IndexType, typename AtomicType> void FIFOReadWritePosition<IndexType, AtomicType>::unlock (ReadSlot slot)
{
    readPos = slot.newStart;
}


} // choc::fifo

#endif
