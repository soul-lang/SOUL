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

#ifndef CHOC_POOL_HEADER_INCLUDED
#define CHOC_POOL_HEADER_INCLUDED

#include <vector>
#include <array>
#include <cstdlib>
#include "../platform/choc_Assert.h"

namespace choc::memory
{

//==============================================================================
/**
    A pool-based object allocator.

    A pool provides a way to quickly allocate objects whose lifetimes are tied to
    the lifetime of the pool rather than managed in the traditional ways.

    Calling Pool::allocate() will return a reference to a new object which will
    survive until the pool itself is later deleted or reset, at which point all
    the objects will be destroyed.

    Because all objects are allocated linearly from large heap blocks, allocation
    has very low overhead and is compact. This class also doesn't attempt to be
    thread-safe, which also helps to make it fast.

    Obviously a pool doesn't suit all use-cases, but in situations where you need
    to quickly allocate a large batch of objects and then use them for the lifetime of
    a finite task, it can be a very efficient tool. It's especially helpful if the group
    of objects have complicated ownership patterns which would make normal ownership
    techniques cumbersome.
*/
class Pool
{
public:
    Pool();
    ~Pool() = default;

    Pool (Pool&&) = default;
    Pool (const Pool&) = delete;
    Pool& operator= (Pool&&) = default;
    Pool& operator= (const Pool&) = delete;

    /// Resets the pool, deleting all the objects that have been allocated.
    void reset();

    /// Returns a reference to a newly-constructed object in the pool.
    /// The caller must not attempt to delete the object that is returned, it'll be
    /// automatically deleted when the pool itself is reset or deleted.
    template <typename ObjectType, typename... Args>
    ObjectType& allocate (Args&&... constructorArgs);


private:
    //==============================================================================
    static constexpr const size_t itemAlignment = 16;
    static constexpr const size_t blockSize = 65536;

    static constexpr size_t alignSize (size_t n)  { return (n + (itemAlignment - 1u)) & ~(itemAlignment - 1u); }

    struct Item;

    struct Block
    {
        Block();
        Block (Block&&);
        Block (const Block&) = delete;
        ~Block();

        bool hasSpaceFor (size_t size) const noexcept;
        Item* getItem (size_t position) noexcept;
        Item& allocateItem (size_t);

        size_t nextItemOffset = 0;
        std::unique_ptr<char[]> space;
    };

    std::vector<Block> blocks;
    void addBlock();
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

inline Pool::Pool()   { reset(); }

inline void Pool::reset()
{
    blocks.clear();
    blocks.reserve (32);
    addBlock();
}

struct Pool::Item
{
    size_t size;
    using Destructor = void(void*);
    Destructor* destructor;

    static constexpr size_t getHeaderSize()                   { return alignSize (sizeof (Item)); }
    static constexpr size_t getSpaceNeeded (size_t content)   { return alignSize (getHeaderSize() + content); }
    void* getItemData() noexcept                              { return reinterpret_cast<char*> (this) + getHeaderSize(); }
};

inline Pool::Block::Block() : space (new char[blockSize]) {}

inline Pool::Block::Block (Block&& other)
   : nextItemOffset (other.nextItemOffset),
     space (std::move (other.space))
{
    other.nextItemOffset = 0;
}

inline Pool::Block::~Block()
{
    for (size_t i = 0; i < nextItemOffset;)
    {
        auto item = getItem (i);

        if (item->destructor != nullptr)
            item->destructor (item->getItemData());

        i += item->size;
    }
}

inline bool Pool::Block::hasSpaceFor (size_t size) const noexcept    { return nextItemOffset + size <= blockSize; }
inline Pool::Item* Pool::Block::getItem (size_t position) noexcept   { return reinterpret_cast<Item*> (space.get() + position); }

inline Pool::Item& Pool::Block::allocateItem (size_t size)
{
    auto i = getItem (nextItemOffset);
    i->size = size;
    i->destructor = nullptr;
    nextItemOffset += size;
    return *i;
}

inline void Pool::addBlock()  { blocks.push_back ({}); }

template <typename ObjectType, typename... Args>
ObjectType& Pool::allocate (Args&&... constructorArgs)
{
    static constexpr auto itemSize = Item::getSpaceNeeded (sizeof (ObjectType));

    static_assert (itemSize <= blockSize, "Object size is larger than the maximum for the Pool class");

    if (! blocks.back().hasSpaceFor (itemSize))
        addBlock();

    auto& newItem = blocks.back().allocateItem (itemSize);
    auto allocatedObject = new (newItem.getItemData()) ObjectType (std::forward<Args> (constructorArgs)...);

    if constexpr (! std::is_trivially_destructible<ObjectType>::value)
        newItem.destructor = [] (void* t) { static_cast<ObjectType*> (t)->~ObjectType(); };

    return *allocatedObject;
}


} // namespace choc::memory

#endif // CHOC_POOL_HEADER_INCLUDED
