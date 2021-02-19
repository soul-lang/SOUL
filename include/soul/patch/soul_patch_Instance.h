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
    Allows the caller to supply a custom class which stores copies of pre-compiled
    binaries, so that the compiler can re-use previously compiled chunks of object code.

    An implementation of this class just needs to store key-value chunks of data in
    some kind of files or database, and retrieve them when asked.
*/
class CompilerCache  : public RefCountedBase
{
public:
    using Ptr = RefCountingPtr<CompilerCache>;

    /** Copies a block of data into the cache with a given key.
        The key will be an alphanumeric hash string of some kind. If there's already a
        matching key in the cache, this should overwrite it with the new data.
        The sourceData pointer will not be null, and the size will be greater than zero.
    */
    virtual void storeItemInCache (const char* key, const void* sourceData, uint64_t size) = 0;

    /**
        The key will be an alphanumeric hash string that was previously used to store the item.
        If destAddress is nullptr or destSize is too small, then this should return the size
        that is required to hold this object.
        If no entry is found for this key, the method returns 0.
    */
    virtual uint64_t readItemFromCache (const char* key, void* destAddress, uint64_t destSize) = 0;
};

//==============================================================================
/**
    Allows the caller to supply a custom class which can act as a pre-processing stage
    for the source files that code into SOUL.

    If one of these objects is provided to the PatchInstance::compileNewPlayer() method,
    then each source file will be passed to preprocessSourceFile() before being compiled,
    which provides the opportunity to either pass it through unchanged, or completely convert
    it to a different format.
    So for example, you could write a pre-processor which modifies the content of .soul files
    in some way (e.g. runs macros, etc). Or you could write something which takes an entirely
    different format (e.g. .faust files) and generates .soul files from them for the compiler
    to use.
*/
class SourceFilePreprocessor  : public RefCountedBase
{
public:
    using Ptr = RefCountingPtr<SourceFilePreprocessor>;

    /** This is called for each source file that the patch contains, before the compiler
        attempts to process it. If this class wants to modify the file, it should return
        a new VirtualFile which the compiler will then use instead of the original.
        If this class doesn't want to modify the file, it can just return a nullptr to let
        the compiler handle the file in its default way.
    */
    virtual VirtualFile* preprocessSourceFile (VirtualFile& inputFile) = 0;
};

//==============================================================================
/**
    Allows the caller to supply a custom class which can supply content for external
    variables that the code needs.

    If one of these objects is provided to the PatchInstance::compileNewPlayer() method,
    then it will be called to retrieve the audio file that should be used for each external
    variable that the code contains.

    If no ExternalDataProvider object is provided, then any externals in the code are resolved
    automatically by looking at the JSON "externals" property in the .soulpatch manifest.
*/
class ExternalDataProvider  : public RefCountedBase
{
public:
    using Ptr = RefCountingPtr<ExternalDataProvider>;

    /** This is called for each external variable that the code needs to resolve.
        If it returns nullptr and no suitable file is found in the "externals" property
        of the .soulpatch manifest, then the compilation will fail. The variable name
        supplied will be fully-qualified, and the method must return an audio file that
        can be successfully loaded to provide suitable data for the type of that variable.
    */
    virtual VirtualFile* getExternalFile (const char* externalVariableName) = 0;
};


//==============================================================================
/** Returned by PatchInstance::getLinkedProgram(), this represents the result of linking
    a patch into a ready-to-run HEART program.
*/
class LinkedProgram  : public RefCountedBase
{
public:
    using Ptr = RefCountingPtr<LinkedProgram>;

    /// Returns a list of any errors or warnings that happened during compilation.
    virtual Span<CompilationMessage> getCompileMessages() const = 0;

    /// Returns the HEART code for the program. You can turn it into an actual
    /// soul::Program object using soul::Program::createFromHEART().
    virtual const char* getHEARTCode() const = 0;
};

//==============================================================================
/**
    Represents an instance of a SOUL patch.

    When you have a PatchInstance, you can use it to compile PatchPlayer objects which
    can be interrogated for their parameters, buses, etc, and used to actually play
    some audio.
*/
class PatchInstance  : public RefCountedBase
{
public:
    using Ptr = RefCountingPtr<PatchInstance>;

    /** Returns the file from which this instance was created. */
    virtual VirtualFile* getLocation() = 0;

    /** Returns an up-to-date description of this patch.
        If the patch is being loaded from a disk file, this method may check it
        for changes and provide the latest version of it.

        Note that if there was an error when parsing the manifest file, the description
        returned will have an null UID and an error message in the description field.
    */
    virtual Description* getDescription() = 0;

    /** Looks at the modification times of all the files that this patch uses, and
        returns the most recent one.
        If something goes wrong, e.g. no files are found, this may return -1.
    */
    virtual int64_t getLastModificationTime() = 0;

    /** Attempts to build a new player for this patch which uses the given config.
        This will always return a new player object, but you should call
        PatchPlayer::isPlayable() on the object before using it to check whether
        an error occurred while building.
        The various other custom callbacks are optional parameters, and can be
        nullptr if not needed.
    */
    virtual PatchPlayer* compileNewPlayer (const PatchPlayerConfiguration&,
                                           CompilerCache* cacheToUse,
                                           SourceFilePreprocessor* preprocessor,
                                           ExternalDataProvider* externalDataProvider) = 0;

    /** For code-generation purposes, this will return a HEART program that can
        be transpiled into other languages like C++. The object that is returned will
        either report some compile errors, or a valid HEART program.
    */
    virtual LinkedProgram* getLinkedProgram (const PatchPlayerConfiguration&,
                                             CompilerCache* cacheToUse,
                                             SourceFilePreprocessor* preprocessor,
                                             ExternalDataProvider* externalDataProvider) = 0;
};


} // namespace patch
} // namespace soul
