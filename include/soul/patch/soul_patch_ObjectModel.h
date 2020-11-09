/*
     _____ _____ _____ __
    |   __|     |  |  |  |
    |__   |  |  |  |  |  |__
    |_____|_____|_____|_____|

    Copyright (c) 2018 - ROLI Ltd.
*/

#if ! SOUL_PATCH_MAIN_INCLUDE_FILE
 #error "This header must not be included directly in your code - include soul_patch.h instead"
#endif

namespace soul
{
namespace patch
{

//==============================================================================
/** Minimal COM-style base class for the objects that the library uses. */
class RefCountedBase
{
public:
    virtual int addRef() noexcept = 0;
    virtual int release() noexcept = 0;
};

//==============================================================================
/** Minimal COM-style smart-pointer to hold the RefCountedBase objects that are
    returned by the library. Most of the classes contain an inner typedef
    which you can use instead of directly using this class.
*/
template <class ObjectType>
struct RefCountingPtr
{
    RefCountingPtr() noexcept = default;
    RefCountingPtr (decltype(nullptr)) noexcept {}
    ~RefCountingPtr() noexcept                                                     { release(); }
    explicit RefCountingPtr (ObjectType* object) noexcept : source (object)        {}
    RefCountingPtr (const RefCountingPtr& other) noexcept : source (other.source)  { addRef(); }
    RefCountingPtr (RefCountingPtr&& other) noexcept : source (other.source)       { other.source = {}; }
    RefCountingPtr& operator= (const RefCountingPtr& other) noexcept               { other.addRef(); release(); source = other.source; return *this; }
    RefCountingPtr& operator= (RefCountingPtr&& other) noexcept                    { release(); source = other.source; other.source = {}; return *this; }

    ObjectType* get() const noexcept                      { return source; }
    ObjectType& operator*() const noexcept                { return *source; }
    ObjectType* operator->() const noexcept               { return source; }
    operator bool() const noexcept                        { return source != nullptr; }
    ObjectType* incrementAndGetPointer() const noexcept   { addRef(); return source; }

    bool operator== (decltype(nullptr)) const noexcept    { return source == nullptr; }
    bool operator!= (decltype(nullptr)) const noexcept    { return source != nullptr; }

private:
    ObjectType* source = nullptr;

    void addRef() const noexcept    { if (source != nullptr) source->addRef(); }
    void release() noexcept         { if (source != nullptr) source->release(); }
};

//==============================================================================
/** Bare-bones COM wrapper to allow simple null-terminated strings to be passed
    safely in and out of the library.
*/
struct String  : public RefCountedBase
{
    virtual const char* getCharPointer() const = 0;

    struct Ptr : public RefCountingPtr<String>
    {
        Ptr() = default;
        explicit Ptr (String* s) : RefCountingPtr<String> (s) {}

        /** Allows casting to a string type such as std::string or juce::String */
        template <typename StringType>
        operator StringType() const
        {
            return toString<StringType>();
        }

        /** Explicitly returns a specified string type such as std::string or juce::String */
        template <typename StringType>
        StringType toString() const
        {
            if (auto c = get())
                return StringType (c->getCharPointer());

            return {};
        }
    };
};

//==============================================================================
template <typename Type>
struct Span
{
    const Type* vectorStart = nullptr;
    const Type* vectorEnd = nullptr;

    const Type* begin() const                  { return vectorStart; }
    const Type* end() const                    { return vectorEnd; }
    uint32_t size() const                      { return (uint32_t) (vectorEnd - vectorStart); }

    template <typename IndexType>
    const Type& operator[](IndexType i) const  { return vectorStart[i]; }
};

} // namespace patch
} // namespace soul
