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
struct TaskThread
{
    TaskThread()
    {
        queues.reserve (8);
        thread = std::thread ([this] { run(); });
    }

    ~TaskThread()
    {
        shutdown();
    }

    void sendShutdownSignal()
    {
        shuttingDown = 1;
        std::lock_guard<std::mutex> l (queueLock);

        for (auto& q : queues)
            q->cancelPendingTasks();

        checkQueues.notify_all();
    }

    void waitForThreadToFinish()
    {
        SOUL_ASSERT (std::this_thread::get_id() != thread.get_id());
        SOUL_ASSERT (shuttingDown);

        if (thread.joinable())
        {
            thread.join();
            thread = {};
        }
    }

    void shutdown()
    {
        sendShutdownSignal();
        waitForThreadToFinish();
        queues.clear();
    }

    using ShouldStopFlag = std::atomic<int>;

    //==============================================================================
    struct Queue
    {
        Queue (TaskThread& t)  : ownerThread (t)
        {
            tasks.reserve (16);
        }

        using TaskFunction = std::function<void(ShouldStopFlag&)>;

        void attach()  { ownerThread.addQueue (*this); }
        void detach()  { ownerThread.removeQueue (*this); }

        void addTask (TaskFunction&& task)
        {
            std::lock_guard<std::mutex> l (taskListLock);

            if (ownerThread.shuttingDown)
                return;

            auto taskHolder = std::make_unique<TaskHolder>();
            taskHolder->function = std::move (task);
            tasks.emplace_back (std::move (taskHolder));
            ownerThread.checkQueues.notify_all();
        }

    private:
        TaskThread& ownerThread;
        friend struct TaskThread;

        struct TaskHolder
        {
            TaskFunction function;
            ShouldStopFlag cancelled { 0 };
        };

        std::vector<std::unique_ptr<TaskHolder>> tasks;
        std::mutex taskListLock;

        bool serviceNextTask()
        {
            std::unique_lock<std::mutex> l (taskListLock);

            if (tasks.empty())
                return false;

            auto task = std::move (tasks.front());
            tasks.erase (tasks.begin());
            l.unlock();

            if (! task->cancelled)
                task->function (task->cancelled);

            return true;
        }

        void cancelPendingTasks()
        {
            std::lock_guard<std::mutex> l (taskListLock);

            for (auto& t : tasks)
                t->cancelled = 1;

            ownerThread.checkQueues.notify_all();
        }
    };

private:
    //==============================================================================
    std::thread thread;
    ShouldStopFlag shuttingDown { 0 };

    std::vector<Queue*> queues;
    std::mutex queueLock;
    std::condition_variable checkQueues;

    void run()
    {
        while (! shuttingDown)
        {
            bool anyTasksDone = false;
            std::unique_lock<std::mutex> l (queueLock);

            // NB: avoid iterator because the vector can be modified during the loop
            for (size_t i = 0; i < queues.size(); ++i)
            {
                if (shuttingDown)
                    return;

                if (queues[i]->serviceNextTask())
                    anyTasksDone = true;
            }

            if (! anyTasksDone)
                checkQueues.wait_for (l, std::chrono::milliseconds (500));
        }
    }

    void addQueue (Queue& queue)
    {
        std::unique_lock<std::mutex> l;

        if (std::this_thread::get_id() != thread.get_id())
            l = std::unique_lock<std::mutex> (queueLock);

        queues.push_back (std::addressof (queue));
    }

    void removeQueue (Queue& queue)
    {
        std::unique_lock<std::mutex> l;

        if (std::this_thread::get_id() != thread.get_id())
            l = std::unique_lock<std::mutex> (queueLock);

        removeIf (queues, [&] (Queue* q) { return q == std::addressof (queue); });
    }
};

//==============================================================================
struct RenderingVenue::Pimpl
{
    Pimpl (std::unique_ptr<PerformerFactory> p)
        : performerFactory (std::move (p)),
          taskThread(),
          createSessionQueue (taskThread)
    {
        createSessionQueue.attach();
        loadMeasurer.reset();
    }

    ~Pimpl()
    {
        std::lock_guard<std::mutex> l (sessionListLock);
        SOUL_ASSERT (activeSessions.empty());
        createSessionQueue.detach();
        taskThread.shutdown();
    }

    //==============================================================================
    struct SessionImpl  : public Venue::Session
    {
        SessionImpl (Pimpl& v, std::unique_ptr<soul::Performer> p)
            : venue (v),
              taskQueue (venue.taskThread),
              performer (std::move (p))
        {
            SOUL_ASSERT (performer != nullptr);
            taskQueue.attach();
        }

        ~SessionImpl() override
        {
            taskQueue.detach();
            venue.removeActiveSession (*this);
        }

        bool load (const Program& program, CompileTaskFinishedCallback loadFinishedCallback) override
        {
            unload();

            if (program.isEmpty())
                return false;

            taskQueue.addTask ([this, program,
                                callback = std::move (loadFinishedCallback)] (TaskThread::ShouldStopFlag& cancelled)
            {
                CompileMessageList messageList;
                bool ok = performer->load (messageList, program);

                if (cancelled)
                    return;

                if (ok)
                    setState (SessionState::loaded);

                callback (messageList);
            });

            return true;
        }

        void unload() override
        {
            stop();

            taskQueue.addTask ([this] (TaskThread::ShouldStopFlag&)
            {
                performer->unload();
                setState (SessionState::empty);
            });
        }

        bool start() override
        {
            taskQueue.addTask ([this] (TaskThread::ShouldStopFlag&)
            {
                if (state == SessionState::linked)
                {
                    setState (SessionState::running);
                    venue.addActiveSession (*this);
                }
            });

            return true;
        }

        bool isRunning() override
        {
            return state == SessionState::running;
        }

        void stop() override
        {
            taskQueue.addTask ([this] (TaskThread::ShouldStopFlag&)
            {
                if (isRunning())
                {
                    venue.removeActiveSession (*this);
                    totalFramesRendered = 0;
                    setState (SessionState::linked);
                }
            });
        }

        //==============================================================================
        // TODO: performer access needs locking
        ArrayView<const EndpointDetails> getInputEndpoints() override       { return performer->getInputEndpoints(); }
        ArrayView<const EndpointDetails> getOutputEndpoints() override      { return performer->getOutputEndpoints(); }

        bool connectExternalEndpoint (EndpointID, EndpointID) override      { return false; }

        ArrayView<const ExternalVariable> getExternalVariables() override                      { return performer->getExternalVariables(); }
        bool setExternalVariable (const char* name, const choc::value::ValueView& v) override  { return performer->setExternalVariable (name, v); }

        //==============================================================================
        EndpointHandle getEndpointHandle (const EndpointID& e) override     { return performer->getEndpointHandle (e); }
        bool isEndpointActive (const EndpointID& e) override                { return performer->isEndpointActive (e); }

        //==============================================================================
        bool link (const BuildSettings& settings, CompileTaskFinishedCallback linkFinishedCallback) override
        {
            taskQueue.addTask ([this, settings, callback = std::move (linkFinishedCallback)] (TaskThread::ShouldStopFlag& cancelled)
            {
                if (state == SessionState::loaded)
                {
                    CompileMessageList messageList;
                    bool ok = performer->link (messageList, settings, {});

                    if (cancelled)
                        return;

                    maxBlockSize = performer->getBlockSize();
                    SOUL_ASSERT (maxBlockSize != 0);
                    callback (messageList);

                    if (ok)
                        setState (SessionState::linked);
                }
            });

            return true;
        }

        Status getStatus() override
        {
            Status s;
            s.state = state;
            s.cpu = venue.loadMeasurer.getCurrentLoad();
            s.xruns = performer->getXRuns();
            return s;
        }

        //==============================================================================
        void setIOServiceCallbacks (BeginNextBlockFn start, GetNextNumFramesFn size, PrepareInputsFn pre, ReadOutputsFn post) override
        {
            beginNextBlockCallback = std::move (start);
            getBlockSizeCallback = std::move (size);
            preRenderCallback = std::move (pre);
            postRenderCallback = std::move (post);
        }

        struct InputActions : public InputEndpointActions
        {
            InputActions (Performer& p) : perf (p) {}
            Performer& perf;

            void setNextInputStreamFrames (EndpointHandle handle, const choc::value::ValueView& frameArray) override
            {
                perf.setNextInputStreamFrames (handle, frameArray);
            }

            void setSparseInputStreamTarget (EndpointHandle handle, const choc::value::ValueView& targetFrameValue, uint32_t numFramesToReachValue) override
            {
                perf.setSparseInputStreamTarget (handle, targetFrameValue, numFramesToReachValue);
            }

            void setInputValue (EndpointHandle handle, const choc::value::ValueView& newValue) override
            {
                perf.setInputValue (handle, newValue);
            }

            void addInputEvent (EndpointHandle handle, const choc::value::ValueView& eventData) override
            {
                perf.addInputEvent (handle, eventData);
            }
        };

        struct OutputActions  : public OutputEndpointActions
        {
            OutputActions (Performer& p) : perf (p) {}
            Performer& perf;

            choc::value::ValueView getOutputStreamFrames (EndpointHandle handle) override
            {
                return perf.getOutputStreamFrames (handle);
            }

            void iterateOutputEvents (EndpointHandle handle, Performer::HandleNextOutputEventFn fn) override
            {
                perf.iterateOutputEvents (handle, std::move (fn));
            }
        };

        void render (uint32_t numFrames)
        {
            if (beginNextBlockCallback != nullptr)
                beginNextBlockCallback (numFrames);

            while (numFrames != 0)
            {
                auto framesToDo = std::min (maxBlockSize, numFrames);

                if (getBlockSizeCallback != nullptr)
                {
                    framesToDo = std::min (framesToDo, getBlockSizeCallback (framesToDo));

                    if (framesToDo == 0)
                    {
                        SOUL_ASSERT_FALSE; // should handle this error case?
                        break;
                    }
                }

                performer->prepare (framesToDo);

                if (preRenderCallback != nullptr)
                {
                    InputActions actions (*performer);
                    preRenderCallback (actions, framesToDo);
                }

                performer->advance();

                if (postRenderCallback != nullptr)
                {
                    OutputActions actions (*performer);
                    postRenderCallback (actions, framesToDo);
                }

                totalFramesRendered += framesToDo;
                numFrames -= framesToDo;
            }
        }

        uint32_t maxBlockSize = 0;

    private:
        RenderingVenue::Pimpl& venue;
        TaskThread::Queue taskQueue;
        std::unique_ptr<Performer> performer;
        std::atomic<SessionState> state { SessionState::empty };
        std::atomic<uint64_t> totalFramesRendered { 0 };

        BeginNextBlockFn beginNextBlockCallback;
        GetNextNumFramesFn getBlockSizeCallback;
        PrepareInputsFn preRenderCallback;
        ReadOutputsFn postRenderCallback;

        void setState (SessionState newState)
        {
            state = newState;
        }
    };

    //==============================================================================
    std::unique_ptr<PerformerFactory> performerFactory;
    TaskThread taskThread;
    TaskThread::Queue createSessionQueue;

    std::mutex sessionListLock;
    std::vector<SessionImpl*> activeSessions;

    CPULoadMeasurer loadMeasurer;

    //==============================================================================
    bool createSession (SessionReadyCallback cb)
    {
        createSessionQueue.addTask ([this, callback = std::move (cb)] (TaskThread::ShouldStopFlag&)
        {
            callback (std::make_unique<Pimpl::SessionImpl> (*this, performerFactory->createPerformer()));
        });

        return true;
    }

    void addActiveSession (SessionImpl& session)
    {
        std::lock_guard<std::mutex> l (sessionListLock);
        activeSessions.push_back (std::addressof (session));
    }

    void removeActiveSession (SessionImpl& session)
    {
        std::lock_guard<std::mutex> l (sessionListLock);
        removeIf (activeSessions, [&] (SessionImpl* s) { return s == std::addressof (session); });
    }

    const char* renderActiveSessions (uint32_t numFrames)
    {
        loadMeasurer.startMeasurement();

        {
            std::lock_guard<std::mutex> l (sessionListLock);

            if (numFrames == 0)
                return "Illegal frame count";

            for (auto& s : activeSessions)
                s->render (numFrames);
        }

        loadMeasurer.stopMeasurement();
        return {};
    }
};

//==============================================================================
RenderingVenue::RenderingVenue (std::unique_ptr<PerformerFactory> p)
    : pimpl (std::make_unique<Pimpl> (std::move (p)))
{
}

RenderingVenue::~RenderingVenue()
{
    pimpl.reset();
}

bool RenderingVenue::createSession (SessionReadyCallback callback)
{
    return pimpl->createSession (std::move (callback));
}

ArrayView<const EndpointDetails> RenderingVenue::getExternalInputEndpoints()    { return {}; }
ArrayView<const EndpointDetails> RenderingVenue::getExternalOutputEndpoints()   { return {}; }

const char* RenderingVenue::render (uint32_t numFrames)
{
    try
    {
        return pimpl->renderActiveSessions (numFrames);
    }
    catch (choc::value::Error e)
    {
        return e.description;
    }

    return "Uncaught exception";
}


} // namespace soul
