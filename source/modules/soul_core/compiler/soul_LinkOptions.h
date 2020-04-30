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
/**
    A set of named properties that are passed into the linker and performers.
*/
struct LinkOptions
{
    /** Create a new LinkOptions object. The only non-optional property is sample rate, so it must be supplied here. */
    LinkOptions (double sampleRateToUse)                    { setSampleRate (sampleRateToUse); }

    //==============================================================================
    /** Sets the optimisation level: -1 for default, or 0 to 3 for the usual -O0 to -O3 levels. */
    void setOptimisationLevel (int level)                   { SOUL_ASSERT (level >= -1 && level <= 3); optimisationLevel = level; }
    /** Gets the optimisation level: -1 for default, or 0 to 3 for the usual -O0 to -O3 levels. */
    int getOptimisationLevel() const                        { return optimisationLevel; }

    //==============================================================================
    /** Sets the maximum allowable size for the processor state in bytes. Zero means a "default" size. */
    void setMaxStateSize (size_t size)                      { SOUL_ASSERT (size == 0 || size > 4096); maxStateSize = size; }
    /** Gets the maximum allowable size for the processor state in bytes. Zero means a "default" size. */
    size_t getMaxStateSize() const                          { return maxStateSize; }

    //==============================================================================
    /** Optionally sets the name of the main processor to run when the program is linked.
        If specified, this will override any [[main]] annotations in the program itself.
    */
    void setMainProcessor (const std::string& name)         { mainProcessor = name; }

    /** Gets the name of the main processor to run when the program is linked.
        This will be empty if a default is to be used.
    */
    std::string getMainProcessor() const                    { return mainProcessor; }

    //==============================================================================
    /** Sets a session ID to use when instantiating the program. Use zero to indicate that a random number should be used. */
    void setSessionID (int32_t newSessionID)                { sessionID = newSessionID; }
    /** Gets the session ID to use when instantiating the program. Zero indicates that a random number should be used. */
    int32_t getSessionID() const                            { return sessionID; }

    //==============================================================================
    /** Sets a maximum number of frames that the compiled processor should be able to handle in a single chunk. Use zero for a default. */
    void setMaxBlockSize (uint32_t newMaxBlockSize)         { maxBlockSize = newMaxBlockSize; }
    /** Gets the maximum number of frames that the compiled processor should be able to handle in a single chunk. Returns zero for a default. */
    uint32_t getMaxBlockSize() const                        { return maxBlockSize; }

    //==============================================================================
    /** Sets the sample rate at which the compiled processor is going to run. This must be a valid value. */
    void setSampleRate (double newRate)                     { SOUL_ASSERT (newRate > 0); sampleRate = newRate; }
    /** Returns the sample rate at which the compiled processor is going to run. */
    double getSampleRate() const                            { return sampleRate; }

private:
    //==============================================================================
    double       sampleRate         = 0;
    uint32_t     maxBlockSize       = 0;
    size_t       maxStateSize       = 0;
    int          optimisationLevel  = -1;
    int32_t      sessionID          = 0;
    std::string  mainProcessor;
};

//==============================================================================
/**
    Provides a mechanism that a Performer may use to store and retrieve reusable
    chunks of binary code, to avoid re-compiling things multiple times.

    An implementation just has to store chunks of data for particular string keys. That
    could be done in some kind of file structure or database depending on the use-case.
*/
class LinkerCache
{
public:
    virtual ~LinkerCache() {}

    /** Copies a block of data into the cache with a given key.
        The key will be an alphanumeric hash string of some kind. If there's already a
        matching key in the cache, this should overwrite it with the new data.
        The sourceData pointer will not be null, and the size will be greater than zero.
    */
    virtual void storeItem (const char* key, const void* sourceData, uint64_t size) = 0;

    /**
        The key will be an alphanumeric hash string that was previously used to store the item.
        If destAddress is nullptr or destSize is too small, then this should return the size
        that is required to hold this object.
        If no entry is found for this key, the method returns 0.
    */
    virtual uint64_t readItem (const char* key, void* destAddress, uint64_t destSize) = 0;
};

}
