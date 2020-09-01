/*
    ██████ ██   ██  ██████   ██████
   ██      ██   ██ ██    ██ ██         Clean Header-Only Classes
   ██      ███████ ██    ██ ██         Copyright (C)2020 Julian Storer
   ██      ██   ██ ██    ██ ██
    ██████ ██   ██  ██████   ██████

   The code in this file is provided under the terms of the ISC license:

   Permission to use, copy, modify, and/or distribute this software for any purpose with
   or without fee is hereby granted, provided that the above copyright notice and this
   permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO
   THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT
   SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR
   ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
   CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
   OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef CHOC_MIDI_HEADER_INCLUDED
#define CHOC_MIDI_HEADER_INCLUDED

#include <string>
#include <cmath>

namespace choc::midi
{

static constexpr float  A440_frequency   = 440.0f;
static constexpr int    A440_noteNumber  = 69;

/** Converts a MIDI note (usually in the range 0-127) to a frequency in Hz. */
inline float noteNumberToFrequency (int note)          { return A440_frequency * std::pow (2.0f, (static_cast<float> (note) - A440_noteNumber) * (1.0f / 12.0f)); }
/** Converts a MIDI note (usually in the range 0-127) to a frequency in Hz. */
inline float noteNumberToFrequency (float note)        { return A440_frequency * std::pow (2.0f, (note - static_cast<float> (A440_noteNumber)) * (1.0f / 12.0f)); }
/** Converts a frequency in Hz to an equivalent MIDI note number. */
inline float frequencyToNoteNumber (float frequency)   { return static_cast<float> (A440_noteNumber) + (12.0f / std::log (2.0f)) * std::log (frequency * (1.0f / A440_frequency)); }

/** Returns the name for a MIDI controller number. */
inline std::string getControllerName (uint8_t controllerNumber);

/** Returns a space-separated string of hex digits in a fomrat that's appropriate for a MIDI data dump. */
inline std::string printHexMIDIData (const uint8_t* data, size_t numBytes);

//==============================================================================
/**
*/
struct NoteNumber
{
    /** The MIDI note number, which must be in the range 0-127. */
    uint8_t note;

    operator uint8_t() const                                    { return note; }

    /** Returns this note's position within an octave, 0-11, where C is 0. */
    uint8_t getChromaticScaleIndex() const                      { return note % 12; }

    /** Returns the note's octave number. */
    int getOctaveNumber (int octaveForMiddleC = 3) const        { return note / 12 + (octaveForMiddleC - 5); }

    /** Returns the note as a frequency in Hz. */
    float getFrequency() const                                  { return noteNumberToFrequency (static_cast<int> (note)); }

    /** Returns the note name, adding sharps and flats where necessary */
    const char* getName() const                                 { return std::addressof ("C\0\0C#\0D\0\0Eb\0E\0\0F\0\0F#\0G\0\0G#\0A\0\0Bb\0B"[3 * getChromaticScaleIndex()]); }
    /** Returns the note name, adding sharps where necessary */
    const char* getNameWithSharps() const                       { return std::addressof ("C\0\0C#\0D\0\0D#\0E\0\0F\0\0F#\0G\0\0G#\0A\0\0A#\0B"[3 * getChromaticScaleIndex()]); }
    /** Returns the note name, adding flats where necessary */
    const char* getNameWithFlats() const                        { return std::addressof ("C\0\0Db\0D\0\0Eb\0E\0\0F\0\0Gb\0G\0\0Ab\0A\0\0Bb\0B"[3 * getChromaticScaleIndex()]); }
    /** Returns true if this is a "white" major scale note. */
    bool isWhiteNote() const                                    { return (0b101010110101 & (1 << getChromaticScaleIndex())) != 0; }
    /** Returns the note name and octave number (using default choices for things like sharp/flat/octave number). */
    std::string getNameWithOctaveNumber() const                 { return getName() + std::to_string (getOctaveNumber()); }
};

//==============================================================================
/**
*/
struct ShortMessage
{
    ShortMessage() = default;
    ShortMessage (uint8_t byte0, uint8_t byte1, uint8_t byte2)  : data { byte0, byte1, byte2 } {}

    uint8_t data[3] = {};

    bool isNull() const                                 { return data[0] == 0; }

    uint8_t length() const;

    uint8_t getChannel0to15() const                     { return data[0] & 0x0f; }
    uint8_t getChannel1to16() const                     { return static_cast<uint8_t> (getChannel0to15() + 1u); }

    bool isNoteOn() const                               { return isVoiceMessage (0x90) && getVelocity() != 0; }
    bool isNoteOff() const                              { return isVoiceMessage (0x80) || (getVelocity() == 0 && isVoiceMessage (0x90)); }
    NoteNumber getNoteNumber() const                    { return { data[1] }; }
    uint8_t getVelocity() const                         { return data[2]; }

    bool isProgramChange() const                        { return isVoiceMessage (0xc0); }
    uint8_t getProgramChangeNumber() const              { return data[1]; }

    bool isPitchWheel() const                           { return isVoiceMessage (0xe0); }
    uint32_t getPitchWheelValue() const                 { return get14BitValue(); }
    bool isAftertouch() const                           { return isVoiceMessage (0xa0); }
    uint8_t getAfterTouchValue() const                  { return data[2]; }

    bool isChannelPressure() const                      { return isVoiceMessage (0xd0); }
    uint8_t getChannelPressureValue() const             { return data[1]; }

    bool isController() const                           { return isVoiceMessage (0xb0); }
    uint8_t getControllerNumber() const                 { return data[1]; }
    uint8_t getControllerValue() const                  { return data[2]; }
    bool isControllerNumber (uint8_t number) const      { return data[1] == number && isController(); }
    bool isAllNotesOff() const                          { return isControllerNumber (123); }
    bool isAllSoundOff() const                          { return isControllerNumber (120); }

    bool isQuarterFrame() const                         { return data[0] == 0xf1; }
    bool isClock() const                                { return data[0] == 0xf8; }
    bool isStart() const                                { return data[0] == 0xfa; }
    bool isContinue() const                             { return data[0] == 0xfb; }
    bool isStop() const                                 { return data[0] == 0xfc; }
    bool isActiveSense() const                          { return data[0] == 0xfe; }
    bool isResetOrMeta() const                          { return data[0] == 0xff; }

    bool isSongPositionPointer() const                  { return data[0] == 0xf2; }
    uint32_t getSongPositionPointerValue() const        { return get14BitValue(); }

    /** Returns a human-readable description of the message. */
    std::string getDescription() const;

    /** Returns a hex string dump of the message. */
    std::string toHexString() const                     { return printHexMIDIData (data, length()); }

    bool operator== (const ShortMessage&) const;
    bool operator!= (const ShortMessage&) const;

private:
    bool isVoiceMessage (uint8_t type) const            { return (data[0] & 0xf0) == type; }
    uint32_t get14BitValue() const                      { return data[1] | (static_cast<uint32_t> (data[2]) << 7); }
};


//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================

inline uint8_t ShortMessage::length() const
{
    constexpr uint8_t groupLengths[] = { 3, 3, 3, 3, 2, 2, 3 };
    constexpr uint8_t lastGroupLengths[] = { 1, 2, 3, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };

    auto firstByte = data[0];
    auto group = (firstByte >> 4);

    if (group < 7)    return groupLengths[group];
    if (group == 7)   return lastGroupLengths[firstByte & 0xf];

    return 0;
}

inline bool ShortMessage::operator== (const ShortMessage& other) const
{
    return data[0] == other.data[0]
        && data[1] == other.data[1]
        && data[2] == other.data[2];
}

inline bool ShortMessage::operator!= (const ShortMessage& other) const
{
    return ! operator== (other);
}

inline std::string ShortMessage::getDescription() const
{
    auto channelText = " Channel " + std::to_string (getChannel1to16());
    auto getNote = [] (ShortMessage m)   { auto s = m.getNoteNumber().getNameWithOctaveNumber(); return s.length() < 4 ? s + " " : s; };

    if (isNoteOn())                return "Note-On:  "   + getNote (*this) + channelText + "  Velocity " + std::to_string (getVelocity());
    if (isNoteOff())               return "Note-Off: "   + getNote (*this) + channelText + "  Velocity " + std::to_string (getVelocity());
    if (isAftertouch())            return "Aftertouch: " + getNote (*this) + channelText +  ": " + std::to_string (getAfterTouchValue());
    if (isPitchWheel())            return "Pitch wheel: " + std::to_string (getPitchWheelValue()) + ' ' + channelText;
    if (isChannelPressure())       return "Channel pressure: " + std::to_string (getChannelPressureValue()) + ' ' + channelText;
    if (isController())            return "Controller:" + channelText + ": " + getControllerName (getControllerNumber()) + " = " + std::to_string (getControllerValue());
    if (isProgramChange())         return "Program change: " + std::to_string (getProgramChangeNumber()) + ' ' + channelText;
    if (isAllNotesOff())           return "All notes off:" + channelText;
    if (isAllSoundOff())           return "All sound off:" + channelText;
    if (isQuarterFrame())          return "Quarter-frame";
    if (isClock())                 return "Clock";
    if (isStart())                 return "Start";
    if (isContinue())              return "Continue";
    if (isStop())                  return "Stop";
    if (isResetOrMeta())           return "Reset/Meta-event";
    if (isSongPositionPointer())   return "Song Position: " + std::to_string (getSongPositionPointerValue());

    return toHexString();
}

inline std::string printHexMIDIData (const uint8_t* data, size_t numBytes)
{
    std::string s;
    s.reserve (3 * numBytes);

    for (size_t i = 0; i < numBytes; ++i)
    {
        if (i != 0)  s += ' ';

        s += "0123456789abcdef"[data[i] >> 4];
        s += "0123456789abcdef"[data[i] & 15];
    }

    return s;
}

inline std::string getControllerName (uint8_t controllerNumber)
{
    if (controllerNumber < 128)
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

        if (auto name = controllerNames[controllerNumber])
            return name;
    }

    return std::to_string (controllerNumber);
}

} // namespace choc::midi

#endif
