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
    virtual bool load (CompileMessageList&, const Program& programToLoad) = 0;

    /** Unloads any currently loaded program, and resets the state of the performer. */
    virtual void unload() = 0;

    /** When a program has been loaded, this returns a list of the input endpoints that
        the program provides.
    */
    virtual ArrayView<const EndpointDetails> getInputEndpoints() = 0;

    /** When a program has been loaded, this returns a list of the output endpoints that
        the program provides.
    */
    virtual ArrayView<const EndpointDetails> getOutputEndpoints() = 0;

    /** When a program has been loaded (but not yet linked), this returns
        an object that allows user data sources to be attached to the given input.
        Will return a nullptr if the ID is not found.
    */
    virtual InputSource::Ptr getInputSource (const EndpointID&) = 0;

    /** When a program has been loaded (but not yet linked), this returns
        an object that allows user data sources to be attached to the given output.
        Will return a nullptr if the ID is not found.
    */
    virtual OutputSink::Ptr getOutputSink (const EndpointID&) = 0;

    /** After loading a program, and optionally connecting up to some of its endpoints,
        link() will complete any preparations needed before the code can be executed.
        If this returns true, then you can safely start calling advance(). If it
        returns false, the error messages will be added to the CompileMessageList
        provided.
        Note that this method blocks until building is finished, and it's not impossible
        that an optimising JIT engine could take up to several seconds, so make sure
        the caller takes this into account.
    */
    virtual bool link (CompileMessageList&, const LinkOptions&, LinkerCache*) = 0;

    /** Returns true if a program is currently loaded. */
    virtual bool isLoaded() = 0;

    /** Returns true if a program is successfully linked and ready to execute. */
    virtual bool isLinked() = 0;

    /** Resets the performer to the state it was in when freshly linked.
        This doesn't unlink or unload the program, it simply resets the program's
        internal state so that the next advance() call will begin a fresh run.
    */
    virtual void reset() = 0;

    /** Prepares to render a block of samples
     Once a program has been loaded and linked, a caller will typically make repeated
     calls to prepare() and advance() to actually perform the rendering work. Between prepare()
     and advance() the caller will fill input buffers with the prepared number of samples
     and following advance() will retieve the prepared number of samples of output.
     samplesToAdvance must be <= the blockSize specified in the link() options.
     Because you're likely to be calling advance() from an audio thread, be careful not to
     allow any calls to other methods such as unload() to overlap with calls to advance()!
     */
    virtual void prepare (uint32_t samplesToAdvance) =  0;

    /** Renders the prepared block of samples.
     Once a a caller has called prepare() a call to advance() will cause the prepared block of
     samples to be rendered. If any inputs have not been correctly populated, over and underruns
     may occur and the associated counters will be incremented to indicate such issues
    */
    virtual void advance () = 0;

    /** Returns the number of over- or under-runs that have happened since the program was linked.
        Underruns can happen when an endpoint callback fails to empty or fill the amount of data
        that it is asked to handle.
    */
    virtual uint32_t getXRuns() = 0;

    /** Returns the block size which is the maximum number of frames that can be rendered in one
        prepare call
     */
    virtual uint32_t getBlockSize() = 0;
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

} // namespace soul
