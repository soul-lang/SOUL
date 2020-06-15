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

#ifndef CHOC_SINGLE_READER_WRITER_FIFO_HEADER_INCLUDED
#define CHOC_SINGLE_READER_WRITER_FIFO_HEADER_INCLUDED

#include <vector>
#include <atomic>

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

    /** Resets the FIFO, keeping the current size. */
    void reset();

    /** Returns the number of items in the FIFO. */
    uint32_t getUsedSlots() const;
    /** Returns the number of free slots in the FIFO. */
    uint32_t getFreeSlots() const;

    /** Attempts to push an into into the FIFO, returning false if no space was available. */
    bool push (const Item&);

    /** Attempts to push an into into the FIFO, returning false if no space was available. */
    bool push (Item&&);

    /** If any items are available, this copies the first into the given target, and returns true. */
    bool pop (Item& result);

private:
    std::vector<Item> items;
    uint32_t capacity = 0;
    std::atomic<uint32_t> validStart, validEnd;

    uint32_t getUsed (uint32_t s, uint32_t e) const   { return e >= s ? (e - s) : (capacity + 1u - (s - e)); }
    uint32_t getFree (uint32_t s, uint32_t e) const   { return e >= s ? (capacity + 1u - (e - s)) : (s - e); }
    uint32_t increment (uint32_t i) const             { return i != capacity ? i + 1u : 0; }

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

template <typename Item> uint32_t SingleReaderSingleWriterFIFO<Item>::getUsedSlots() const    { return getUsed (validStart, validEnd); }
template <typename Item> uint32_t SingleReaderSingleWriterFIFO<Item>::getFreeSlots() const    { return getFree (validStart, validEnd); }

template <typename Item> void SingleReaderSingleWriterFIFO<Item>::reset (size_t size)
{
    capacity = static_cast<uint32_t> (size);
    items.resize (capacity + 1u);
    reset();
}

template <typename Item> void SingleReaderSingleWriterFIFO<Item>::reset()
{
    validStart = 0;
    validEnd = 0;
}

template <typename Item> bool SingleReaderSingleWriterFIFO<Item>::push (const Item& item)
{
    auto end = validEnd.load();
    auto newEnd = increment (end);

    if (newEnd == validStart)
        return false;

    items[end] = item;
    validEnd = newEnd;
    return true;
}

template <typename Item> bool SingleReaderSingleWriterFIFO<Item>::push (Item&& item)
{
    auto end = validEnd.load();
    auto newEnd = increment (end);

    if (newEnd == validStart)
        return false;

    items[end] = std::move (item);
    validEnd = newEnd;
    return true;
}

template <typename Item> bool SingleReaderSingleWriterFIFO<Item>::pop (Item& result)
{
    if (validStart == validEnd)
        return false;

    auto start = validStart.load();
    auto newStart = increment (start);
    result = std::move (items[start]);
    validStart = newStart;
    return true;
}


} // choc::fifo

#endif
