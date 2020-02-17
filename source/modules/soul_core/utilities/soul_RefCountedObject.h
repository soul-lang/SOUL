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
/** A base class for intrusively-reference-counted objects, suitable for use by RefCountedPtr.
    Note that the counter is non-atomic!
*/
struct RefCountedObject
{
    RefCountedObject() = default;
    RefCountedObject (const RefCountedObject&) noexcept {}
    RefCountedObject (RefCountedObject&&) noexcept {}
    ~RefCountedObject() = default;

    uint32_t refCount = 0;
};

//==============================================================================
/** A smart pointer for referring to classes that inherit from RefCountedObject.
    Note that this is intended to be fast, not thread-safe!
*/
template <typename ObjectType>
struct RefCountedPtr  final
{
    constexpr RefCountedPtr() = default;
    constexpr RefCountedPtr (decltype (nullptr)) noexcept {}
    ~RefCountedPtr()   { decIfNotNull (object); }

    explicit RefCountedPtr (ObjectType* o) noexcept         : object (o) { incIfNotNull (o); }
    RefCountedPtr (ObjectType& o) noexcept                  : object (std::addressof (o)) { o.refCount++; }
    RefCountedPtr (const RefCountedPtr& other) noexcept     : object (other.object) { incIfNotNull (object); }
    RefCountedPtr (RefCountedPtr&& other) noexcept          : object (other.object) { other.object = nullptr; }

    RefCountedPtr& operator= (const RefCountedPtr& other)   { reset (other.object); return *this; }

    RefCountedPtr& operator= (RefCountedPtr&& other)
    {
        if (other.object != object)
        {
            auto oldObject = object;
            object = other.object;
            other.object = nullptr;
            decIfNotNull (oldObject);
        }

        return *this;
    }

    ObjectType* get() const                                 { return object; }
    ObjectType& operator*() const                           { SOUL_ASSERT (object != nullptr); return *object; }
    ObjectType* operator->() const                          { SOUL_ASSERT (object != nullptr); return object; }
    explicit operator bool() const                          { return object != nullptr; }

    void reset()                                            { reset (nullptr); }

    void reset (ObjectType* o)
    {
        incIfNotNull (o);
        auto oldObject = object;
        object = o;
        decIfNotNull (oldObject);
    }

    bool operator== (const RefCountedPtr& other) const      { return object == other.get(); }
    bool operator!= (const RefCountedPtr& other) const      { return object != other.get(); }
    bool operator== (const ObjectType* other) const         { return object == other; }
    bool operator!= (const ObjectType* other) const         { return object != other; }
    bool operator== (const ObjectType& other) const         { return object == std::addressof (other); }
    bool operator!= (const ObjectType& other) const         { return object != std::addressof (other); }

private:
    //==============================================================================
    ObjectType* object = nullptr;

    static void incIfNotNull (ObjectType* o) noexcept
    {
        if (o != nullptr)
            o->refCount++;
    }

    static void decIfNotNull (ObjectType* o)
    {
        if (o != nullptr)
        {
            SOUL_ASSERT (o->refCount > 0);

            if (--(o->refCount) == 0)
                delete o;
        }
    }
};

} // namespace soul
