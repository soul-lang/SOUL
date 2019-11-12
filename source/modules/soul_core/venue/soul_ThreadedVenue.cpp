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

    bool connectSessionInputEndpoint (Venue::Session&, soul::InputEndpoint&, EndpointID) override
    {
        return false;
    }

    bool connectSessionOutputEndpoint (Venue::Session&, soul::OutputEndpoint&, EndpointID) override
    {
        return false;
    }

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
                    setState (State::loaded);
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
            setState (State::empty);
        }

        bool start() override
        {
            if (state != State::linked)
                return false;

            SOUL_ASSERT (performer->isLinked());
            waitForThreadToFinish();
            shouldStop = false;
            loadMeasurer.reset();
            renderThread = std::thread ([this] { run(); });
            setState (State::running);
            return true;
        }

        bool isRunning() override
        {
            return state == State::running;
        }

        void stop() override
        {
            if (isRunning())
            {
                shouldStop = true;

                if (std::this_thread::get_id() != renderThread.get_id())
                    waitForThreadToFinish();
            }
        }

        std::vector<InputEndpoint::Ptr>  getInputEndpoints() override       { return performer->getInputEndpoints(); }
        std::vector<OutputEndpoint::Ptr> getOutputEndpoints() override      { return performer->getOutputEndpoints(); }

        bool link (CompileMessageList& messageList, const LinkOptions& linkOptions) override
        {
            if (state == State::loaded && performer->link (messageList, linkOptions, {}))
            {
                setState (State::linked);
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

        void setStateChangeCallback (StateChangeCallbackFn f) override
        {
            stateChangeCallback = std::move (f);
        }

    private:
        ThreadedVenue& venue;
        std::unique_ptr<Performer> performer;
        std::thread renderThread;
        CPULoadMeasurer loadMeasurer;
        StateChangeCallbackFn stateChangeCallback;
        std::atomic<State> state { State::empty };

        std::atomic<bool> shouldStop { false };

        void waitForThreadToFinish()
        {
            SOUL_ASSERT (std::this_thread::get_id() != renderThread.get_id());

            if (renderThread.joinable())
            {
                renderThread.join();
                renderThread = {};
            }
        }

        void setState (State newState)
        {
            if (state != newState)
            {
                state = newState;

                if (stateChangeCallback != nullptr)
                    stateChangeCallback (state);
            }
        }

        void run()
        {
            while (! shouldStop.load())
            {
                loadMeasurer.startMeasurement();
                performer->advance (512);
                loadMeasurer.stopMeasurement();
            }

            setState (State::linked);
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
