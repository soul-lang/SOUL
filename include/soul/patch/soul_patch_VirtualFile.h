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
/**
    Allows the user to define a custom class for representing a file, so that
    any kind of virtual filesystems can be used to load bundles.
*/
class VirtualFile  : public RefCountedBase
{
public:
    using Ptr = RefCountingPtr<VirtualFile>;

    /** Returns the filename portion of this file. */
    virtual String* getName() = 0;

    /** Returns an absolute path for this file, if such a thing is appropriate. */
    virtual String* getAbsolutePath() = 0;

    /** Returns the parent folder of this file, or nullptr if that isn't possible. */
    virtual VirtualFile* getParent() = 0;

    /** Returns the file found at the given relative path, assuming this object is a folder.
        If there's no such child, it may just return a nullptr.
    */
    virtual VirtualFile* getChildFile (const char* subPath) = 0;

    /** Returns the file size, or -1 if unknown. */
    virtual int64_t getSize() = 0;

    /** Returns the last modification time as milliseconds since the epoch, or -1
        if the file doesn't exist. If the object refers to something for which the
        concept of a modification time makes no sense, it can just return 0.
    */
    virtual int64_t getLastModificationTime() = 0;

    /** Reads a chunk of the file.
        Returns the number of bytes successfully read, or -1 if an error occurred.
    */
    virtual int64_t read (uint64_t startPositionInFile, void* targetBuffer, uint64_t bytesToRead) = 0;
};


} // namespace patch
} // namespace soul
