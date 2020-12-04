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
template <typename Vector, typename Type>
inline bool contains (const Vector& v, Type&& i)
{
    return std::find (std::begin (v), std::end (v), i) != v.end();
}

template <typename Vector, typename Predicate>
inline bool removeIf (Vector& v, Predicate&& pred)
{
    auto oldEnd = std::end (v);
    auto newEnd = std::remove_if (std::begin (v), oldEnd, pred);

    if (newEnd == oldEnd)
        return false;

    v.erase (newEnd, oldEnd);
    return true;
}

template <typename Vector, typename Predicate>
inline bool removeFirst (Vector& v, Predicate&& pred)
{
    auto found = std::find_if (std::begin (v), std::end (v), pred);

    if (found == std::end (v))
        return false;

    v.erase (found);
    return true;
}

template <typename Vector, typename ItemType>
inline bool removeItem (Vector& v, ItemType&& itemToRemove)
{
    auto found = std::find (std::begin (v), std::end (v), itemToRemove);

    if (found == std::end (v))
        return false;

    v.erase (found);
    return true;
}

template <typename Vector>
inline void sortAndRemoveDuplicates (Vector& v)
{
    if (v.size() > 1)
    {
        std::sort (std::begin (v), std::end (v));
        v.erase (std::unique (std::begin (v), std::end (v)), std::end (v));
    }
}

template <typename Vector1, typename Vector2>
inline void appendVector (Vector1& dest, const Vector2& source)
{
    dest.reserve (dest.size() + source.size());

    for (auto& i : source)
        dest.push_back (i);
}

template<typename Vector, typename Type>
inline bool appendIfNotPresent (Vector& v, Type&& i)
{
    if (contains (v, i))
        return false;

    v.push_back (i);
    return true;
}


template <typename Vector1, typename Vector2>
inline void copyVector (Vector1& dest, const Vector2& source)
{
    dest.clear();
    appendVector (dest, source);
}

template <typename Vector1, typename Vector2>
inline void mergeSortedVectors (Vector1& dest, const Vector2& source)
{
    appendVector (dest, source);
    sortAndRemoveDuplicates (dest);
}

template <typename Vector1, typename Vector2>
inline bool intersectVectors (Vector1& target, const Vector2& itemsToRetain)
{
    return removeIf (target, [&] (auto& item) { return ! contains (itemsToRetain, item); });
}

template <typename Vector1, typename Vector2>
inline bool removeFromVector (Vector1& target, const Vector2& itemsToRemove)
{
    return removeIf (target, [&] (auto& item) { return contains (itemsToRemove, item); });
}

template <typename Vector, typename Position>
inline auto getIteratorForIndex (Vector& vector, Position index)
{
    return vector.begin() + static_cast<typename Vector::difference_type> (index);
}

template <typename ConvertStringToValueFn>
static choc::value::Value replaceStringsWithValues (const choc::value::ValueView& value,
                                                    const ConvertStringToValueFn& convertStringToValue)
{
    if (value.isString())
        return convertStringToValue (value.getString());

    if (value.isArray())
    {
        auto v = choc::value::createEmptyArray();

        for (auto i : value)
            v.addArrayElement (replaceStringsWithValues (i, convertStringToValue));

        return v;
    }

    if (value.isObject())
    {
        auto v = choc::value::createObject (value.getObjectClassName());

        value.visitObjectMembers ([&] (std::string_view memberName, const choc::value::ValueView& memberValue)
        {
            v.addMember (memberName, replaceStringsWithValues (memberValue, convertStringToValue));
        });

        return v;
    }

    return choc::value::Value (value);
}

//==============================================================================
/** This is a bit like a lite version of std::span.
    However, it does have the huge advantage that it asserts when mistakes are made like
    out-of-bounds accesses, so that we get clean internal compiler rather than crashes
    and UB.
*/
template <typename Type>
struct ArrayView
{
    ArrayView() = default;
    ArrayView (const ArrayView&) = default;
    ArrayView (ArrayView&&) = default;
    ArrayView& operator= (ArrayView&&) = default;

    ArrayView (Type* start, Type* end) noexcept  : s (start), e (end) {}
    ArrayView (const Type* start, size_t length) noexcept  : ArrayView (const_cast<Type*> (start), const_cast<Type*> (start) + length) {}

    template <typename VectorType>
    ArrayView (const VectorType& v)  : ArrayView (v.data(), v.size()) {}

    template <size_t length>
    ArrayView (Type (&array)[length])  : ArrayView (array, length) {}

    Type* data() const noexcept                 { return s; }
    Type* begin() noexcept                      { return s; }
    Type* end() noexcept                        { return e; }
    const Type* begin() const noexcept          { return s; }
    const Type* end() const noexcept            { return e; }

    Type& front()                               { SOUL_ASSERT (! empty()); return *s; }
    Type& back()                                { SOUL_ASSERT (! empty()); return *(e - 1); }
    const Type& front() const                   { SOUL_ASSERT (! empty()); return *s; }
    const Type& back() const                    { SOUL_ASSERT (! empty()); return *(e - 1); }

    Type& operator[] (size_t i)                 { SOUL_ASSERT (i < size()); return s[i]; }
    const Type& operator[] (size_t i) const     { SOUL_ASSERT (i < size()); return s[i]; }

    bool empty() const                          { return s == e; }
    size_t size() const                         { return static_cast<size_t> (e - s); }

    ArrayView tail() const                      { SOUL_ASSERT (! empty()); return { s + 1, e }; }

    bool operator== (ArrayView other) const
    {
        if (size() != other.size())
            return false;

        for (size_t i = 0; i < size(); ++i)
            if (s[i] != other.s[i])
                return false;

        return true;
    }

    bool operator!= (ArrayView other) const    { return ! operator== (other); }

    operator std::vector<typename std::remove_const<Type>::type>() const
    {
        return toVector();
    }

    std::vector<typename std::remove_const<Type>::type> toVector() const
    {
        std::vector<typename std::remove_const<Type>::type> v;
        v.reserve (size());

        for (auto& i : *this)
            v.emplace_back (i);

        return v;
    }

private:
    Type* s = nullptr;
    Type* e = nullptr;
};

//==============================================================================
/** A simple, intrusive single-linked-list.
    The main use-case that this was written for is dealing with the list of statements
    in a block, where using vectors is tricky because it's common to need to mutate
    the list while iterating it.
*/
template <typename Type>
struct LinkedList
{
    LinkedList() = default;
    LinkedList (const LinkedList&) = default;
    LinkedList& operator= (const LinkedList&) = default;

    struct Iterator : public std::iterator<std::forward_iterator_tag, Type>
    {
        Iterator() = default;
        Iterator (decltype (nullptr)) {}
        Iterator (Type* o) : object (o) {}
        Iterator (Type& o) : Iterator (std::addressof (o)) {}

        Type* operator*() const                         { SOUL_ASSERT (object != nullptr); return object; }
        Type* operator->() const                        { SOUL_ASSERT (object != nullptr); return object; }
        operator bool() const                           { return object != nullptr; }

        Iterator& operator++()                          { object = next(); return *this; }

        Type* next() const                              { SOUL_ASSERT (object != nullptr); return object->nextObject; }

        bool operator== (decltype (nullptr)) const      { return object == nullptr; }
        bool operator!= (decltype (nullptr)) const      { return object != nullptr; }
        bool operator== (Iterator other) const          { return object == other.object; }
        bool operator!= (Iterator other) const          { return object != other.object; }

        void removeAllSuccessors()
        {
            if (object != nullptr)
                object->nextObject = nullptr;
        }

    private:
        friend struct LinkedList;

        void insertAfter (Type& newObject)
        {
            newObject.nextObject = next();
            object->nextObject = std::addressof (newObject);
        }

        void replaceNext (Type& newObject)
        {
            SOUL_ASSERT (object != nullptr && object->nextObject != nullptr);
            newObject.nextObject = object->nextObject->nextObject;
            object->nextObject = std::addressof (newObject);
        }

        void removeNext()
        {
            SOUL_ASSERT (object != nullptr);

            if (object->nextObject != nullptr)
                object->nextObject = object->nextObject->nextObject;
        }

        Type* object = nullptr;
    };

    Iterator begin() const      { return Iterator (firstObject); }
    static Iterator end()       { return {}; }

    bool empty() const          { return firstObject == nullptr; }
    void clear()                { firstObject = nullptr; }

    Iterator getLast() const
    {
        if (auto o = firstObject)
        {
            while (o->nextObject != nullptr)
                o = o->nextObject;

            return Iterator (o);
        }

        return {};
    }

    Iterator getNext (Iterator predecessor) const
    {
        return predecessor == Iterator() ? begin() : predecessor.next();
    }

    Iterator getPredecessor (Type& object) const
    {
        Iterator last;

        for (auto i : *this)
        {
            if (i == std::addressof (object))
                return last;

            last = i;
        }

        SOUL_ASSERT_FALSE;
        return {};
    }

    bool contains (Type& object) const
    {
        for (auto i : *this)
            if (i == std::addressof (object))
                return true;

        return false;
    }

    void insertFront (Type& newObject)
    {
        newObject.nextObject = firstObject;
        firstObject = std::addressof (newObject);
    }

    void removeFront()
    {
        if (! empty())
            firstObject = firstObject->nextObject;
    }

    void replaceFront (Type& newObject)
    {
        SOUL_ASSERT (firstObject != nullptr);
        newObject.nextObject = firstObject->nextObject;
        firstObject = std::addressof (newObject);
    }

    Iterator insertAfter (Iterator predecessor, Type& newObject)
    {
        if (predecessor == Iterator())
            insertFront (newObject);
        else
            predecessor.insertAfter (newObject);

        return Iterator (newObject);
    }

    void replaceAfter (Iterator predecessor, Type& newObject)
    {
        if (predecessor == Iterator())
            replaceFront (newObject);
        else
            predecessor.replaceNext (newObject);
    }

    void removeNext (Iterator predecessor)
    {
        if (predecessor == Iterator())
            removeFront();
        else
            predecessor.removeNext();
    }

    void append (Type& newObject)
    {
        if (auto last = getLast())
            last->nextObject = std::addressof (newObject);
        else
            firstObject = std::addressof (newObject);
    }

    template <typename Predicate>
    void removeMatches (Predicate&& shouldRemove)
    {
        while (! empty() && shouldRemove (*firstObject))
            removeFront();

        for (auto i : *this)
            while (i->nextObject != nullptr && shouldRemove (*i->nextObject))
                removeNext (*i);
    }

    void remove (Type& item)
    {
        removeMatches ([&] (Type& i) { return std::addressof (i) == std::addressof (item); });
    }

    template <typename Predicate>
    void replaceMatches (Predicate&& getReplacement)
    {
        if (! empty())
        {
            for (;;)
            {
                if (auto replacement = getReplacement (*firstObject))
                    replaceFront (*replacement);
                else
                    break;
            }

            for (auto i : *this)
            {
                while (i->nextObject != nullptr)
                {
                    if (auto replacement = getReplacement (*i->nextObject))
                        replaceAfter (*i, *replacement);
                    else
                        break;
                }
            }
        }
    }

private:
    Type* firstObject = nullptr;
};

} // namespace soul
