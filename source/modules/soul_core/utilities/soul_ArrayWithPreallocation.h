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

namespace soul
{

//==============================================================================
/**
    In a compiler there are lots of places where you need a vector, but where you know
    that it'll almost always contain just a few items, so this class acts like a simple
    vector which embeds some non-heap storage (but which uses the heap when its
    embedded storage is exceeded).

    std::vector has more tricks up its sleeve to optimise its behaviour based on the
    element type, but this class has the advantage that most mistakes like out of bounds
    accesses will trigger a SOUL_ASSERT and hence throw a clean internal compiler
    error rather than just causing UB.
*/
template <typename Item, size_t preallocatedItems>
struct ArrayWithPreallocation
{
    ArrayWithPreallocation() noexcept  : items (getPreallocatedSpace())
    {
    }

    ArrayWithPreallocation (const ArrayWithPreallocation& other)  : ArrayWithPreallocation()
    {
        operator= (other);
    }

    ArrayWithPreallocation (ArrayWithPreallocation&& other) noexcept
    {
        if (other.isHeapAllocated())
        {
            items = other.items;
            numActive = other.numActive;
            numAllocated = other.numAllocated;
            other.resetToInternalStorage();
            other.numActive = 0;
        }
        else
        {
            items = getPreallocatedSpace();
            numActive = other.numActive;

            for (size_t i = 0; i < numActive; ++i)
                new (items + i) Item (std::move (other.items[i]));
        }
    }

    template <typename ArrayType>
    ArrayWithPreallocation (const ArrayType& initialItems)  : ArrayWithPreallocation()
    {
        reserve (initialItems.size());

        for (auto& i : initialItems)
            emplace_back (i);
    }

    ArrayWithPreallocation& operator= (ArrayWithPreallocation&& other) noexcept
    {
        clear();

        if (other.isHeapAllocated())
        {
            items = other.items;
            numActive = other.numActive;
            numAllocated = other.numAllocated;
            other.resetToInternalStorage();
            other.numActive = 0;
        }
        else
        {
            numActive = other.numActive;

            for (size_t i = 0; i < numActive; ++i)
                new (items + i) Item (std::move (other.items[i]));
        }

        return *this;
    }

    ArrayWithPreallocation& operator= (const ArrayWithPreallocation& other)
    {
        if (other.size() > numActive)
        {
            reserve (other.size());

            for (size_t i = 0; i < numActive; ++i)
                items[i] = other.items[i];

            for (size_t i = numActive; i < other.size(); ++i)
                new (items + i) Item (other.items[i]);

            numActive = other.size();
        }
        else
        {
            shrink (other.size());

            for (size_t i = 0; i < numActive; ++i)
                items[i] = other.items[i];
        }

        return *this;
    }

    template <typename ArrayType>
    ArrayWithPreallocation& operator= (const ArrayType& other)
    {
        if (other.size() > numActive)
        {
            reserve (other.size());

            for (size_t i = 0; i < numActive; ++i)
                items[i] = other[i];

            for (size_t i = numActive; i < other.size(); ++i)
                new (items + i) Item (other[i]);

            numActive = other.size();
        }
        else
        {
            shrink (other.size());

            for (size_t i = 0; i < numActive; ++i)
                items[i] = other[i];
        }

        return *this;
    }

    ~ArrayWithPreallocation() noexcept
    {
        clear();
    }

    using value_type = Item;

    Item& operator[] (size_t index)                         { SOUL_ASSERT (index < numActive); return items[index]; }
    const Item& operator[] (size_t index) const             { SOUL_ASSERT (index < numActive); return items[index]; }

    Item* data() const noexcept                             { return items; }
    Item* begin() noexcept                                  { return items; }
    Item* end() noexcept                                    { return items + numActive; }
    const Item* begin() const noexcept                      { return items; }
    const Item* end() const noexcept                        { return items + numActive; }

    Item& front()                                           { SOUL_ASSERT (! empty()); return items[0]; }
    const Item& front() const                               { SOUL_ASSERT (! empty()); return items[0]; }
    Item& back()                                            { SOUL_ASSERT (! empty()); return items[numActive - 1]; }
    const Item& back() const                                { SOUL_ASSERT (! empty()); return items[numActive - 1]; }

    size_t size() const noexcept                            { return numActive; }
    bool empty() const noexcept                             { return numActive == 0; }

    bool operator== (choc::span<Item> other) const          { return choc::span<Item> (*this) == other; }
    bool operator!= (choc::span<Item> other) const          { return choc::span<Item> (*this) != other; }

    void push_back (const Item& item)                       { reserve (numActive + 1); new (items + numActive) Item (item); ++numActive; }
    void push_back (Item&& item)                            { reserve (numActive + 1); new (items + numActive) Item (std::move (item)); ++numActive; }

    template <typename... OtherItems>
    void push_back (const Item& first, OtherItems&&... otherItems)
    {
        reserve (numActive + 1 + sizeof... (otherItems));
        push_back (first);
        push_back (std::forward<OtherItems> (otherItems)...);
    }

    template <typename... Args>
    void emplace_back (Args&&... args)
    {
        reserve (numActive + 1);
        new (items + numActive) Item (std::forward<Args> (args)...);
        ++numActive;
    }

    void insert (Item* insertPos, const Item& item)
    {
        SOUL_ASSERT (insertPos != nullptr && insertPos >= begin() && insertPos <= end());
        auto index = insertPos - begin();
        push_back (item);
        std::rotate (begin() + index, end() - 1, end());
    }

    void insert (Item* insertPos, Item&& item)
    {
        SOUL_ASSERT (insertPos != nullptr && insertPos >= begin() && insertPos <= end());
        auto index = insertPos - begin();
        push_back (std::move (item));
        std::rotate (begin() + index, end() - 1, end());
    }

    void pop_back()
    {
        if (numActive == 1)
        {
            clear();
        }
        else
        {
            SOUL_ASSERT (numActive > 0);
            items[--numActive].~Item();
        }
    }

    void clear() noexcept
    {
        for (size_t i = 0; i < numActive; ++i)
            items[i].~Item();

        numActive = 0;
        freeIfHeapAllocated();
    }

    void resize (size_t newSize)
    {
        if (newSize > numActive)
        {
            reserve (newSize);

            while (numActive < newSize)
                new (items + numActive++) Item (Item());
        }
        else
        {
            shrink (newSize);
        }
    }

    void shrink (size_t newSize)
    {
        if (newSize == 0)
            return clear();

        SOUL_ASSERT (newSize <= numActive);

        while (newSize < numActive && numActive > 0)
            items[--numActive].~Item();
    }

    void reserve (size_t minSize)
    {
        if (minSize > numAllocated)
        {
            minSize = getAlignedSize<16> (minSize);

            if (minSize > preallocatedItems)
            {
                auto* newObjects = reinterpret_cast<Item*> (new char[minSize * sizeof (Item)]);

                for (size_t i = 0; i < numActive; ++i)
                {
                    new (newObjects + i) Item (std::move (items[i]));
                    items[i].~Item();
                }

                freeIfHeapAllocated();
                items = newObjects;
            }

            numAllocated = minSize;
        }
    }

    void erase (Item* startElement)
    {
        erase (startElement, startElement + 1);
    }

    void erase (Item* startElement, Item* endElement)
    {
        SOUL_ASSERT (startElement != nullptr && startElement >= begin() && startElement <= end());
        SOUL_ASSERT (endElement != nullptr && endElement >= begin() && endElement <= end());

        if (startElement == endElement)
            return;

        SOUL_ASSERT (startElement < endElement);

        if (endElement == end())
            return shrink ((size_t) (startElement - begin()));

        auto dest = startElement;

        for (auto src = endElement; src < end(); ++dest, ++src)
            *dest = std::move (*src);

        shrink (size() - (size_t) (endElement - startElement));
    }

private:
    Item* items;
    size_t numActive = 0, numAllocated = preallocatedItems;
    uint64_t space[(preallocatedItems * sizeof (Item) + sizeof (uint64_t) - 1) / sizeof (uint64_t)];

    Item* getPreallocatedSpace() noexcept               { return reinterpret_cast<Item*> (space); }
    const Item* getPreallocatedSpace() const noexcept   { return reinterpret_cast<const Item*> (space); }
    bool isHeapAllocated() const noexcept               { return numAllocated > preallocatedItems; }

    void resetToInternalStorage() noexcept
    {
        items = getPreallocatedSpace();
        numAllocated = preallocatedItems;
    }

    void freeIfHeapAllocated() noexcept
    {
        if (isHeapAllocated())
        {
            delete[] reinterpret_cast<char*> (items);
            resetToInternalStorage();
        }
    }
};

} // namespace soul
