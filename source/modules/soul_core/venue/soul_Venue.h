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

        using CompileTaskFinishedCallback = std::function<void(const CompileMessageList&)>;

        //==============================================================================
        /** Contains various indicators of what the venue is currently doing.
            @see getStatus
        */
        struct Status
        {
            SessionState state = SessionState::empty;
            float cpu = 0;
            uint32_t xruns = 0;
            double sampleRate = 0;
            uint32_t blockSize = 0;
        };

        /** Returns the venue's current status. */
        virtual Status getStatus() = 0;

        /** Loads a set of fully-resolved modules.
            If the session is not in a state where loading is possible, this will return false.
            Otherwise, it will and return true and will begin asynchronously compiling and loading
            the program. If the load finishes without interruption, the callback function will be invoked,
            providing a (hopefully empty) list of compile errors. After a successful load is complete, the
            getInputEndpoints() and getOutputEndpoints() methods become available, so you can query
            and connect them to data sources before calling link().
        */
        virtual bool load (const Program&, CompileTaskFinishedCallback loadFinishedCallback) = 0;

        /** When a program has been loaded, this returns a list of the input endpoints that
            the program provides.
        */
        virtual ArrayView<const EndpointDetails> getInputEndpoints() = 0;

        /** When a program has been loaded, this returns a list of the output endpoints that
            the program provides.
        */
        virtual ArrayView<const EndpointDetails> getOutputEndpoints() = 0;

        /** Connects one of the venue's external endpoints to an endpoint in the currently loaded program. */
        virtual bool connectExternalEndpoint (EndpointID programEndpoint, EndpointID externalEndpoint) = 0;

        /** Returns the list of external variables that need to be resolved before a loaded
            program can be linked.
        */
        virtual ArrayView<const ExternalVariable> getExternalVariables() = 0;

        /** Set the value of an external in the loaded program. */
        virtual bool setExternalVariable (const char* name, const choc::value::ValueView& value) = 0;

        /** When a program has been loaded successfully and its endpoints have suitable data
            sources attached, call link() to finish the process of preparing the program to be run.

            If the session is not in a state where linking is possible, this will return false.
            Otherwise, it will return true and begin to asynchronously link the program. If
            If this returns true, then start() can be called to begin playback.
        */
        virtual bool link (const BuildSettings&, CompileTaskFinishedCallback linkFinishedCallback) = 0;

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

        using OutgoingEventHandlerFn = std::function<void(uint64_t frameIndex,
                                                          const std::string& endpointName,
                                                          const choc::value::ValueView& eventData)>;

        /** Returns true if this endpoint is currently active. */
        virtual bool isEndpointActive (const EndpointID&) = 0;

        /** When a program has been loaded (but not yet linked), this returns
            a handle that can be used later by other methods which need to reference
            an input or output endpoint.
            Will return a null handle if the ID is not found.
        */
        virtual EndpointHandle getEndpointHandle (const EndpointID&) = 0;

        //==============================================================================
        struct InputEndpointActions
        {
            virtual ~InputEndpointActions() {}

            /** Pushes a block of samples to an input endpoint.
                This should be called to provide the next block of samples for an input stream.
            */
            virtual void setNextInputStreamFrames (EndpointHandle, const choc::value::ValueView& frameArray) = 0;

            /** Updates the trajectory for a sparse input stream. */
            virtual void setSparseInputStreamTarget (EndpointHandle, const choc::value::ValueView& targetFrameValue, uint32_t numFramesToReachValue) = 0;

            /** Sets a new value for a value input. */
            virtual void setInputValue (EndpointHandle, const choc::value::ValueView& newValue) = 0;

            /** Adds an event to an input queue.
                It can be called multiple times during the same callback to add multiple events for an event input
                endpoint. The events will then all be dispatched in order.
            */
            virtual void addInputEvent (EndpointHandle, const choc::value::ValueView& eventData) = 0;
        };

        struct OutputEndpointActions
        {
            virtual ~OutputEndpointActions() {}

            /** Retrieves the most recent block of frames from an output stream.
                A nullptr return value indicates that no frames are available.
            */
            virtual choc::value::ValueView getOutputStreamFrames (EndpointHandle) = 0;

            /** Retrieves the last block of events which were emitted by an event output.
                It will call the provided handler for each event that was posted during the last block
                to be rendered. The handler function is given the frame offset and content of each event.
            */
            virtual void iterateOutputEvents (EndpointHandle, Performer::HandleNextOutputEventFn) = 0;
        };

        //==============================================================================
        using BeginNextBlockFn = std::function<void(uint32_t totalFrames)>;

        /** A callback function which gives a client the chance to fill endpoints.
            It is given a maximum number of frames which could be done, and it must return the
            actual number of frames which it wants the venue to render. This allows a client
            to subdivide the blocks that get rendered.
        */
        using GetNextNumFramesFn = std::function<uint32_t(uint32_t maxNumFrames)>;

        /** A callback function which gives a client the chance to fill endpoints.
            It is given a maximum number of frames which could be done, and it must return the
            actual number of frames which it wants the venue to render. This allows a client
            to subdivide the blocks that get rendered.
        */
        using PrepareInputsFn = std::function<void(InputEndpointActions&, uint32_t numFrames)>;

        /** A callback which lets a client empty any endpoints which it needs to service after
            a block has been rendered.
        */
        using ReadOutputsFn = std::function<void(OutputEndpointActions&, uint32_t numFrames)>;

        /** Allows client code to get callbacks to . */
        virtual void setIOServiceCallbacks (BeginNextBlockFn, GetNextNumFramesFn, PrepareInputsFn, ReadOutputsFn) = 0;
    };

    //==============================================================================
    using SessionReadyCallback = std::function<void(std::unique_ptr<Session>)>;

    /** Asks for a new session to be created. This happens asynchronously, and the callback
        will be invoked when one is ready.
        If the function returns false then for some reason another session can't be opened.
    */
    virtual bool createSession (SessionReadyCallback sessionReadyCallback) = 0;

    //==============================================================================
    /** Returns a list of any external inputs that the venue provides. */
    virtual ArrayView<const EndpointDetails> getExternalInputEndpoints() = 0;

    /** Returns a list of any external outputs that the venue provides. */
    virtual ArrayView<const EndpointDetails> getExternalOutputEndpoints() = 0;
};


} // namespace soul
