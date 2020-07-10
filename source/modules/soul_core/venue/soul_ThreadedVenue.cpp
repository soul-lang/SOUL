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

#if ! SOUL_INSIDE_CORE_CPP
 #error "Don't add this cpp file to your build, it gets included indirectly by soul_core.cpp"
#endif

namespace soul
{

//==============================================================================
struct ThreadedVenue  : public soul::Venue
{
    ThreadedVenue (std::unique_ptr<PerformerFactory> p) : performerFactory (std::move (p)) {}
    ~ThreadedVenue() override {}

    std::unique_ptr<Venue::Session> createSession() override
    {
        return std::make_unique<ThreadedVenueSession> (*this, performerFactory->createPerformer());
    }

    std::vector<EndpointDetails> getSourceEndpoints() override    { return {}; }
    std::vector<EndpointDetails> getSinkEndpoints() override      { return {}; }

    //==============================================================================
    struct ThreadedVenueSession    : public Venue::Session
    {
        ThreadedVenueSession (ThreadedVenue& v, std::unique_ptr<soul::Performer> p)
            : venue (v), performer (std::move (p))
        {
            SOUL_ASSERT (performer != nullptr);
        }

        ~ThreadedVenueSession() override
        {
            unload();
            venue.sessionDeleted (this);
        }

        bool load (CompileMessageList& messageList, const Program& p) override
        {
            if (! p.isEmpty())
            {
                unload();

                if (performer->load (messageList, p))
                {
                    setState (SessionState::loaded);
                    return true;
                }
            }

            return false;
        }

        void unload() override
        {
            stop();
            waitForThreadToFinish();
            performer->unload();
            setState (SessionState::empty);
        }

        bool start() override
        {
            if (state != SessionState::linked)
                return false;

            SOUL_ASSERT (performer->isLinked());
            waitForThreadToFinish();
            shouldStop = false;
            loadMeasurer.reset();
            renderThread = std::thread ([this] { run(); });
            setState (SessionState::running);
            return true;
        }

        bool isRunning() override
        {
            return state == SessionState::running;
        }

        void stop() override
        {
            if (isRunning())
            {
                shouldStop = true;

                if (std::this_thread::get_id() != renderThread.get_id())
                    waitForThreadToFinish();

                totalFramesRendered = 0;
            }
        }

        bool connectSessionInputEndpoint (EndpointID, EndpointID) override        { return false; }
        bool connectSessionOutputEndpoint (EndpointID, EndpointID) override       { return false; }

        ArrayView<const EndpointDetails> getInputEndpoints() override   { return performer->getInputEndpoints(); }
        ArrayView<const EndpointDetails> getOutputEndpoints() override  { return performer->getOutputEndpoints(); }

        void setEndpointActive (const EndpointID& endpointID) override  { performer->getEndpointHandle (endpointID); }

        void setNextInputStreamFrames (EndpointHandle handle, const choc::value::ValueView& frameArray) override
        {
            performer->setNextInputStreamFrames (handle, frameArray);
        }

        void setSparseInputStreamTarget (EndpointHandle handle, const choc::value::ValueView& targetFrameValue, uint32_t numFramesToReachValue, float curveShape) override
        {
            performer->setSparseInputStreamTarget (handle, targetFrameValue, numFramesToReachValue, curveShape);
        }

        void setInputValue (EndpointHandle handle, const choc::value::ValueView& newValue) override
        {
            performer->setInputValue (handle, newValue);
        }

        void addInputEvent (EndpointHandle handle, const choc::value::ValueView& eventData) override
        {
            performer->addInputEvent (handle, eventData);
        }

        choc::value::ValueView getOutputStreamFrames (EndpointHandle handle) override
        {
            return performer->getOutputStreamFrames (handle);
        }

        void iterateOutputEvents (EndpointHandle handle, Performer::HandleNextOutputEventFn fn) override
        {
            performer->iterateOutputEvents (handle, std::move (fn));
        }

        bool isEndpointActive (const EndpointID& e) override
        {
            return performer->isEndpointActive (e);
        }

        bool link (CompileMessageList& messageList, const BuildSettings& settings) override
        {
            if (state == SessionState::loaded && performer->link (messageList, settings, {}))
            {
                blockSize = performer->getBlockSize();
                setState (SessionState::linked);
                return true;
            }

            return false;
        }

        Status getStatus() override
        {
            Status s;
            s.state = state;
            s.cpu = loadMeasurer.getCurrentLoad();
            s.xruns = performer->getXRuns();
            return s;
        }

        void setStateChangeCallback (StateChangeCallbackFn f) override     { stateChangeCallback = std::move (f); }

        uint64_t getTotalFramesRendered() const override                   { return totalFramesRendered; }

        bool setInputEndpointServiceCallback (EndpointID endpoint, EndpointServiceFn callback) override
        {
            if (! containsEndpoint (performer->getInputEndpoints(), endpoint))
                return false;

            inputCallbacks.push_back ({ performer->getEndpointHandle (endpoint), std::move (callback) });
            return true;
        }

        bool setOutputEndpointServiceCallback (EndpointID endpoint, EndpointServiceFn callback) override
        {
            if (! containsEndpoint (performer->getOutputEndpoints(), endpoint))
                return false;

            outputCallbacks.push_back ({ performer->getEndpointHandle (endpoint), std::move (callback) });
            return true;
        }

    private:
        ThreadedVenue& venue;
        std::unique_ptr<Performer> performer;
        std::thread renderThread;
        CPULoadMeasurer loadMeasurer;
        StateChangeCallbackFn stateChangeCallback;
        std::atomic<SessionState> state { SessionState::empty };
        std::atomic<bool> shouldStop { false };
        std::atomic<uint64_t> totalFramesRendered { 0 };
        uint32_t blockSize = 0;

        struct EndpointCallback
        {
            EndpointHandle endpointHandle;
            EndpointServiceFn callback;
        };

        std::vector<EndpointCallback> inputCallbacks, outputCallbacks;

        void waitForThreadToFinish()
        {
            SOUL_ASSERT (std::this_thread::get_id() != renderThread.get_id());

            if (renderThread.joinable())
            {
                renderThread.join();
                renderThread = {};
            }
        }

        void setState (SessionState newState)
        {
            if (state != newState)
            {
                state = newState;

                if (stateChangeCallback != nullptr)
                    stateChangeCallback (state);
            }
        }

        void handleError (const std::string&)
        {
        }

        void run()
        {
            try
            {
                while (! shouldStop.load())
                {
                    loadMeasurer.startMeasurement();
                    performer->prepare (blockSize);

                    for (auto& c : inputCallbacks)
                        c.callback (*this, c.endpointHandle);

                    performer->advance();

                    for (auto& c : outputCallbacks)
                        c.callback (*this, c.endpointHandle);

                    totalFramesRendered += blockSize;
                    loadMeasurer.stopMeasurement();
                }
            }
            catch (choc::value::Error e)
            {
                handleError (e.description);
            }
            catch (...)
            {
                handleError ("Uncaught exception");
            }

            setState (SessionState::linked);
        }
    };

private:
    std::unique_ptr<PerformerFactory> performerFactory;
    std::vector<Session*> sessions;

    void sessionDeleted (ThreadedVenueSession* session)
    {
        removeIf (sessions, [=] (Session* s) { return s == session; });
    }
};

std::unique_ptr<Venue> createThreadedVenue (std::unique_ptr<PerformerFactory> performerFactory)
{
    return std::make_unique<ThreadedVenue> (std::move (performerFactory));
}

} // namespace soul
