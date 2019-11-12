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

/*  The following string literal forms part of a set of SOUL code chunks that form
    the built-in library. (See the soul::getBuiltInLibraryCode() function)
*/
R"library(

/**
    This namespace contains some types which are handy for representing synthesiser
    note events. They do a similar job to MIDI events, but as strongly-typed structs
    instead of a group of bytes. Things like the midi::MPEParser class generate them.
*/
namespace soul::NoteEvents
{
    struct NoteOn
    {
        int channel;
        float note;
        float velocity;
    }

    struct NoteOff
    {
        int channel;
        float note;
        float velocity;
    }

    struct PitchBend
    {
        int channel;
        float bendSemitones;
    }

    struct Pressure
    {
        int channel;
        float pressure;
    }

    struct Slide
    {
        int channel;
        float slide;
    }

    struct Control
    {
        int channel;
        int control;
        float value;
    }
}

//==============================================================================
/**
    Various simple voice allocation processors, which take a single stream of
    input events, and redirect them to an array of target voices.
*/
namespace soul::VoiceAllocators
{
    /** A simple allocator which chooses either an inactive voice, or the
        least-recently used active one if it needs to steal.
    */
    processor Basic (int voiceCount)  [[ main: false ]]
    {
        input event (soul::NoteEvents::NoteOn,
                     soul::NoteEvents::NoteOff,
                     soul::NoteEvents::PitchBend,
                     soul::NoteEvents::Pressure,
                     soul::NoteEvents::Slide) eventIn;

        output event (soul::NoteEvents::NoteOn,
                      soul::NoteEvents::NoteOff,
                      soul::NoteEvents::PitchBend,
                      soul::NoteEvents::Pressure,
                      soul::NoteEvents::Slide) voiceEventOut[voiceCount];

        event eventIn (soul::NoteEvents::NoteOn e)
        {
            wrap<voiceCount> allocatedVoice = 0;
            var allocatedVoiceAge = voiceInfo[allocatedVoice].voiceAge;

            // Find the oldest voice to reuse
            for (int i = 1; i < voiceCount; ++i)
            {
                let age = voiceInfo.at(i).voiceAge;

                if (age < allocatedVoiceAge)
                {
                    allocatedVoiceAge = age;
                    allocatedVoice = wrap<voiceCount>(i);
                }
            }

            // Send the note on to the voice
            voiceEventOut[allocatedVoice] << e;

            // If the voice was previously active, we're stealing it, so send a note off too
            if (voiceInfo[allocatedVoice].active)
            {
                soul::NoteEvents::NoteOff noteOff;

                noteOff.channel = voiceInfo[allocatedVoice].channel;
                noteOff.note    = voiceInfo[allocatedVoice].note;

                voiceEventOut[allocatedVoice] << noteOff;
            }

            // Update the VoiceInfo for our chosen voice
            voiceInfo[allocatedVoice].active   = true;
            voiceInfo[allocatedVoice].channel  = e.channel;
            voiceInfo[allocatedVoice].note     = e.note;
            voiceInfo[allocatedVoice].voiceAge = nextAllocatedVoiceAge++;
        }

        event eventIn (soul::NoteEvents::NoteOff e)
        {
            // Release all voices associated with this note/channel
            wrap<voiceCount> voice = 0;

            loop (voiceCount)
            {
                if (voiceInfo[voice].channel == e.channel
                     && voiceInfo[voice].note == e.note)
                {
                    // Mark the voice as being unused
                    voiceInfo[voice].active   = false;
                    voiceInfo[voice].voiceAge = nextUnallocatedVoiceAge++;

                    voiceEventOut[voice] << e;
                }

                ++voice;
            }
        }

        event eventIn (soul::NoteEvents::PitchBend e)
        {
            // Forward the pitch bend to all notes on this channel
            wrap<voiceCount> voice = 0;

            loop (voiceCount)
            {
                if (voiceInfo[voice].channel == e.channel)
                    voiceEventOut[voice] << e;

                ++voice;
            }
        }

        event eventIn (soul::NoteEvents::Pressure p)
        {
            // Forward the event to all notes on this channel
            wrap<voiceCount> voice = 0;

            loop (voiceCount)
            {
                if (voiceInfo[voice].channel == p.channel)
                    voiceEventOut[voice] << p;

                ++voice;
            }
        }

        event eventIn (soul::NoteEvents::Slide s)
        {
            // Forward the event to all notes on this channel
            wrap<voiceCount> voice = 0;

            loop (voiceCount)
            {
                if (voiceInfo[voice].channel == s.channel)
                    voiceEventOut[voice] << s;

                ++voice;
            }
        }

        struct VoiceInfo
        {
            bool active;
            int channel;
            float note;
            int voiceAge;
        }

        int nextAllocatedVoiceAge   = 1000000000;
        int nextUnallocatedVoiceAge = 1;

        VoiceInfo[voiceCount] voiceInfo;

        void run()
        {
            loop advance();
        }
    }
}

)library"
