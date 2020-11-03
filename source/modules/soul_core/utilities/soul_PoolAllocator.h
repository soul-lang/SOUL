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

class PoolAllocator;
template <typename Type> struct pool_ref;

//==============================================================================
/** A smart-pointer for objects which were created by a PoolAllocator.

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
/** A never-null smart-pointer for objects which were created by a PoolAllocator.

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

//==============================================================================
/**
    An object pool.

    Objects added to the pool will all be deleted when the pool is destroyed, but
    no items are ever removed - the pool can only grow in size.

    Allocation of pool objects is very fast, since it allocates memory in bulk
    internally, and is designed to be single-threaded so has no overhead wasted
    on locking.

    When you create an object via the allocate() method, the best practice used in
    the SOUL codebase is to either keep a reference to it, or a pool_ptr, but never
    a raw pointer.
*/
class PoolAllocator  final
{
public:
    PoolAllocator()    { clear(); }
    ~PoolAllocator() = default;

    PoolAllocator (const PoolAllocator&) = delete;
    PoolAllocator& operator= (const PoolAllocator&) = delete;
    PoolAllocator (PoolAllocator&&) = default;
    PoolAllocator& operator= (PoolAllocator&&) = default;

    /** Clears the pool (deleting all the objects in it) */
    void clear()
    {
        pools.clear();
        pools.reserve (32);
        addNewPool();
    }

    /** Allocates a new object for the pool, returning a reference to it. */
    template <typename Type, typename... Args>
    Type& allocate (Args&&... args)
    {
        static_assert (sizeof (Type) + itemHeaderSize <= sizeof (Pool::space), "Can't allocate a pool object bigger than the pool block size");
        auto& newItem = allocateSpaceForObject (sizeof (Type));
        auto newObject = new (std::addressof (newItem.item)) Type (std::forward<Args> (args)...);

        // NB: the constructor may throw, so we have to be careful not to register its destructor until afterwards
        if constexpr (! std::is_trivially_destructible<Type>::value)
            newItem.destructor = [] (void* t) { static_cast<Type*> (t)->~Type(); };

        return *newObject;
    }

private:
    using DestructorFn = void(void*);

    static constexpr const size_t poolSize = 1024 * 64 - 32;
    static constexpr const size_t poolItemAlignment = 16;

    struct PoolItem
    {
        size_t size;
        DestructorFn* destructor;
        alignas(poolItemAlignment) void* item;
    };

    static constexpr const size_t itemHeaderSize = offsetof (PoolItem, item);

    struct Pool
    {
        Pool()
        {
            SOUL_ASSERT (isAlignedPointer<poolItemAlignment> (getNextAddress()));
        }

        Pool (const Pool&) = delete;
        Pool (Pool&&) = delete;

        ~Pool()
        {
            for (size_t i = 0; i < nextSlot;)
            {
                auto item = getItem (i);

                if (item->destructor != nullptr)
                    item->destructor (&item->item);

                i += item->size;
            }
        }

        bool hasSpaceFor (size_t size) const
        {
            return nextSlot + getAlignedSize<poolItemAlignment> (size + itemHeaderSize) <= poolSize;
        }

        void* getNextAddress()
        {
            auto item = getItem (nextSlot);
            return &item->item;
        }

        PoolItem& createItem (size_t size)
        {
            size = getAlignedSize<poolItemAlignment> (size + itemHeaderSize);
            auto item = getItem (nextSlot);
            item->size = size;
            item->destructor = nullptr;
            nextSlot += size;
            return *item;
        }

        PoolItem* getItem (size_t byteOffset) noexcept  { return reinterpret_cast<PoolItem*> (space.data() + byteOffset); }

        size_t nextSlot = 0;
        alignas(poolItemAlignment) std::array<char, poolSize> space;
    };

    std::vector<std::unique_ptr<Pool>> pools;
    Pool* currentPool = nullptr;

    void addNewPool()
    {
        currentPool = new Pool();
        pools.emplace_back (currentPool);
    }

    PoolItem& allocateSpaceForObject (size_t size)
    {
        if (! currentPool->hasSpaceFor (size))
        {
            addNewPool();
            SOUL_ASSERT (currentPool->hasSpaceFor (size));
        }

        return currentPool->createItem (size);
    }
};

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
