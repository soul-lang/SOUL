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

#ifndef CHOC_DIRTY_LIST_HEADER_INCLUDED
#define CHOC_DIRTY_LIST_HEADER_INCLUDED

#include "choc_SingleReaderMultipleWriterFIFO.h"

namespace choc::fifo
{

//==============================================================================
/**
    A lock-free list of objects where multiple threads may mark an object as dirty,
    while a single thread polls the list and service the dirty ones.

    The main use-case for this class is where a set of objects need to be asynchronously
    serviced by a single thread or timer after they get flagged by a realtime thread.

    The class is designed so that the markAsDirty() and popNextDirtyObject() functions
    are lock-free and realtime-safe, and execute in constant time even when the total
    number of objects is very large.
    To make this possible, the compromises are that it needs to be initialised with a
    complete list of the objects needed, so that it can assign handles to them, and its
    memory requirements include allocating a small amount of storage per object.
*/
template <typename ObjectType>
struct DirtyList
{
    DirtyList() = default;
    ~DirtyList() = default;

    DirtyList (DirtyList&&) = delete;
    DirtyList (const DirtyList&) = delete;

    using Handle = uint32_t;

    /** Prepares the list by giving it the complete set of objects that it will manage.
        The return value is the set of handles assigned to each of the objects. The handles
        are later needed by the caller in order to call markAsDirty().

        Note that this method is not thread-safe, and must be performed before any other
        operations begin. It can be called multiple times to re-initialise the same list
        for other objects, as long as thread-safety is observed.
    */
    template <typename Array>
    std::vector<Handle> initialise (const Array& objects);

    /** Clears the queue of pending items and resets the 'dirty' state of all objects. */
    void resetAll();

    /** Marks an object as dirty.
        This may be called from any thread, and is lock-free.

        If the object is already marked as dirty, this function does nothing. If not, then
        the object is marked as dirty and added to the queue of objects which will later
        be returned by calls to popNextDirtyObject().
    */
    void markAsDirty (Handle objectHandle);

    /** Returns a pointer to the next dirty object (and in doing so marks that object
        as now being 'clean').
        If no objects are dirty, this returns nullptr.
        This method is lock-free, but is designed to be called by only a single reader
        thread.
    */
    ObjectType* popNextDirtyObject();

    /** Returns true if any objects are currently queued for attention. */
    bool areAnyObjectsDirty() const;

private:
    //==============================================================================
    std::unique_ptr<std::atomic_flag[]> flags; // avoiding a vector here as atomics aren't copyable
    std::vector<ObjectType*> allObjects;
    choc::fifo::SingleReaderMultipleWriterFIFO<Handle> fifo;
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

template <typename ObjectType>
template <typename Array>
std::vector<typename DirtyList<ObjectType>::Handle> DirtyList<ObjectType>::initialise (const Array& objects)
{
    std::vector<Handle> handles;
    auto numObjects = static_cast<size_t> (objects.size());
    handles.reserve (numObjects);
    flags.reset (new std::atomic_flag[numObjects]);
    allObjects.resize (numObjects);
    fifo.reset (numObjects);
    size_t i = 0;

    for (auto& o : objects)
    {
        CHOC_ASSERT (o != nullptr);
        flags[i].clear();
        allObjects[i] = o;
        handles.push_back (static_cast<Handle> (i));
        ++i;
    }

    return handles;
}

template <typename ObjectType>
void DirtyList<ObjectType>::resetAll()
{
    for (auto& o : allObjects)
        o.isDirty = false;

    fifo.reset();
}

template <typename ObjectType>
void DirtyList<ObjectType>::markAsDirty (Handle objectHandle)
{
    CHOC_ASSERT (objectHandle < allObjects.size());

    if (! flags[objectHandle].test_and_set())
        fifo.push (objectHandle);
}

template <typename ObjectType>
ObjectType* DirtyList<ObjectType>::popNextDirtyObject()
{
    Handle item;

    if (! fifo.pop (item))
        return nullptr;

    flags[item].clear();
    return allObjects[item];
}

template <typename ObjectType>
bool DirtyList<ObjectType>::areAnyObjectsDirty() const
{
    return fifo.getUsedSlots() != 0;
}


} // choc::fifo

#endif
