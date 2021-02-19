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

/** The library compatibility API version is used to make sure this set of header
    files is compatible with the library that gets loaded.
*/
static constexpr int currentLibraryAPIVersion = 0x100b;

//==============================================================================
/**
    Dynamically opens and connects to the shared library containing the patch loader.
    You should only create a single instance of this class. and use it in an RAII
    style, making sure it lives longer than all the objects that originate from it.
*/
struct SOULPatchLibrary
{
    /** Attempts to open the shared library at the given file path. */
    SOULPatchLibrary (const char* soulPatchLibraryPath)
        : handle (openLibrary (soulPatchLibraryPath))
    {
        if (handle != nullptr)
            if (auto getVersionFn = (GetLibraryVersionFunction) getFunction (handle, "getSOULPatchLibraryVersion"))
                if (isCompatibleLibraryVersion (getVersionFn()))
                    createInstanceFunction = (CreateInstanceFunction) getFunction (handle, "createSOULPatchBundle");
    }

    /** Make sure you don't delete the library while any objects are still in use! */
    ~SOULPatchLibrary()
    {
        if (handle != nullptr)
            closeLibrary (handle);
    }

    /** Returns true if the library has been loaded and is ready to use. */
    bool loadedSuccessfully() const
    {
        return createInstanceFunction != nullptr;
    }

    /** Creates a new instance of a PatchInstance for a given file path.
        The path should be that of a .soulpatch file. Once you have a PatchInstance object,
        you can use it to build and run instances of that patch.
    */
    PatchInstance::Ptr createPatchFromFileBundle (const char* path) const
    {
        return create ({}, path);
    }

    /** Creates a new instance of a PatchInstance for a given file path.
        The file should point to a .soulpatch file. Once you have a PatchInstance object,
        you can use it to build and run instances of that patch.
    */
    PatchInstance::Ptr createPatchFromFileBundle (VirtualFile::Ptr file) const
    {
        return create (file.get(), {});
    }

    /** Returns the standard shared library filename for the current build platform. */
    static constexpr const char* getLibraryFileName()
    {
       #ifdef _WIN32
        return "SOUL_PatchLoader.dll";
       #elif __APPLE__
        return "SOUL_PatchLoader.dylib";
       #elif __linux__
        return "SOUL_PatchLoader.so";
       #else
        #error "Unknown platform"
       #endif
    }

private:
    void* const handle;
    using GetLibraryVersionFunction = int(*)();
    using CreateInstanceFunction = PatchInstance*(*)(VirtualFile*, const char*);
    CreateInstanceFunction createInstanceFunction = {};

    bool isCompatibleLibraryVersion (int version)         { return version == currentLibraryAPIVersion; }

    PatchInstance::Ptr create (VirtualFile* file, const char* path) const
    {
        if (loadedSuccessfully())
            if (auto c = createInstanceFunction (file, path))
                return PatchInstance::Ptr (c);

        return {};
    }

   #ifdef _WIN32
    static void* openLibrary (const char* name)           { return ::LoadLibraryA (name); }
    static void closeLibrary (void* h)                    { ::FreeLibrary ((HMODULE) h); }
    static void* getFunction (void* h, const char* name)  { return ::GetProcAddress ((HMODULE) h, name); }
   #else
    static void* openLibrary (const char* name)           { return ::dlopen (name, RTLD_LOCAL | RTLD_NOW); }
    static void closeLibrary (void* h)                    { ::dlclose (h); }
    static void* getFunction (void* h, const char* name)  { return ::dlsym (h, name); }
   #endif
};

} // namespace patch
} // namespace soul
