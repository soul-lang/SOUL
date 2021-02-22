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
    static inline constexpr auto manifestTemplate = R"manifest(
{
    "MANIFEST_NAME":
    {
        "ID":               "com.yourcompany.PATCH_NAME",
        "version":          "0.1",
        "name":             "PATCH_NAME",
        "description":      "This is a SOUL Patch called PATCH_NAME!",
        "category":         "CATEGORY",
        "manufacturer":     "Your Company Name",
        "isInstrument":     IS_INSTRUMENT,

        "source":           [ "PATCH_NAME.soul" ]
    }
}
)manifest";

    static inline constexpr auto synthCodeTemplate = R"proc(
/**
    This is an auto-generated SOUL patch template.
    This example code simply plays a trivial sinewave mono-synth -
    it's up to you to build upon this and create a real synthesiser!
*/
graph PATCH_NAME  [[main]]
{
    input event soul::midi::Message midiIn;
    output stream float audioOut;

    let
    {
        midiParser = soul::midi::MPEParser;
        voice = SineOsc;
    }

    connection
    {
        midiIn -> midiParser -> voice -> audioOut;
    }
}

//==============================================================================
processor SineOsc
{
    input event (soul::note_events::NoteOn,
                 soul::note_events::NoteOff) eventIn;

    output stream float audioOut;

    event eventIn (soul::note_events::NoteOn e)
    {
        currentNote = e.note;
        phaseIncrement = float (twoPi * processor.period * soul::noteNumberToFrequency (e.note));
    }

    event eventIn (soul::note_events::NoteOff e)
    {
        if (e.note == currentNote)
            currentNote = 0;
    }

    float currentNote, phaseIncrement, amplitude;

    void run()
    {
        float phase;

        loop
        {
            // A very simple amplitude envelope - linear attack, exponential decay
            if (currentNote == 0)
                amplitude *= 0.999f;
            else
                amplitude = min (amplitude + 0.001f, 1.0f);

            phase = addModulo2Pi (phase, phaseIncrement);

            audioOut << amplitude * sin (phase);
            advance();
        }
    }
}
)proc";

    static inline constexpr auto effectCodeTemplate = R"proc(
/**
    This is an auto-generated SOUL patch template.
    This example code simply performs a simple gain between its input
    and output. Now it's your turn to build this up into a real effect!
*/
processor PATCH_NAME  [[main]]
{
    input stream float audioIn;
    output stream float audioOut;

    input stream float gainDb  [[ name: "Gain", min: -60.0, max: 10.0, init: 0, step: 0.1, slewRate: 200.0 ]];

    void run()
    {
        loop
        {
            let gain = soul::dBtoGain (gainDb);

            audioOut << audioIn * gain;
            advance();
        }
    }
}
)proc";

    std::vector<SourceFile> createExamplePatchFiles (PatchGeneratorOptions options)
    {
        std::vector<SourceFile> files;

        if (! (options.isSynth || options.isEffect))
            options.isSynth = true;

        options.name = makeSafeIdentifierName (choc::text::trim (options.name));

        auto manifest = choc::text::replace (manifestTemplate,
                                             "MANIFEST_NAME",   soul::patch::getManifestTopLevelPropertyName(),
                                             "PATCH_NAME",      options.name,
                                             "CATEGORY",        options.isSynth ? "synth" : "effect",
                                             "IS_INSTRUMENT",   options.isSynth ? "true" : "false");

        auto processorCode = choc::text::replace (options.isSynth ? synthCodeTemplate
                                                                  : effectCodeTemplate,
                                                  "PATCH_NAME", options.name);

        files.push_back ({ options.name + soul::patch::getManifestSuffix(), manifest });
        files.push_back ({ options.name + ".soul", processorCode });

        return files;
    }
}
