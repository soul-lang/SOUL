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

namespace soul
{
    //==============================================================================
    /*
        Some audio-themed helper functions
    */
    float32 dBtoGain (float32 decibels)   { return decibels > -100.0f ? pow (10.0f, decibels * 0.05f) : 0.0f; }
    float64 dBtoGain (float64 decibels)   { return decibels > -100.0  ? pow (10.0,  decibels * 0.05)  : 0.0; }

    float32 gainTodB (float32 gain)       { return gain > 0 ? log10 (gain) * 20.0f : -100.0f; }
    float64 gainTodB (float64 gain)       { return gain > 0 ? log10 (gain) * 20.0  : -100.0; }

    /** Converts a MIDI note (usually in the range 0-127) to a frequency in Hz using a periodic tuning.
    
        @param note                 a note number
        @param rootNote             the note number at which the scale begins
        @param rootFrequency        the frequency at which the scale begins
        @param scale                the frequency interval of each scale degree (`scale[-1]` is used as the period)
    */
    float32 noteNumberToFrequency (int note, int rootNote, float32 rootFrequency, float32[] scale)
    {
        note -= rootNote + 1;
        return scale.at(note) * rootFrequency * pow(scale[-1], float32((note-wrap(note, scale.size)) / scale.size));
    }
    /** Converts a MIDI note (usually in the range 0-127) to a frequency in Hz using A440 12-TET tuning. */
    float32 noteNumberToFrequency (int note)            { return 440.0f * pow (2.0f, (note - 69) * (1.0f / 12.0f)); }
    /** Converts a MIDI note (usually in the range 0-127) to a frequency in Hz using A440 12-TET tuning. */
    float32 noteNumberToFrequency (float32 note)        { return 440.0f * pow (2.0f, (note - 69.0f) * (1.0f / 12.0f)); }
    /** Converts a frequency in Hz to an equivalent MIDI note number using A440 12-TET tuning. */
    float32 frequencyToNoteNumber (float32 frequency)   { return 69.0f + (12.0f / log (2.0f)) * log (frequency * (1.0f / 440.0f)); }

    /** Returns the ratio by which a sample's playback must be sped-up in order to map
        from it native rate and note to a target rate and note.
    */
    float64 getSpeedRatioForPitchedSample (float64 sourceSampleRate, float32 sourceMIDINote,
                                           float64 targetSampleRate, float32 targetMIDINote)
    {
        return (sourceSampleRate * pow (2.0f, targetMIDINote * (1.0f / 12.0f)))
             / (targetSampleRate * pow (2.0f, sourceMIDINote * (1.0f / 12.0f)));
    }
}

//==============================================================================
/** This namespace contains some handy stuctures to use when declaring external
    variables which are going to be loaded with data from audio files.
*/
namespace soul::audio_samples
{
    /** An external variable declared with the type soul::audio_samples::Mono
        can be loaded with monoised data from an audio file.
    */
    struct Mono
    {
        float[] frames;
        float64 sampleRate;
    }

    /** An external variable declared with the type soul::audio_samples::Stereo
        can be loaded with stereo data from an audio file.
    */
    struct Stereo
    {
        float<2>[] frames;
        float64 sampleRate;
    }
}

/** This namespace contains various pan-related helper functions */
namespace soul::pan_law
{
    /** Applies a simple linear pan law to convert a pan position (-1.0 to 1.0) to
        a (left, right) pair of stereo gains.
    */
    float<2> linear (float pan)
    {
        return (1.0f - pan,
                1.0f + pan);
    }

    /** Applies a 3dB-centre pan law to convert a pan position (-1.0 to 1.0) to
        a (left, right) pair of stereo gains.
    */
    float<2> centre3dB (float pan)
    {
        let quarterPi = float (pi / 4);

        return (sin ((1.0f - pan) * quarterPi),
                sin ((1.0f + pan) * quarterPi));
    }
}

/** This namespace contains delay related helper function */
namespace soul::delay
{
    processor EventDelay (using EventType, int maxEvents, int delay)
    {
        input event EventType eventIn;
        output event EventType eventOut;

        int currentTime;

        struct EventEntry
        {
            int dispatchTime;
            EventType queuedEvent;
        }

        EventEntry[maxEvents] eventEntries;

        wrap<maxEvents> readPos, writePos;
        int queuedEvents;

        event eventIn (EventType e)
        {
            if (queuedEvents < maxEvents)
            {
                eventEntries[writePos].dispatchTime = currentTime + delay;
                eventEntries[writePos].queuedEvent = e;
                writePos++;
                queuedEvents++;
            }
        }

        void run()
        {
            loop
            {
                while (queuedEvents > 0 && eventEntries[readPos].dispatchTime == currentTime)
                {
                    eventOut << eventEntries[readPos].queuedEvent;
                    readPos++;
                    queuedEvents--;
                }

                currentTime++;
                advance();
            }
        }
    }
}

)library"
