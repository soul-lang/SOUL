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

template <typename Type> struct pool_ref;

//==============================================================================
/** A smart-pointer for objects which were created by a choc::memory::Pool.

    Almost all the AST classes are pool_ptrs to avoid the horror of trying dealing with
    ownership within a huge spaghetti-like graph of interconnected objects.

    A pool_ptr is little more than a wrapper around a raw pointer, but one of the handy
    tricks it has is that using pool_ptr instead of a raw pointer will detect any nullptr
    accesses and turn them into nice clean internal compiler errors rather than UB crashes.
    It also allows the opportunity for implementing new faster ways of casting than dynamic_cast,
    and casting is a very common operation on AST objects.
*/
template <typename Type>
struct pool_ptr  final
{
    pool_ptr() noexcept {}
    ~pool_ptr() noexcept {}

    pool_ptr (decltype (nullptr)) noexcept {}

    template <typename OtherType, typename = typename std::enable_if<std::is_convertible<OtherType*, Type*>::value>::type>
    pool_ptr (OtherType& o) noexcept : object (std::addressof (o)) {}

    template <typename OtherType, typename = typename std::enable_if<std::is_convertible<OtherType*, Type*>::value>::type>
    pool_ptr (pool_ptr<OtherType> other) noexcept : object (other.get()) {}

    template <typename OtherType, typename = typename std::enable_if<std::is_convertible<OtherType*, Type*>::value>::type>
    pool_ptr& operator= (pool_ptr<OtherType> other) noexcept    { object = other.get(); return *this; }

    Type* get() const noexcept                              { return object; }
    Type& getReference() const                              { SOUL_ASSERT (object != nullptr); return *object; }
    pool_ref<Type> getAsPoolRef() const                     { SOUL_ASSERT (object != nullptr); return *object; }
    Type& operator*() const                                 { SOUL_ASSERT (object != nullptr); return *object; }
    Type* operator->() const                                { SOUL_ASSERT (object != nullptr); return object; }

    void reset() noexcept                                   { object = nullptr; }
    void reset (Type* newObject) noexcept                   { object = newObject; }

    bool operator== (Type& other) const noexcept            { return object == std::addressof (other); }
    bool operator!= (Type& other) const noexcept            { return object != std::addressof (other); }
    bool operator<  (Type& other) const noexcept            { return object <  std::addressof (other); }
    bool operator== (pool_ptr other) const noexcept         { return object == other.object; }
    bool operator!= (pool_ptr other) const noexcept         { return object != other.object; }
    bool operator<  (pool_ptr other) const noexcept         { return object <  other.object; }
    bool operator== (decltype (nullptr)) const noexcept     { return object == nullptr; }
    bool operator!= (decltype (nullptr)) const noexcept     { return object != nullptr; }

    operator bool() const noexcept                          { return object != nullptr; }

    using ObjectType = Type;

private:
    Type* object = nullptr;

    operator size_t() const = delete; // these are here to avoid accidentally casting a pointer
    operator char() const = delete;   // to a numeric type via the bool cast.
    operator int() const = delete;
    operator long() const = delete;
    operator void*() const = delete;
};

//==============================================================================
/** A never-null smart-pointer for objects which were created by a choc::memory::Pool.

    This is like a pool_ptr but cannot contain a null pointer, so has more
    reference-like access methods and needs less checking.
*/
template <typename Type>
struct pool_ref  final
{
    pool_ref() = delete;
    pool_ref (decltype (nullptr)) = delete;
    ~pool_ref() noexcept {}

    template <typename OtherType, typename = typename std::enable_if<std::is_convertible<OtherType*, Type*>::value>::type>
    pool_ref (OtherType& o) noexcept : object (std::addressof (o)) {}

    template <typename OtherType, typename = typename std::enable_if<std::is_convertible<OtherType*, Type*>::value>::type>
    pool_ref (pool_ref<OtherType> other) noexcept : object (other.getPointer()) {}

    Type& get() const noexcept                              { return *object; }
    operator Type&() const noexcept                         { return *object; }
    operator pool_ptr<Type>() const noexcept                { return pool_ptr<Type> (*object); }
    Type* getPointer() const noexcept                       { return object; }
    Type& getReference() const noexcept                     { return *object; }
    Type* operator->() const                                { return object; }

    bool operator== (const Type& other) const noexcept      { return object == std::addressof (other); }
    bool operator!= (const Type& other) const noexcept      { return object != std::addressof (other); }
    bool operator<  (const Type& other) const noexcept      { return object <  std::addressof (other); }
    bool operator== (pool_ref other) const noexcept         { return object == other.object; }
    bool operator!= (pool_ref other) const noexcept         { return object != other.object; }
    bool operator<  (pool_ref other) const noexcept         { return object <  other.object; }

    using ObjectType = Type;

private:
    Type* object;
};

template <typename T1, typename T2> bool operator== (pool_ptr<T1> p1, const T2* p2) noexcept     { return p1.get() == p2; }
template <typename T1, typename T2> bool operator!= (pool_ptr<T1> p1, const T2* p2) noexcept     { return p1.get() != p2; }
template <typename T1, typename T2> bool operator== (const T1* p1, pool_ptr<T2> p2) noexcept     { return p1 == p2.get(); }
template <typename T1, typename T2> bool operator!= (const T1* p1, pool_ptr<T2> p2) noexcept     { return p1 != p2.get(); }
template <typename T1, typename T2> bool operator== (pool_ptr<T1> p1, pool_ptr<T2> p2) noexcept  { return p1.get() == p2.get(); }
template <typename T1, typename T2> bool operator!= (pool_ptr<T1> p1, pool_ptr<T2> p2) noexcept  { return p1.get() != p2.get(); }

template <typename T1, typename T2> bool operator== (pool_ref<T1> p1, const T2& p2) noexcept     { return p1.getPointer() == std::addressof (p2); }
template <typename T1, typename T2> bool operator!= (pool_ref<T1> p1, const T2& p2) noexcept     { return p1.getPointer() != std::addressof (p2); }
template <typename T1, typename T2> bool operator== (pool_ref<T1> p1, pool_ptr<T2> p2) noexcept  { return p1.getPointer() == p2.get(); }
template <typename T1, typename T2> bool operator!= (pool_ref<T1> p1, pool_ptr<T2> p2) noexcept  { return p1.getPointer() != p2.get(); }
template <typename T1, typename T2> bool operator== (const T1& p1, pool_ref<T2> p2) noexcept     { return std::addressof (p1) == p2.getPointer(); }
template <typename T1, typename T2> bool operator!= (const T1& p1, pool_ref<T2> p2) noexcept     { return std::addressof (p1) != p2.getPointer(); }
template <typename T1, typename T2> bool operator== (pool_ptr<T1> p1, pool_ref<T2> p2) noexcept  { return p1.get() == p2.getPointer(); }
template <typename T1, typename T2> bool operator!= (pool_ptr<T1> p1, pool_ref<T2> p2) noexcept  { return p1.get() != p2.getPointer(); }
template <typename T1, typename T2> bool operator== (pool_ref<T1> p1, pool_ref<T2> p2) noexcept  { return p1.getPointer() == p2.getPointer(); }
template <typename T1, typename T2> bool operator!= (pool_ref<T1> p1, pool_ref<T2> p2) noexcept  { return p1.getPointer() != p2.getPointer(); }

template <typename TargetType, typename SrcType>
inline pool_ptr<TargetType> cast (pool_ptr<SrcType> object)
{
    pool_ptr<TargetType> p;
    p.reset (dynamic_cast<TargetType*> (object.get()));
    return p;
}

template <typename TargetType, typename SrcType>
inline pool_ptr<TargetType> cast (pool_ref<SrcType> object)
{
    pool_ptr<TargetType> p;
    p.reset (dynamic_cast<TargetType*> (object.getPointer()));
    return p;
}

template <typename TargetType, typename SrcType>
inline pool_ptr<TargetType> cast (SrcType& object)
{
    pool_ptr<TargetType> p;
    p.reset (dynamic_cast<TargetType*> (&object));
    return p;
}

template <typename TargetType, typename SrcType>
inline bool is_type (pool_ptr<SrcType> object)
{
    if (object.get() == nullptr)
        return false;

    if constexpr (std::is_same<TargetType, SrcType>::value)
        return true;
    else
        return dynamic_cast<TargetType*> (object.get()) != nullptr;
}

template <typename TargetType, typename SrcType>
inline bool is_type (pool_ref<SrcType> object)
{
    ignoreUnused (object);

    if constexpr (std::is_same<TargetType, SrcType>::value)
        return true;
    else
        return dynamic_cast<TargetType*> (object.getPointer()) != nullptr;
}

template <typename TargetType, typename SrcType>
inline bool is_type (SrcType& object)
{
    ignoreUnused (object);

    if constexpr (std::is_same<TargetType, SrcType>::value)
        return true;
    else
        return dynamic_cast<TargetType*> (&object) != nullptr;
}

} // namespace soul

namespace std
{
    template <typename Type>
    struct hash<soul::pool_ptr<Type>>
    {
        size_t operator() (const soul::pool_ptr<Type>& p) const noexcept { return reinterpret_cast<size_t> (p.get()); }
    };

    template <typename Type>
    struct hash<soul::pool_ref<Type>>
    {
        size_t operator() (const soul::pool_ref<Type>& p) const noexcept { return reinterpret_cast<size_t> (p.getPointer()); }
    };
}
