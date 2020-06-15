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

  dependencies:     soul_core soul_utilities juce_audio_devices

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#pragma once
#define SOUL_VENUE_AUDIOPLAYER_H_INCLUDED 1

#include <soul_core/soul_core.h>
#include <soul_utilities/soul_utilities.h>

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

        /** The caller can provide a lambda here to handle log messages about audio
            and MIDI devices being opened and closed.
        */
        std::function<void(std::string_view)> printLogMessage;
    };

    /** Returns an implementation of a soul::Venue which uses the standard JUCE
        audio and MIDI devices to play the output of a performer.

        The Requirements object is used to provide hints about preferred sample
        rates, etc. to use when opening an audio device
    */
    std::unique_ptr<soul::Venue> createAudioPlayerVenue (const Requirements&,
                                                         std::unique_ptr<PerformerFactory>);

}
