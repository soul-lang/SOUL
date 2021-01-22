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

#ifndef CHOC_SINGLE_READER_MULTI_WRITER_FIFO_HEADER_INCLUDED
#define CHOC_SINGLE_READER_MULTI_WRITER_FIFO_HEADER_INCLUDED

#include <mutex>

#include "choc_SingleReaderSingleWriterFIFO.h"
#include "../platform/choc_SpinLock.h"

namespace choc::fifo
{

//==============================================================================
/**
    A simple atomic single-reader, multiple-writer FIFO.
*/
template <typename Item>
struct SingleReaderMultipleWriterFIFO
{
    SingleReaderMultipleWriterFIFO() = default;
    ~SingleReaderMultipleWriterFIFO() = default;

    /** Clears the FIFO and allocates a size for it.
        Note that this is not thread-safe with respect to the other methods - it must
        only be called when nothing else is modifying the FIFO.
    */
    void reset (size_t numItems)                                { fifo.reset (numItems); }

    /** Clears the FIFO and allocates a size for it, filling the slots with
        copies of the given object.
    */
    void reset (size_t numItems, const Item& itemInitialiser)   { fifo.reset (numItems, itemInitialiser); }

    /** Resets the FIFO, keeping the current size. */
    void reset()                                                { fifo.reset(); }

    /** Returns the number of items in the FIFO. */
    uint32_t getUsedSlots() const                               { return fifo.getUsedSlots(); }
    /** Returns the number of free slots in the FIFO. */
    uint32_t getFreeSlots() const                               { return fifo.getFreeSlots(); }

    /** Attempts to push an into into the FIFO, returning false if no space was available. */
    bool push (const Item&);

    /** Attempts to push an into into the FIFO, returning false if no space was available. */
    bool push (Item&&);

    /** If any items are available, this copies the first into the given target, and returns true. */
    bool pop (Item& result)                                     { return fifo.pop (result); }

private:
    choc::fifo::SingleReaderSingleWriterFIFO<Item> fifo;
    choc::threading::SpinLock writeLock;

    SingleReaderMultipleWriterFIFO (const SingleReaderMultipleWriterFIFO&) = delete;
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

template <typename Item> bool SingleReaderMultipleWriterFIFO<Item>::push (const Item& item)
{
    const std::lock_guard<decltype (writeLock)> lock (writeLock);
    return fifo.push (item);
}

template <typename Item> bool SingleReaderMultipleWriterFIFO<Item>::push (Item&& item)
{
    const std::lock_guard<decltype (writeLock)> lock (writeLock);
    return fifo.push (std::move (item));
}

} // choc::fifo

#endif
