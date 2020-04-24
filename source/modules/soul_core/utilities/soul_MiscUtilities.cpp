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

bool inExceptionHandler()
{
   #if defined(__APPLE__) && MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_12
    return std::uncaught_exception();
   #else
    return std::uncaught_exceptions() != 0;
   #endif
}

ScopedDisableDenormals::ScopedDisableDenormals() noexcept  : oldFlags (getFPMode())
{
   #if SOUL_ARM64 || SOUL_ARM32
    constexpr intptr_t mask = 1 << 24;
   #else
    constexpr intptr_t mask = 0x8040;
   #endif

    setFPMode (oldFlags | mask);
}

ScopedDisableDenormals::~ScopedDisableDenormals() noexcept
{
    setFPMode (oldFlags);
}

intptr_t ScopedDisableDenormals::getFPMode() noexcept
{
    intptr_t flags = 0;

   #if SOUL_ARM64
    asm volatile("mrs %0, fpcr" : "=r" (flags));
   #elif SOUL_ARM32
    asm volatile("vmrs %0, fpscr" : "=r" (flags));
   #elif SOUL_INTEL
    flags = static_cast<intptr_t> (_mm_getcsr());
   #endif

    return flags;
}

void ScopedDisableDenormals::setFPMode (intptr_t newValue) noexcept
{
   #if SOUL_ARM64
    asm volatile("msr fpcr, %0" : : "ri" (newValue));
   #elif SOUL_ARM32
    asm volatile("vmsr fpscr, %0" : : "ri" (newValue));
   #elif SOUL_INTEL
    auto v = static_cast<uint32_t> (newValue);
    _mm_setcsr (v);
   #endif
}

//==============================================================================
const char* getMIDINoteName (uint8_t midiNote, bool useSharps)
{
    auto names = useSharps ? "C\0 C#\0D\0 D#\0E\0 F\0 F#\0G\0 G#\0A\0 A#\0B"
                           : "C\0 Db\0D\0 Eb\0E\0 F\0 Gb\0G\0 Ab\0A\0 Bb\0B";

    return names + (midiNote % 12) * 3;
}

int getMIDIOctaveNumber (uint8_t midiNote, int octaveForMiddleC)
{
    return midiNote / 12 + (octaveForMiddleC - 5);
}

static std::string getMIDIControllerName (uint8_t controllerNumber)
{
    static constexpr const char* controllerNames[128] =
    {
        "Bank Select",                  "Modulation Wheel (coarse)",      "Breath controller (coarse)",       nullptr,
        "Foot Pedal (coarse)",          "Portamento Time (coarse)",       "Data Entry (coarse)",              "Volume (coarse)",
        "Balance (coarse)",             nullptr,                          "Pan position (coarse)",            "Expression (coarse)",
        "Effect Control 1 (coarse)",    "Effect Control 2 (coarse)",      nullptr,                            nullptr,
        "General Purpose Slider 1",     "General Purpose Slider 2",       "General Purpose Slider 3",         "General Purpose Slider 4",
        nullptr,                        nullptr,                          nullptr,                            nullptr,
        nullptr,                        nullptr,                          nullptr,                            nullptr,
        nullptr,                        nullptr,                          nullptr,                            nullptr,
        "Bank Select (fine)",           "Modulation Wheel (fine)",        "Breath controller (fine)",         nullptr,
        "Foot Pedal (fine)",            "Portamento Time (fine)",         "Data Entry (fine)",                "Volume (fine)",
        "Balance (fine)",               nullptr,                          "Pan position (fine)",              "Expression (fine)",
        "Effect Control 1 (fine)",      "Effect Control 2 (fine)",        nullptr,                            nullptr,
        nullptr,                        nullptr,                          nullptr,                            nullptr,
        nullptr,                        nullptr,                          nullptr,                            nullptr,
        nullptr,                        nullptr,                          nullptr,                            nullptr,
        nullptr,                        nullptr,                          nullptr,                            nullptr,
        "Hold Pedal",                   "Portamento",                     "Sustenuto Pedal",                  "Soft Pedal",
        "Legato Pedal",                 "Hold 2 Pedal",                   "Sound Variation",                  "Sound Timbre",
        "Sound Release Time",           "Sound Attack Time",              "Sound Brightness",                 "Sound Control 6",
        "Sound Control 7",              "Sound Control 8",                "Sound Control 9",                  "Sound Control 10",
        "General Purpose Button 1",     "General Purpose Button 2",       "General Purpose Button 3",         "General Purpose Button 4",
        nullptr,                        nullptr,                          nullptr,                            nullptr,
        nullptr,                        nullptr,                          nullptr,                            "Reverb Level",
        "Tremolo Level",                "Chorus Level",                   "Celeste Level",                    "Phaser Level",
        "Data Button increment",        "Data Button decrement",          "Non-registered Parameter (fine)",  "Non-registered Parameter (coarse)",
        "Registered Parameter (fine)",  "Registered Parameter (coarse)",  nullptr,                            nullptr,
        nullptr,                        nullptr,                          nullptr,                            nullptr,
        nullptr,                        nullptr,                          nullptr,                            nullptr,
        nullptr,                        nullptr,                          nullptr,                            nullptr,
        nullptr,                        nullptr,                          nullptr,                            nullptr,
        "All Sound Off",                "All Controllers Off",            "Local Keyboard",                   "All Notes Off",
        "Omni Mode Off",                "Omni Mode On",                   "Mono Operation",                   "Poly Operation"
    };

    if (controllerNumber < 128)
        if (auto name = controllerNames[controllerNumber])
            return name;

    return std::to_string (controllerNumber);
}

std::string getMIDIMessageDescription (const uint8_t* data, size_t numBytes)
{
    if (numBytes == 0)
        return "?";

    if (numBytes < 4)
    {
        auto getNoteDesc = [] (ShortMIDIMessage m)
        {
            auto note = m.getNoteNumber();
            return padded (getMIDINoteName (note, true) + std::to_string (getMIDIOctaveNumber (note)), 4);
        };

        auto getChannelDesc = [] (ShortMIDIMessage m)      { return " Channel " + std::to_string (m.getChannel1to16()); };

        ShortMIDIMessage m;
        memcpy (m.bytes, data, numBytes);

        if (m.isNoteOn())           return "Note-On:  " + getNoteDesc (m) + getChannelDesc (m) + "  Velocity " + std::to_string (m.getVelocity());
        if (m.isNoteOff())          return "Note-Off: " + getNoteDesc (m) + getChannelDesc (m) + "  Velocity " + std::to_string (m.getVelocity());
        if (m.isPitchWheel())       return "Pitch wheel: " + std::to_string (m.getPitchWheelValue()) + " " + getChannelDesc (m);
        if (m.isAftertouch())       return "Aftertouch: " + getNoteDesc (m) + getChannelDesc (m) +  ": " + std::to_string (m.getAfterTouchValue());
        if (m.isChannelPressure())  return "Channel pressure: " + std::to_string (m.getChannelPressureValue()) + " " + getChannelDesc (m);
        if (m.isProgramChange())    return "Program change: " + std::to_string (m.getProgramChangeNumber()) + " " + getChannelDesc (m);
        if (m.isAllSoundOff())      return "All sound off:" + getChannelDesc (m);
        if (m.isAllNotesOff())      return "All notes off:" + getChannelDesc (m);
        if (m.isController())       return "Controller:" + getChannelDesc (m) + ": " + getMIDIControllerName (m.getControllerNumber()) + " = " + std::to_string (m.getControllerValue());
    }

    auto getMessageType = [] (uint8_t firstByte) -> const char*
    {
        switch (firstByte)
        {
            case 0xf0:  return "Sysyex";
            case 0xf1:  return "Quarter frame";
            case 0xf2:  return "Song position pointer";
            case 0xf8:  return "MIDI clock";
            case 0xfa:  return "MIDI start";
            case 0xfb:  return "MIDI continue";
            case 0xfc:  return "MIDI stop";
            case 0xfe:  return "Active Sense";
            case 0xff:  return "Meta-event";
            default:    return "MIDI data";
        }
    };

    auto desc = std::string (getMessageType (data[0])) + ":";

    for (size_t i = 0; i < numBytes; ++i)
        desc += " " + soul::toHexString (data[i], 2);

    return desc;
}

}
