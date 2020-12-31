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

namespace soul::audioplayer
{

struct AudioPlayerVenue::Pimpl  : private AudioMIDISystem::Callback
{
    Pimpl (AudioPlayerVenue& v, const Requirements& r, std::unique_ptr<PerformerFactory> f)
       : venue (v), audioSystem (std::move (r)), renderingVenue (std::move (f))
    {
        audioSystem.setCallback (this);
    }

    ~Pimpl() override
    {
        audioSystem.setCallback (nullptr);
    }

    //==============================================================================
    struct AudioVenueSession  : public Venue::Session
    {
        AudioVenueSession (std::unique_ptr<Session> s, AudioPlayerVenue& v)
            : session (std::move (s)), venue (v)
        {
            session->setIOServiceCallbacks ([this] (uint32_t numFrames) { startNextBlock (numFrames); },
                                            [this] (uint32_t maxFrames) { return getNextNumFrames (maxFrames); },
                                            [this] (InputEndpointActions& actions, uint32_t numFrames) { preProcess (actions, numFrames); },
                                            [this] (OutputEndpointActions& actions, uint32_t numFrames) { postProcess (actions, numFrames); });
        }

        ~AudioVenueSession() override
        {
            unload();
        }

        bool load (BuildBundle build, CompileTaskFinishedCallback loadFinishedCallback) override
        {
            return session->load (std::move (build),
                                  [this, clientCallback = std::move (loadFinishedCallback)] (const CompileMessageList& messages)
                                  {
                                      clientCallback (messages);
                                      postLoadSetup();
                                  });
        }

        void unload() override
        {
            externalInputConnections.clear();
            externalOutputConnections.clear();
            session->unload();
        }

        bool start() override            { return session->start(); }
        bool isRunning() override        { return session->isRunning(); }
        void stop() override             { session->stop(); }
        Status getStatus() override      { return session->getStatus(); }

        //==============================================================================
        ArrayView<const EndpointDetails> getInputEndpoints() override   { return session->getInputEndpoints(); }
        ArrayView<const EndpointDetails> getOutputEndpoints() override  { return session->getOutputEndpoints(); }

        //==============================================================================
        EndpointHandle getEndpointHandle (const EndpointID& e) override   { return session->getEndpointHandle (e); }
        bool isEndpointActive (const EndpointID& e) override              { return session->isEndpointActive (e); }

        //==============================================================================
        void setIOServiceCallbacks (BeginNextBlockFn start, GetNextNumFramesFn size, PrepareInputsFn pre, ReadOutputsFn post) override
        {
            beginNextBlockCallback = std::move (start);
            getBlockSizeCallback = std::move (size);
            preRenderCallback = std::move (pre);
            postRenderCallback = std::move (post);
        }

        bool link (const BuildSettings& settings, CompileTaskFinishedCallback linkFinishedCallback) override
        {
            return session->link (settings, std::move (linkFinishedCallback));
        }

        bool connectExternalEndpoint (EndpointID sessionEndpoint, EndpointID externalEndpoint) override
        {
            if (auto external = venue.pimpl->findExternalEndpoint (externalEndpoint))
            {
                if (external->isInput)
                {
                    if (auto programIn = findInternalEndpoint (session->getInputEndpoints(), sessionEndpoint))
                    {
                        if (external->isMIDI)
                        {
                            if (isMIDIEventEndpoint (*programIn))
                            {
                                externalInputConnections.emplace_back (ExternalConnection { sessionEndpoint, externalEndpoint });
                                return true;
                            }
                        }
                        else
                        {
                            auto numChannelsOut = getNumAudioChannels (*programIn);

                            if (numChannelsOut != 0)
                            {
                                externalInputConnections.emplace_back (ExternalConnection { sessionEndpoint, externalEndpoint });
                                return true;
                            }
                        }
                    }
                }
                else
                {
                    if (auto programOut = findInternalEndpoint (session->getOutputEndpoints(), sessionEndpoint))
                    {
                        if (external->isMIDI)
                            return false;  // MIDI out - currently ignored

                        auto numChannelsIn = getNumAudioChannels (*programOut);

                        if (numChannelsIn != 0)
                        {
                            externalOutputConnections.emplace_back (ExternalConnection { sessionEndpoint, externalEndpoint });
                            return true;
                        }
                    }
                }
            }

            return false;
        }

        void postLoadSetup()
        {
            midiInputList.clear();
            audioInputList.initialise (maxBlockSize);
            audioOutputList.clear();

            eventOutputList.clear();

            for (auto& e : getOutputEndpointsOfType (*this, OutputEndpointType::event))
                eventOutputList.connectEndpoint (*this, e.endpointID);

            // in lieu of handling midi out, just send it along with other output events..
            for (auto& e : getOutputEndpointsOfType (*this, OutputEndpointType::midi))
                eventOutputList.connectEndpoint (*this, e.endpointID);

            for (auto& c : externalInputConnections)
            {
                if (auto e = venue.pimpl->findExternalEndpoint (c.externalEndpoint))
                {
                    if (e->isMIDI)
                        midiInputList.connectEndpoint (*session, c.sessionEndpoint);
                    else
                        audioInputList.connectEndpoint (*session, c.sessionEndpoint, e->channels);
                }
            }

            for (auto& c : externalOutputConnections)
            {
                if (auto e = venue.pimpl->findExternalEndpoint (c.externalEndpoint))
                    audioOutputList.connectEndpoint (*session, c.sessionEndpoint, e->channels);
            }
        }

        void startNextBlock (uint32_t numFrames)
        {
            SOUL_ASSERT (numFrames <= maxBlockSize);
            auto framePos = venue.pimpl->totalFramesRendered;
            audioInputList.addToFIFO (inputFIFO, framePos, venue.pimpl->currentInputBuffer);
            midiInputList.addToFIFO (inputFIFO, framePos, venue.pimpl->currentMIDIBuffer);

            frameOffset = 0;
            inputFIFO.prepareForReading (venue.pimpl->totalFramesRendered, numFrames);

            if (beginNextBlockCallback != nullptr)
                beginNextBlockCallback (numFrames);
        }

        uint32_t getNextNumFrames (uint32_t maxNumFrames)
        {
            if (getBlockSizeCallback != nullptr)
                maxNumFrames = getBlockSizeCallback (maxNumFrames);

            return inputFIFO.getNumFramesInNextChunk (maxNumFrames);
        }

        void preProcess (InputEndpointActions& actions, uint32_t numFrames)
        {
            if (preRenderCallback != nullptr)
                preRenderCallback (actions, numFrames);

            inputFIFO.processNextChunk ([&] (soul::EndpointHandle endpoint, uint64_t /*frame*/,
                                             const choc::value::ValueView& value)
            {
                switch (endpoint.getType())
                {
                    case EndpointType::stream:
                        SOUL_TODO // need to also handle sparse streams
                        actions.setNextInputStreamFrames (endpoint, value);
                        break;

                    case EndpointType::event:
                        actions.addInputEvent (endpoint, value);
                        break;

                    case EndpointType::value:
                        actions.setInputValue (endpoint, value);
                        break;

                    case EndpointType::unknown:
                    default:
                        SOUL_ASSERT_FALSE;
                }
            });
        }

        void postProcess (OutputEndpointActions& actions, uint32_t numFrames)
        {
            if (postRenderCallback != nullptr)
                postRenderCallback (actions, numFrames);

            auto output = venue.pimpl->currentOutputBuffer;
            audioOutputList.handleOutputData (actions, output.getFrameRange ({ frameOffset, frameOffset + numFrames }));
            eventOutputList.postOutputEvents (actions, venue.pimpl->totalFramesRendered + frameOffset);
            frameOffset += numFrames;
        }

    private:
        std::unique_ptr<Session> session;
        AudioPlayerVenue& venue;

        MIDIInputList midiInputList;
        AudioInputList audioInputList;
        AudioOutputList audioOutputList;
        MultiEndpointFIFO inputFIFO;
        EventOutputList eventOutputList;

        BeginNextBlockFn beginNextBlockCallback;
        GetNextNumFramesFn getBlockSizeCallback;
        PrepareInputsFn preRenderCallback;
        ReadOutputsFn postRenderCallback;
        uint32_t frameOffset = 0;

        struct ExternalConnection
        {
            EndpointID sessionEndpoint, externalEndpoint;
        };

        std::vector<ExternalConnection> externalInputConnections, externalOutputConnections;
    };

    //==============================================================================
    struct ExternalIOEndpoint
    {
        std::string endpointID, name;
        bool isInput, isMIDI;
        choc::buffer::ChannelRange channels;

        EndpointDetails getDetails() const
        {
            EndpointDetails details;
            details.endpointID = EndpointID::create (endpointID);
            details.name = name;

            if (isMIDI)
            {
                SOUL_ASSERT (isInput);
                details.endpointType = EndpointType::event;

                auto midiType = choc::value::createObject ("Message",
                                                           "midiBytes", (int32_t) 0).getType();
                details.dataTypes.push_back (midiType);
            }
            else
            {
                details.endpointType = EndpointType::stream;
                details.dataTypes.push_back (choc::value::Type::createVectorFloat32 (channels.size()));
            }

            return details;
        }

        bool operator== (const ExternalIOEndpoint& other) const
        {
            return endpointID == other.endpointID
                    && name == other.name
                    && isInput == other.isInput
                    && isMIDI == other.isMIDI
                    && channels == other.channels;
        }

        bool operator!= (const ExternalIOEndpoint& other) const
        {
            return ! operator== (other);
        }
    };

    //==============================================================================
    ArrayView<const EndpointDetails> getExternalInputEndpoints()  { return externalInputDetails; }
    ArrayView<const EndpointDetails> getExternalOutputEndpoints() { return externalOutputDetails; }

    //==============================================================================
    void renderStarting (double, uint32_t) override
    {
        refreshExternalEndpoints();
    }

    void renderStopped() override {}

    void render (choc::buffer::ChannelArrayView<const float> input,
                 choc::buffer::ChannelArrayView<float> output,
                 MIDIEventInputList midiIn) override
    {
        auto numFrames = output.getNumFrames();
        SOUL_ASSERT (numFrames == input.getNumFrames());
        output.clear();
        uint32_t offset = 0;

        while (numFrames != 0)
        {
            auto numToDo = std::min (numFrames, maxBlockSize);

            currentInputBuffer = input.getFrameRange ({ offset, offset + numToDo });
            currentOutputBuffer = output.getFrameRange ({ offset, offset + numToDo });
            currentMIDIBuffer = midiIn.removeEventsBefore (numToDo);

            auto result = renderingVenue.render (numToDo);
            SOUL_ASSERT (result == nullptr);

            totalFramesRendered += numToDo;
            offset += numToDo;
            numFrames -= numToDo;
        }
    }

    //==============================================================================
    void refreshExternalEndpoints()
    {
        externalEndpoints = getExternalEndpointList();

        externalInputDetails.clear();
        externalOutputDetails.clear();

        for (auto& e : externalEndpoints)
        {
            if (e.isInput)
                externalInputDetails.push_back (e.getDetails());
            else
                externalOutputDetails.push_back (e.getDetails());
        }
    }

    //==============================================================================
    static constexpr uint32_t maxBlockSize = 1024;

    AudioPlayerVenue& venue;
    AudioMIDISystem audioSystem;
    RenderingVenue renderingVenue;

    std::mutex audioCallbackLock;

    std::vector<ExternalIOEndpoint> externalEndpoints;
    std::vector<EndpointDetails> externalInputDetails, externalOutputDetails;

    uint64_t totalFramesRendered = 0;

    choc::buffer::ChannelArrayView<const float> currentInputBuffer;
    choc::buffer::ChannelArrayView<float> currentOutputBuffer;
    MIDIEventInputList currentMIDIBuffer;

    decltype (externalEndpoints) getExternalEndpointList() const
    {
        decltype (externalEndpoints) list;

        if (auto numInputChannels = static_cast<uint32_t> (audioSystem.getNumInputChannels()))
            list.push_back ({ "audio_in", "default audio input", true, false, { 0, numInputChannels } });

        if (auto numOutputChannels = static_cast<uint32_t> (audioSystem.getNumOutputChannels()))
            list.push_back ({ "audio_out", "default audio output", false, false, { 0, numOutputChannels } });

        list.push_back ({ "midi_in", "MIDI in", true, true, {} });
        return list;
    }

    ExternalIOEndpoint* findExternalEndpoint (const EndpointID& endpointID)
    {
        for (auto& e : externalEndpoints)
            if (e.endpointID == endpointID.toString())
                return std::addressof (e);

        return {};
    }

    static const EndpointDetails* findInternalEndpoint (ArrayView<const EndpointDetails> list, const EndpointID& endpointID)
    {
        for (auto& e : list)
            if (e.endpointID == endpointID)
                return std::addressof (e);

        return nullptr;
    }
};

//==============================================================================
AudioPlayerVenue::AudioPlayerVenue (const Requirements& r, std::unique_ptr<PerformerFactory> f)
   : pimpl (std::make_unique<Pimpl> (*this, r, std::move (f)))
{
}

AudioPlayerVenue::~AudioPlayerVenue() = default;

bool AudioPlayerVenue::createSession (SessionReadyCallback callback)
{
    return pimpl->renderingVenue.createSession ([this, cb = std::move (callback)] (std::unique_ptr<Session> newSession)
                                                {
                                                    SOUL_ASSERT (newSession != nullptr);
                                                    cb (std::make_unique<Pimpl::AudioVenueSession> (std::move (newSession), *this));
                                                });
}

ArrayView<const EndpointDetails> AudioPlayerVenue::getExternalInputEndpoints()  { return pimpl->getExternalInputEndpoints(); }
ArrayView<const EndpointDetails> AudioPlayerVenue::getExternalOutputEndpoints() { return pimpl->getExternalOutputEndpoints(); }

}
