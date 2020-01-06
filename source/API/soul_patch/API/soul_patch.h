/*
     _____ _____ _____ __
    |   __|     |  |  |  |
    |__   |  |  |  |  |  |__
    |_____|_____|_____|_____|

    Copyright (c) 2018 - ROLI Ltd.
*/

/*
    This is the header you should add to your project in order to get all the patch-related
    classes from this folder.

    If you're trying to learn your way around this library, the important classes to start
    with are soul::patch::SOULPatchLibrary and soul::patch::PatchInstance.
*/

#pragma once

#include <cstdint>

#if _WIN32
 // Avoid including windows.h as this can cause all kinds of conflicts over what it pulls in
 extern "C" __declspec(dllimport) void* __stdcall LoadLibraryA (const char*);
 extern "C" __declspec(dllimport) int   __stdcall FreeLibrary (void*);
 extern "C" __declspec(dllimport) void* __stdcall GetProcAddress (void*, const char*);
#else
 #include <dlfcn.h>
#endif

#pragma pack (push, 1)
#define SOUL_PATCH_MAIN_INCLUDE_FILE 1

namespace soul
{
namespace patch
{

//==============================================================================
/** Minimal COM-style base class for the objects that the library uses. */
class RefCountedBase
{
public:
    virtual void addRef() noexcept = 0;
    virtual void release() noexcept = 0;

protected:
    RefCountedBase() = default;
    virtual ~RefCountedBase() = default;
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
    explicit RefCountingPtr (ObjectType* object) noexcept : source (object)        { addRef(); }
    RefCountingPtr (const RefCountingPtr& other) noexcept : source (other.source)  { addRef(); }
    RefCountingPtr (RefCountingPtr&& other) noexcept : source (other.source)       { other.source = {}; }
    RefCountingPtr& operator= (const RefCountingPtr& other) noexcept               { other.addRef(); release(); source = other.source; return *this; }
    RefCountingPtr& operator= (RefCountingPtr&& other) noexcept                    { release(); source = other.source; other.source = {}; return *this; }

    ObjectType* get() const noexcept                      { return source; }
    ObjectType& operator*() const noexcept                { return *source; }
    ObjectType* operator->() const noexcept               { return source; }
    operator bool() const noexcept                        { return source != nullptr; }

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
    const Type* vectorStart;
    const Type* vectorEnd;

    const Type* begin() const                  { return vectorStart; }
    const Type* end() const                    { return vectorEnd; }
    uint32_t size() const                      { return (uint32_t) (vectorEnd - vectorStart); }

    template <typename IndexType>
    const Type& operator[](IndexType i) const  { return vectorStart[i]; }
};

} // namespace patch
} // namespace soul

//==============================================================================
#include "soul_patch_VirtualFile.h"
#include "soul_patch_Player.h"
#include "soul_patch_Instance.h"
#include "soul_patch_Library.h"

#pragma pack (pop)
#undef SOUL_PATCH_MAIN_INCLUDE_FILE
