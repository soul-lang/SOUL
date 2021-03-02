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

class LinkerCache;

//==============================================================================
/**
    Abstract base class for a "performer" which can compile and execute a soul::Program.

    A typical performer is likely to be a JIT compiler or an interpreter.

    Note that performer implementations are not expected to be thread-safe!
    Performers will typically not create any internal threads, and all its methods
    are synchronous (for an asynchronous playback engine, see soul::Venue).
    Any code which uses a performer is responsible for making sure it calls the methods
    in a race-free way, and takes into account the fact that some of the calls may block
    for up to a few seconds.
*/
class Performer
{
public:
    virtual ~Performer() {}

    /** Provides the program for the performer to load.
        If a program is already loaded or linked, calling this should reset the state
        before attempting to load the new one.
        After successfully loading a program, the caller should then connect getter/setter
        callback to any endpoints that it wants to communicate with, and then call link()
        to prepare it for use.
        Note that this method blocks until building is finished, and it's not impossible
        that an optimising JIT engine could take up to several seconds, so make sure
        the caller takes this into account.
        Returns true on success; on failure, the CompileMessageList should contain error
        messages describing what went wrong.
    */
    virtual bool load (CompileMessageList&, const Program& programToLoad) noexcept = 0;

    /** Unloads any currently loaded program, and resets the state of the performer. */
    virtual void unload() noexcept = 0;

    /** When a program has been loaded, this returns a list of the input endpoints that
        the program provides.
    */
    virtual choc::span<const EndpointDetails> getInputEndpoints() noexcept = 0;

    /** When a program has been loaded, this returns a list of the output endpoints that
        the program provides.
    */
    virtual choc::span<const EndpointDetails> getOutputEndpoints() noexcept = 0;

    /** Returns the list of external variables that need to be resolved before a loaded
        program can be linked.
    */
    virtual choc::span<const ExternalVariable> getExternalVariables() noexcept = 0;

    /** Set the value of an external in the loaded program. */
    virtual bool setExternalVariable (const char* name, const choc::value::ValueView& value) noexcept = 0;

    /** After loading a program, and optionally connecting up to some of its endpoints,
        link() will complete any preparations needed before the code can be executed.
        If this returns true, then you can safely start calling advance(). If it
        returns false, the error messages will be added to the CompileMessageList
        provided.
        Note that this method blocks until building is finished, and it's not impossible
        that an optimising JIT engine could take up to several seconds, so make sure
        the caller takes this into account.
    */
    virtual bool link (CompileMessageList&, const BuildSettings&, LinkerCache*) noexcept = 0;

    /** Returns true if a program is currently loaded. */
    virtual bool isLoaded() noexcept = 0;

    /** Returns true if a program is successfully linked and ready to execute. */
    virtual bool isLinked() noexcept = 0;

    /** Resets the performer to the state it was in when freshly linked.
        This doesn't unlink or unload the program, it simply resets the program's
        internal state so that the next advance() call will begin a fresh run.
    */
    virtual void reset() noexcept = 0;

    /** When a program has been loaded (but not yet linked), this returns
        a handle that can be used later by other methods which need to reference
        an input or output endpoint.
        Will return a null handle if the ID is not found.
    */
    virtual EndpointHandle getEndpointHandle (const EndpointID&) noexcept = 0;

    /** Indicates that a block of frames is going to be rendered.

        Once a program has been loaded and linked, a caller will typically make repeated
        calls to prepare() and advance() to actually perform the rendering work.
        Between calls to prepare() and advance(), the caller must fill input buffers with the
        content needed to render the number of frames requested here. Then advance() can be
        called, after which the prepared number of frames of output are ready to be read.
        The value of numFramesToBeRendered must not exceed the block size specified when linking.
        Because you're likely to be calling advance() from an audio thread, be careful not to
        allow any calls to other methods such as unload() to overlap with calls to advance()!
    */
    virtual void prepare (uint32_t numFramesToBeRendered) noexcept = 0;

    /** Callback function used by iterateOutputEvents().
        The frameOffset is relative to the start of the last block that was rendered during advance().
        @returns true to continue iterating, or false to stop.
    */
    using HandleNextOutputEventFn = std::function<bool(uint32_t frameOffset, const choc::value::ValueView& event)>&&;

    /** Pushes a block of samples to an input endpoint.
        After a successful call to prepare(), and before a call to advance(), this should be called
        to provide the next block of samples for an input stream. The value provided should be an
        array of as many frames as was specified in prepare(). If this is called more than once before
        advance(), only the most recent value is used.
        The EndpointHandle is obtained by calling getEndpointHandle().
    */
    virtual void setNextInputStreamFrames (EndpointHandle, const choc::value::ValueView& frameArray) noexcept = 0;

    /** Sets the next levels for a sparse-stream input.
        After a successful call to prepare(), and before a call to advance(), this should be called
        to set the trajectory for a sparse input stream over the next block. If this is called more
        than once before advance(), only the most recent value is used.
        The EndpointHandle is obtained by calling getEndpointHandle().
    */
    virtual void setSparseInputStreamTarget (EndpointHandle, const choc::value::ValueView& targetFrameValue,
                                             uint32_t numFramesToReachValue) noexcept = 0;

    /** Sets a new value for a value input.
        After a successful call to prepare(), and before a call to advance(), this may be called
        to set a new value for a value input. If this is called more than once before advance(),
        only the most recent value is used.
        The EndpointHandle is obtained by calling getEndpointHandle().
    */
    virtual void setInputValue (EndpointHandle, const choc::value::ValueView& newValue) noexcept = 0;

    /** Adds an event to an input queue.
        After a successful call to prepare(), and before a call to advance(), this may be called
        multiple times to add events for an event input endpoint. During the next call to advance,
        all the events that were added will be dispatched in order, and the queue will be reset.
        The EndpointHandle is obtained by calling getEndpointHandle().
    */
    virtual void addInputEvent (EndpointHandle, const choc::value::ValueView& eventData) noexcept = 0;

    /** Retrieves the most recent block of frames from an output stream.
        After a successful call to advance(), this may be called to get the block of frames which
        were rendered during that call. A nullptr return value indicates an error.
    */
    virtual choc::value::ValueView getOutputStreamFrames (EndpointHandle) noexcept = 0;

    /** Retrieves the current value of the value output.
        After a successful call to advance(), this may be called to get the value of the given output.
        A nullptr return value indicates an error.
    */
    virtual choc::value::ValueView getOutputValue (EndpointHandle) noexcept = 0;

    /** Retrieves the last block of events which were emitted by an event output.
        After a successful call to advance(), this may be called to iterate the list of events
        which the program emitted on the given endpoint. The callback function provides the
        frame offset and content of each event.
    */
    virtual void iterateOutputEvents (EndpointHandle, HandleNextOutputEventFn) noexcept = 0;

    /** Renders the next block of frames.

        Once the caller has called prepare(), a call to advance() will synchronously render the next
        block of frames. If any inputs have not been correctly populated, over- and under-runs
        may occur and the associated counters will be incremented to reflect this.
    */
    virtual void advance() noexcept = 0;

    /** Returns true if something has got a handle to this endpoint and might be using it
        during the current program run.
    */
    virtual bool isEndpointActive (const EndpointID&) noexcept = 0;

    /** Returns the latency, in samples, of the currently loaded program. */
    virtual uint32_t getLatency() noexcept = 0;

    /** Returns the number of over- or under-runs that have happened since the program was linked.
        Underruns can happen when an endpoint callback fails to empty or fill the amount of data
        that it is asked to handle.
    */
    virtual uint32_t getXRuns() noexcept = 0;

    /** Returns the block size which is the maximum number of frames that can be rendered in one
        prepare call.
    */
    virtual uint32_t getBlockSize() noexcept = 0;

    /** Returns whether the performer is in an error state
    */
    virtual bool hasError() noexcept = 0;

    /** Returns the error message for the performer - if no error is present, this returns nullptr
    */
    virtual const char* getError() noexcept = 0;
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

//==============================================================================
/**
    Abstract base class for a factory which can construct Performers
*/
class PerformerFactory
{
public:
    virtual ~PerformerFactory() {}

    virtual std::unique_ptr<Performer> createPerformer() = 0;
};


//==============================================================================
/**
    A converter class that can be used to wrap one performer inside another one and forward
    its method calls. Useful if you need to take an existing performer and just intercept a
    couple of methods.
*/
struct PerformerWrapper  : public soul::Performer
{
    PerformerWrapper (std::unique_ptr<soul::Performer> p) : performer (std::move (p)) {}

    bool load (soul::CompileMessageList& list, const soul::Program& p) noexcept override                                    { return performer->load (list, p); }
    choc::span<const soul::EndpointDetails> getInputEndpoints() noexcept override                                           { return performer->getInputEndpoints(); }
    choc::span<const soul::EndpointDetails> getOutputEndpoints() noexcept override                                          { return performer->getOutputEndpoints(); }
    void unload() noexcept override                                                                                         { performer->unload(); }
    void reset() noexcept override                                                                                          { performer->reset(); }
    bool isLoaded() noexcept override                                                                                       { return performer->isLoaded(); }
    bool link (soul::CompileMessageList& ml, const soul::BuildSettings& s, soul::LinkerCache* c) noexcept override          { return performer->link (ml, s, c); }
    bool isLinked() noexcept override                                                                                       { return performer->isLinked(); }
    soul::EndpointHandle getEndpointHandle (const soul::EndpointID& e) noexcept override                                    { return performer->getEndpointHandle (e); }
    choc::span<const soul::ExternalVariable> getExternalVariables() noexcept override                                       { return performer->getExternalVariables(); }
    bool setExternalVariable (const char* name, const choc::value::ValueView& v) noexcept override                          { return performer->setExternalVariable (name, std::move (v)); }
    void setNextInputStreamFrames (soul::EndpointHandle h, const choc::value::ValueView& v) noexcept override               { performer->setNextInputStreamFrames (h, v); }
    void setSparseInputStreamTarget (soul::EndpointHandle h, const choc::value::ValueView& v, uint32_t t) noexcept override { performer->setSparseInputStreamTarget (h, v, t); }
    void setInputValue (soul::EndpointHandle h, const choc::value::ValueView& v) noexcept override                          { performer->setInputValue (h, v); }
    void addInputEvent (soul::EndpointHandle h, const choc::value::ValueView& v) noexcept override                          { performer->addInputEvent (h, v); }
    choc::value::ValueView getOutputStreamFrames (soul::EndpointHandle h) noexcept override                                 { return performer->getOutputStreamFrames (h); }
    choc::value::ValueView getOutputValue (soul::EndpointHandle h) noexcept override                                        { return performer->getOutputValue (h); }
    void iterateOutputEvents (soul::EndpointHandle h, HandleNextOutputEventFn f) noexcept override                          { return performer->iterateOutputEvents (h, std::move (f)); }
    bool isEndpointActive (const soul::EndpointID& e) noexcept override                                                     { return performer->isEndpointActive (e); }
    uint32_t getLatency()  noexcept override                                                                                { return performer->getLatency(); }
    void prepare (uint32_t samples) noexcept override                                                                       { return performer->prepare (samples); }
    void advance() noexcept override                                                                                        { return performer->advance(); }
    uint32_t getXRuns() noexcept override                                                                                   { return performer->getXRuns(); }
    uint32_t getBlockSize() noexcept override                                                                               { return performer->getBlockSize(); }
    bool hasError() noexcept override                                                                                       { return performer->hasError(); }
    const char* getError() noexcept override                                                                                { return performer->getError(); }

    std::unique_ptr<soul::Performer> performer;
};


} // namespace soul
