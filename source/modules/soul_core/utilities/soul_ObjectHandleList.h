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
/** Tracks a list of pointers, giving them opaque 32-bit handles which can be
    converted back to pointers in O(1), and which adds a custom mask to the handle
    to help avoid clashes between handles for different types of object.
*/
template <typename Type, typename HandleType, uint32_t maskBits>
struct ObjectHandleList
{
    Type* getFrom (HandleType handle) const noexcept
    {
        auto index = ((uint32_t) (uint64_t) handle) ^ maskBits;
        return index < objects.size() ? objects[index] : nullptr;
    }

    HandleType registerObject (Type* o)
    {
        SOUL_ASSERT (o != nullptr);
        SOUL_ASSERT (! contains (objects, o));

        for (auto& slot : objects)
        {
            if (slot == nullptr)
            {
                slot = o;
                return getHandleForSlot (std::addressof (slot));
            }
        }

        auto handle = ((uint64_t) objects.size()) ^ maskBits;
        objects.push_back (o);
        return (HandleType) handle;
    }

    HandleType findExistingHandle (Type* o) const noexcept
    {
        SOUL_ASSERT (o != nullptr);

        for (auto& slot : objects)
            if (slot == o)
                return getHandleForSlot (std::addressof (slot));

        SOUL_ASSERT_FALSE;
        return {};
    }

    void deleteObject (HandleType handle)
    {
        if (auto o = getFrom (handle))
        {
            deregisterObject (o);
            delete o;
        }
        else
        {
            SOUL_ASSERT_FALSE;
        }
    }

    void deregisterObject (Type* o)
    {
        SOUL_ASSERT (o != nullptr);
        auto it = std::find (objects.begin(), objects.end(), o);

        if (it != objects.end())
            *it = nullptr;
        else
            SOUL_ASSERT_FALSE;
    }

    size_t count() const
    {
        size_t result = 0;

        for (auto* o : objects)
            if (o != nullptr)
                ++result;

        return result;
    }

private:
    HandleType getHandleForSlot (Type* const* slot) const noexcept
    {
        return (HandleType) (uint64_t) ((uint32_t) (slot - std::addressof (objects.front())) ^ maskBits);
    }

    std::vector<Type*> objects;
};

} // namespace soul
