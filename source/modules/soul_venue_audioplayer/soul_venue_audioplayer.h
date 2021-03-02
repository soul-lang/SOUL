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

/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:               soul_venue_audioplayer
  vendor:           SOUL
  version:          0.0.1
  name:             SOUL audio player venue
  description:      A simple JUCE-based SOUL audio player venue implementation
  website:          https://soul.dev/
  license:          ISC

  dependencies:     soul_core juce_audio_devices

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#pragma once
#define SOUL_VENUE_AUDIOPLAYER_H_INCLUDED 1

#include <soul_core/soul_core.h>

//==============================================================================
namespace soul::audioplayer
{
    /** Some info that the venue needs to know when opening audio devices. */
    struct Requirements
    {
        double sampleRate = 0; // 0 means "default"
        int blockSize = 0;
        int numInputChannels = 2;
        int numOutputChannels = 2;

        using PrintLogMessageFn = std::function<void(std::string_view)>;

        /** The caller can provide a lambda here to handle log messages about audio
            and MIDI devices being opened and closed.
        */
        PrintLogMessageFn printLogMessage;
    };

    //==============================================================================
    struct MIDIInputCollector
    {
        MIDIInputCollector (Requirements::PrintLogMessageFn);
        ~MIDIInputCollector();

        void clearFIFO();
        MIDIEventInputList getNextBlock (double sampleRate, uint32_t numFrames);

    private:
        struct Pimpl;
        std::unique_ptr<Pimpl> pimpl;
    };

    //==============================================================================
    struct AudioMIDISystem
    {
        AudioMIDISystem (Requirements);
        ~AudioMIDISystem();

        struct Callback
        {
            virtual ~Callback() = default;

            virtual void render (choc::buffer::ChannelArrayView<const float> input,
                                 choc::buffer::ChannelArrayView<float> output,
                                 MIDIEventInputList) = 0;

            virtual void renderStarting (double sampleRate, uint32_t blockSize) = 0;
            virtual void renderStopped() = 0;
        };

        void setCallback (Callback*);

        double getSampleRate() const;
        uint32_t getMaxBlockSize() const;

        float getCPULoad() const;
        int getXRunCount() const;

        int getNumInputChannels() const;
        int getNumOutputChannels() const;

    private:
        struct Pimpl;
        std::unique_ptr<Pimpl> pimpl;
    };

    //==============================================================================
    /** A venue inplementation that connects to the system audio devices. */
    struct AudioPlayerVenue  : public soul::Venue
    {
        AudioPlayerVenue (Requirements, std::unique_ptr<PerformerFactory>);
        ~AudioPlayerVenue() override;

        //==============================================================================
        bool createSession (SessionReadyCallback) override;

        choc::span<const EndpointDetails> getExternalInputEndpoints() override;
        choc::span<const EndpointDetails> getExternalOutputEndpoints() override;

    private:
        struct Pimpl;
        std::unique_ptr<Pimpl> pimpl;
    };

    //==============================================================================
    // These helper functions auto-connect the standard audio and MIDI external endpoints
    // to any suitable endpoints in the program that the session has loaded.
    void connectDefaultAudioInputEndpoints  (soul::Venue&, soul::Venue::Session&);
    void connectDefaultAudioOutputEndpoints (soul::Venue&, soul::Venue::Session&);
    void connectDefaultMIDIInputEndpoints   (soul::Venue&, soul::Venue::Session&);

} // namespace soul::audioplayer
