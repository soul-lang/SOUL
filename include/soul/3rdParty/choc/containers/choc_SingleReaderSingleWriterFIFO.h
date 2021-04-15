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

#ifndef CHOC_SINGLE_READER_WRITER_FIFO_HEADER_INCLUDED
#define CHOC_SINGLE_READER_WRITER_FIFO_HEADER_INCLUDED

#include <vector>
#include <atomic>

#include "choc_FIFOReadWritePosition.h"

namespace choc::fifo
{

//==============================================================================
/**
    A simple atomic single-reader, single-writer FIFO.
*/
template <typename Item>
struct SingleReaderSingleWriterFIFO
{
    SingleReaderSingleWriterFIFO();
    ~SingleReaderSingleWriterFIFO() = default;

    /** Clears the FIFO and allocates a size for it. */
    void reset (size_t numItems);

    /** Clears the FIFO and allocates a size for it, filling the slots with
        copies of the given object.
        Note that this is not thread-safe with respect to the other methods - it must
        only be called when nothing else is modifying the FIFO.
    */
    void reset (size_t numItems, const Item& itemInitialiser);

    /** Resets the FIFO, keeping the current size. */
    void reset()                                            { position.reset(); }

    /** Returns the number of items in the FIFO. */
    uint32_t getUsedSlots() const                           { return position.getUsedSlots(); }
    /** Returns the number of free slots in the FIFO. */
    uint32_t getFreeSlots() const                           { return position.getFreeSlots(); }

    /** Attempts to push an into into the FIFO, returning false if no space was available. */
    bool push (const Item&);

    /** Attempts to push an into into the FIFO, returning false if no space was available. */
    bool push (Item&&);

    /** If any items are available, this copies the first into the given target, and returns true. */
    bool pop (Item& result);

private:
    FIFOReadWritePosition<uint32_t, std::atomic<uint32_t>> position;
    std::vector<Item> items;

    SingleReaderSingleWriterFIFO (const SingleReaderSingleWriterFIFO&) = delete;
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

template <typename Item> SingleReaderSingleWriterFIFO<Item>::SingleReaderSingleWriterFIFO()   { reset (1); }

template <typename Item> void SingleReaderSingleWriterFIFO<Item>::reset (size_t size)
{
    position.reset (size);
    items.resize (size + 1u);
}

template <typename Item> void SingleReaderSingleWriterFIFO<Item>::reset (size_t size, const Item& itemToCopy)
{
    position.reset (size);
    items.resize (size + 1u, itemToCopy);
}

template <typename Item> bool SingleReaderSingleWriterFIFO<Item>::push (const Item& item)
{
    if (auto slot = position.lockSlotForWriting())
    {
        items[slot.index] = item;
        position.unlock (slot);
        return true;
    }

    return false;
}

template <typename Item> bool SingleReaderSingleWriterFIFO<Item>::push (Item&& item)
{
    if (auto slot = position.lockSlotForWriting())
    {
        items[slot.index] = std::move (item);
        position.unlock (slot);
        return true;
    }

    return false;
}

template <typename Item> bool SingleReaderSingleWriterFIFO<Item>::pop (Item& result)
{
    if (auto slot = position.lockSlotForReading())
    {
        result = std::move (items[slot.index]);
        position.unlock (slot);
        return true;
    }

    return false;
}


} // choc::fifo

#endif
