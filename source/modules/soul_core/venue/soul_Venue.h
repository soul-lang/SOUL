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
    Abstract base class for a "venue" which hosts sessions.

    A venue has a similar overall API to a performer - they both load, link and
    run programs. But while a performer is synchronous and just crunches the raw
    numbers, a venue runs asynchronously and will use a performer internally to
    play the audio through some kind of real device, either local or remote.
*/
class Venue
{
public:
    Venue() {}

    /** Destructor */
    virtual ~Venue() {}

    //==============================================================================
    /**
    */
    class Session
    {
    public:
        virtual ~Session() {}

        /** Loads a set of fully-resolved modules.
            After load() has returned successfully, the getInputEndpoints() and
            getOutputEndpoints() methods become available, so you can query and connect
            them to data sources before calling link().
        */
        virtual bool load (CompileMessageList&, const Program&) = 0;

        /** When a program has been loaded, this returns a list of the input endpoints that
            the program provides.
        */
        virtual ArrayView<const EndpointDetails> getInputEndpoints() = 0;

        /** When a program has been loaded, this returns a list of the output endpoints that
            the program provides.
        */
        virtual ArrayView<const EndpointDetails> getOutputEndpoints() = 0;

        /** When a program has been loaded and its endpoints have had suitable data
            sources attached, call link() to finish the process of preparing the program
            to be run.
            If this returns true, then start() can be called to begin playback.
        */
        virtual bool link (CompileMessageList&, const LinkOptions&) = 0;

        /** Instructs the venue to begin playback.
            If no program is linked, this will fail and return false.
        */
        virtual bool start() = 0;

        /** Returns true if a program is linked and playing.
            @see getStatus
        */
        virtual bool isRunning() = 0;

        /** Instructs the venue to stop playback. */
        virtual void stop() = 0;

        /** Instructs the venue to stop playback, and to unload the current program. */
        virtual void unload() = 0;

        /** When a program has been loaded (but not yet linked), this returns
            a handle that can be used later by other methods which need to reference
            an input or output endpoint.
            Will return a null handle if the ID is not found.
        */
        virtual EndpointHandle getEndpointHandle (const EndpointID&) = 0;

        /** Pushes a block of samples to an input endpoint.
            This should be called to provide the next block of samples for an input stream.
            This method may only be called during a callback attached to setInputEndpointServiceCallback(),
            using the endpoint handle that was provided as an argument to that callback.
            @returns the number of samples actually consumed. If not all the samples provided
                     are consumed, the caller will probably want to re-send the remaining ones
                     during the next callback
        */
        virtual uint32_t setNextInputStreamFrames (EndpointHandle, const Value& frameArray) = 0;

        /** Updates the trajectory for a sparse input stream.
            This method may only be called during a callback attached to setInputEndpointServiceCallback(),
            using the endpoint handle that was provided as an argument to that callback.
        */
        virtual void setSparseInputStreamTarget (EndpointHandle, const Value& targetFrameValue,
                                                 uint32_t numFramesToReachValue, float curveShape) = 0;

        /** Sets a new value for a value input.
            This method may only be called during a callback attached to setInputEndpointServiceCallback(),
            using the endpoint handle that was provided as an argument to that callback.
        */
        virtual void setInputValue (EndpointHandle, const Value& newValue) = 0;

        /** Adds an event to an input queue.
            This method may only be called during a callback attached to setInputEndpointServiceCallback(),
            using the endpoint handle that was provided as an argument to that callback.
            It can be called multiple times during the same callback to add events for an event input
            endpoint. The events will then all be dispatched in order, and the queue will be reset before
            the next callback.
        */
        virtual void addInputEvent (EndpointHandle, const Value& eventData) = 0;

        /** Retrieves the most recent block of frames from an output stream.
            This method may only be called during a callback attached to setOutputEndpointServiceCallback(),
            using the endpoint handle that was provided as an argument to that callback.
            A nullptr return value indicates that no frames are available.
        */
        virtual const Value* getOutputStreamFrames (EndpointHandle) = 0;

        /** Retrieves the last block of events which were emitted by an event output.
            This method may only be called during a callback attached to setOutputEndpointServiceCallback(),
            using the endpoint handle that was provided as an argument to that callback.
            It will call the provided handler for each event that was posted during the last block
            to be rendered. The handler function is given the frame offset and content of each event.
        */
        virtual void iterateOutputEvents (EndpointHandle, Performer::HandleNextOutputEventFn) = 0;

        /** Returns true if something has got a handle to this endpoint and might be using it
            during the current program run.
        */
        virtual bool isEndpointActive (const EndpointID&) = 0;
        
        /** Represents the overall status of a Venue. */
        enum class State
        {
            empty,
            loaded,
            linked,
            running
        };

        /** Contains various indicators of what the venue is currently doing.
            @see getStatus
        */
        struct Status
        {
            State state;
            float cpu;
            uint32_t xruns;
            double sampleRate;
            uint32_t blockSize;
        };

        /** Returns the venue's current status. */
        virtual Status getStatus() = 0;

        /** Returns the total number of frames which have been rendered since the venue started running
            its current program.
        */
        virtual uint64_t getTotalFramesRendered() const = 0;

        /** A callback function to indicate that the venue's state has changed.
            @see setStateChangeCallback
        */
        using StateChangeCallbackFn = std::function<void(State)>;

        /** A callback function which gives a client the chance to fill an input endpoint when it becomes
            starved or has data ready.
        */
        using EndpointServiceFn = std::function<void (Session&, EndpointHandle)>;

        /** Allows the client code to attach a lambda to be called when the current state changes. */
        virtual void setStateChangeCallback (StateChangeCallbackFn) = 0;

        /** Allows client code to get a callback when the amount of data in an endpoint's FIFO changes. */
        virtual bool setInputEndpointServiceCallback (EndpointID, EndpointServiceFn) = 0;

        /** Allows client code to get a callback when the amount of data in an endpoint's FIFO changes. */
        virtual bool setOutputEndpointServiceCallback (EndpointID, EndpointServiceFn) = 0;
    };

    //==============================================================================
    /** Creates and returns a new session for this venue. */
    virtual std::unique_ptr<Session> createSession() = 0;

    virtual std::vector<EndpointDetails> getSourceEndpoints() = 0;
    virtual std::vector<EndpointDetails> getSinkEndpoints() = 0;

    virtual bool connectSessionInputEndpoint  (Session&, EndpointID inputID, EndpointID venueSourceID) = 0;
    virtual bool connectSessionOutputEndpoint (Session&, EndpointID outputID, EndpointID venueSinkID) = 0;
};

/// Create a standard threaded venue where a separate render thread renders the performer
std::unique_ptr<Venue> createThreadedVenue (std::unique_ptr<PerformerFactory> performerFactory);


} // namespace soul
