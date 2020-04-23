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
    LinkOptions (SampleRateAndBlockSize sampleRateAndMaxBlockSize)  : rateAndMaxBlockSize (sampleRateAndMaxBlockSize) {}
    LinkOptions (double sampleRate, uint32_t maxBlockSize) : LinkOptions (SampleRateAndBlockSize (sampleRate, maxBlockSize)) {}

    //==============================================================================
    void setOptimisationLevel (int level)                   { SOUL_ASSERT (level >= -1 && level <= 3); optimisationLevel = level; }
    int getOptimisationLevel() const                        { return optimisationLevel; }

    //==============================================================================
    void setMaxStateSize (size_t size)                      { maxStateSize = size > 0 ? size : defaultMaximumStateSize; }
    size_t getMaxStateSize() const                          { return maxStateSize; }

    //==============================================================================
    void setMainProcessor (const std::string& name)         { mainProcessor = name; }
    std::string getMainProcessor() const                    { return mainProcessor; }

    //==============================================================================
    void setPlatform (const std::string& name)              { platform = name; }
    std::string getPlatform() const                         { return platform; }

    //==============================================================================
    void setSessionID (int32_t newSessionID)                { sessionID = newSessionID; }
    int32_t getSessionID() const                            { return sessionID; }
    bool hasSessionID() const                               { return sessionID != 0; }

    //==============================================================================
    void setMaxBlockSize (uint32_t newMaxBlockSize)         { rateAndMaxBlockSize.blockSize = newMaxBlockSize; }
    uint32_t getMaxBlockSize() const                        { return rateAndMaxBlockSize.blockSize; }

    //==============================================================================
    void setSampleRate (double newRate)                     { rateAndMaxBlockSize.sampleRate = newRate; }
    double getSampleRate() const                            { return rateAndMaxBlockSize.sampleRate; }

    void setSampleRateAndMaxBlockSize (SampleRateAndBlockSize newRateAndSize)   { rateAndMaxBlockSize = newRateAndSize; }
    SampleRateAndBlockSize getSampleRateAndBlockSize() const                    { return rateAndMaxBlockSize; }

    //==============================================================================
    using ExternalValueProviderFn = std::function<ConstantTable::Handle (ConstantTable&,
                                                                         const char* name,
                                                                         const Type& requiredType,
                                                                         const Annotation& annotation)>;

    /** If this lamdba is set, it must return the Value that should be bound to a
        given external variable. The name provided will be fully-qualified, and the Value
        returned must match the given type, or an error will be thrown.
    */
    ExternalValueProviderFn externalValueProvider;


private:
    //==============================================================================
    static constexpr size_t defaultMaximumStateSize = 1024 * 1024 * 20;
    SampleRateAndBlockSize rateAndMaxBlockSize;
    size_t maxStateSize = defaultMaximumStateSize;
    int optimisationLevel = -1;
    int32_t sessionID = 0;
    std::string mainProcessor, platform;
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
