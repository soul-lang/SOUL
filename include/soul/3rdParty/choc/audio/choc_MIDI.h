//
//    ██████ ██   ██  ██████   ██████
//   ██      ██   ██ ██    ██ ██            ** Clean Header-Only Classes **
//   ██      ███████ ██    ██ ██
//   ██      ██   ██ ██    ██ ██           https://github.com/Tracktion/choc
//    ██████ ██   ██  ██████   ██████
//
//   CHOC is (C)2021 Tracktion Corporation, and is offered under the terms of the ISC license:
//
//   Permission to use, copy, modify, and/or distribute this software for any purpose with or
//   without fee is hereby granted, provided that the above copyright notice and this permission
//   notice appear in all copies. THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
//   WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
//   AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
//   CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
//   WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
//   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#ifndef CHOC_MIDI_HEADER_INCLUDED
#define CHOC_MIDI_HEADER_INCLUDED

#include <string>
#include <cmath>
#include "../platform/choc_Assert.h"

namespace choc::midi
{

static constexpr float  A440_frequency   = 440.0f;
static constexpr int    A440_noteNumber  = 69;

/// Converts a MIDI note (usually in the range 0-127) to a frequency in Hz.
inline float noteNumberToFrequency (int note)          { return A440_frequency * std::pow (2.0f, (static_cast<float> (note) - A440_noteNumber) * (1.0f / 12.0f)); }
/// Converts a MIDI note (usually in the range 0-127) to a frequency in Hz.
inline float noteNumberToFrequency (float note)        { return A440_frequency * std::pow (2.0f, (note - static_cast<float> (A440_noteNumber)) * (1.0f / 12.0f)); }
/// Converts a frequency in Hz to an equivalent MIDI note number.
inline float frequencyToNoteNumber (float frequency)   { return static_cast<float> (A440_noteNumber) + (12.0f / std::log (2.0f)) * std::log (frequency * (1.0f / A440_frequency)); }

/// Returns the name for a MIDI controller number.
inline std::string getControllerName (uint8_t controllerNumber);

/// Returns a space-separated string of hex digits in a fomrat that's appropriate for a MIDI data dump.
inline std::string printHexMIDIData (const uint8_t* data, size_t numBytes);

//==============================================================================
/**
    A class to hold a 0-127 MIDI note number, which provides some helpful methods.
*/
struct NoteNumber
{
    /// The MIDI note number, which must be in the range 0-127.
    uint8_t note;

    /// A NoteNumber can be cast to an integer to get the raw MIDI note number.
    operator uint8_t() const                                    { return note; }

    /// Returns this note's position within an octave, 0-11, where C is 0.
    uint8_t getChromaticScaleIndex() const                      { return note % 12; }

    /// Returns the note's octave number.
    int getOctaveNumber (int octaveForMiddleC = 3) const        { return note / 12 + (octaveForMiddleC - 5); }

    /// Returns the note as a frequency in Hertz.
    float getFrequency() const                                  { return noteNumberToFrequency (static_cast<int> (note)); }

    /// Returns the note name, adding sharps and flats where necessary.
    std::string_view getName() const                            { return std::addressof ("C\0\0C#\0D\0\0Eb\0E\0\0F\0\0F#\0G\0\0G#\0A\0\0Bb\0B"[3 * getChromaticScaleIndex()]); }
    /// Returns the note name, adding sharps where necessary.
    std::string_view getNameWithSharps() const                  { return std::addressof ("C\0\0C#\0D\0\0D#\0E\0\0F\0\0F#\0G\0\0G#\0A\0\0A#\0B"[3 * getChromaticScaleIndex()]); }
    /// Returns the note name, adding flats where necessary.
    std::string_view getNameWithFlats() const                   { return std::addressof ("C\0\0Db\0D\0\0Eb\0E\0\0F\0\0Gb\0G\0\0Ab\0A\0\0Bb\0B"[3 * getChromaticScaleIndex()]); }
    /// Returns the note name and octave number (using default choices for things like sharp/flat/octave number).
    std::string getNameWithOctaveNumber() const                 { return std::string (getName()) + std::to_string (getOctaveNumber()); }

    /// Returns true if this is a natural note in the C major scale.
    bool isNatural() const                                      { return (0b101010110101 & (1 << getChromaticScaleIndex())) != 0; }
    /// Returns true if this is an accidental note, i.e. a sharp or flat.
    bool isAccidental() const                                   { return ! isNatural(); }
};

//==============================================================================
/**
    A short (3-byte) MIDI message.

    For a data type that can also hold long messages, use choc::midi::Message.
*/
struct ShortMessage
{
    ShortMessage() = default;
    ShortMessage (uint8_t byte0, uint8_t byte1, uint8_t byte2)  : data { byte0, byte1, byte2 } {}

    /// The raw data. (Actual message length is determined by interpreting the content)
    uint8_t data[3] = {};

    /// Returns true if this is an empty, uninitialised message.
    bool isNull() const                                 { return data[0] == 0; }

    /// Returns the size of the message in bytes.
    uint8_t length() const;
    /// Returns the size of the message in bytes.
    uint8_t size() const;

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
    bool isMetaEvent() const                            { return data[0] == 0xff; }

    bool isSongPositionPointer() const                  { return data[0] == 0xf2; }
    uint32_t getSongPositionPointerValue() const        { return get14BitValue(); }

    /// Returns a human-readable description of the message.
    std::string getDescription() const;

    /// Returns a hex string dump of the message.
    std::string toHexString() const                     { return printHexMIDIData (data, length()); }

    bool operator== (const ShortMessage&) const;
    bool operator!= (const ShortMessage&) const;

private:
    bool isVoiceMessage (uint8_t type) const            { return (data[0] & 0xf0) == type; }
    uint32_t get14BitValue() const                      { return data[1] | (static_cast<uint32_t> (data[2]) << 7); }
};

//==============================================================================
/**
    Holds any kind of MIDI message.

    If all you need are short (3-byte) messages, then you'd be better off using
    choc::midi::ShortMessage, which doesn't need to do any heap allocation.
*/
struct Message
{
    Message() = default;
    ~Message() = default;

    Message (const void* data, size_t size);
    Message (ShortMessage);

    Message (Message&&) = default;
    Message (const Message&) = default;
    Message& operator= (Message&&) = default;
    Message& operator= (const Message&) = default;
    Message& operator= (ShortMessage);

    /// Returns true if the message is uninitialised.
    bool empty() const;
    /// Returns the size of the message in bytes.
    size_t length() const;
    /// Returns the size of the message in bytes.
    uint8_t size() const;
    /// Returns a byte from the message
    uint8_t operator[] (size_t index) const;
    /// Returns a pointer to the raw message data.
    const uint8_t* data() const;

    /// Returns true if this is a short message (up to 3 bytes).
    bool isShortMessage() const;
    /// Returns true if this is a sysex.
    bool isSysex() const;
    /// Returns true if this is a meta-event.
    bool isMetaEvent() const;
    /// Returns true if this is a meta-event with the given type.
    bool isMetaEventOfType (uint8_t type) const;

    /// If this is a short message, this method will return it. If not, it
    /// will trigger an assertion, so be sure to check isShortMessage() first.
    ShortMessage getShortMessage() const;

    /// If this is a short message, then this cast will succeed. If not, it
    /// will trigger an assertion, so be sure to check isShortMessage() first.
    operator ShortMessage() const;

    /// If this is a meta-event, this will return its meta-type byte. If it isn't
    /// a meta-event, this will trigger an assertion, so you should check beforehand.
    uint8_t getMetaEventType() const;

    /// If this is a meta-event, this will return a description of its type. If it isn't
    /// a meta-event, this will trigger an assertion, so you should check beforehand.
    std::string getMetaEventTypeName() const;

    /// If this is a meta-event, this will return the payload data (i.e. the chunk of
    /// variable-length data after the type and length fields). If it isn't a meta-event,
    /// this will trigger an assertion, so you should check beforehand. If the message
    /// data is malformed, this may return an empty value.
    std::string_view getMetaEventData() const;

    /// Returns a human-readable description of the message.
    std::string getDescription() const;

    /// Returns a hex string dump of the message.
    std::string toHexString() const;

    bool operator== (const Message&) const;
    bool operator!= (const Message&) const;

    /// For some use-cases, this is handy for building a message by concatenating chunks
    void appendData (const void* data, size_t size);

private:
    std::string content; // std::string makes a good container here, as most
                         // implementations will use a short-string optimisation
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
    auto group = (firstByte >> 4) & 7;

    if (group < 7)    return groupLengths[group];
    if (group == 7)   return lastGroupLengths[firstByte & 0xf];

    return 0;
}

inline uint8_t ShortMessage::size() const   { return length(); }

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
    if (isMetaEvent())             return "Meta-event: type " + std::to_string (data[1]);
    if (isSongPositionPointer())   return "Song Position: " + std::to_string (getSongPositionPointerValue());

    return toHexString();
}

inline std::string printHexMIDIData (const uint8_t* data, size_t numBytes)
{
    if (numBytes == 0)
        return "[empty]";

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

//==============================================================================
inline Message::Message (const void* data, size_t size)  : content (static_cast<const char*> (data), size) {}
inline Message::Message (ShortMessage m)  : Message (m.data, m.length()) {}
inline Message& Message::operator= (ShortMessage m) { return operator= (Message (m)); }
inline void Message::appendData (const void* data, size_t size)    { content.append (static_cast<const char*> (data), size); }

inline bool Message::empty() const           { return content.empty(); }
inline size_t Message::length() const        { return content.length(); }
inline const uint8_t* Message::data() const  { return reinterpret_cast<const uint8_t*> (content.data()); }

inline uint8_t Message::operator[] (size_t i) const { CHOC_ASSERT (i < content.length()); return static_cast<uint8_t> (content[i]); }

static constexpr char sysexStartByte      = -16; // 0xf0
static constexpr char metaEventStartByte  = -1;  // 0xff

inline bool Message::isShortMessage() const
{
    auto len = content.length();

    if (len == 0 || len > 3)
        return false;

    auto firstByte = content[0];
    return firstByte != sysexStartByte && firstByte != metaEventStartByte;
}

inline bool Message::isSysex() const                         { return content.length() > 1 && content[0] == sysexStartByte; }
inline bool Message::isMetaEvent() const                     { return content.length() > 2 && content[0] == metaEventStartByte; }
inline bool Message::isMetaEventOfType (uint8_t type) const  { return content.length() > 2 && content[1] == (char) type && content[0] == metaEventStartByte; }

inline ShortMessage Message::getShortMessage() const
{
    auto size = content.length();
    auto d = data();

    if (size == 3)  return { d[0], d[1], d[2] };
    if (size == 2)  return { d[0], d[1], 0 };
    if (size == 1)  return { d[0], 0, 0 };

    CHOC_ASSERT (false); // You must check that this is actually a short message before calling this method
    return {};
}

inline Message::operator ShortMessage() const       { return getShortMessage(); }

inline uint8_t Message::getMetaEventType() const
{
    CHOC_ASSERT (isMetaEvent()); // You must check that this is a meta-event before calling this method
    return data()[1];
}

inline std::string Message::getMetaEventTypeName() const
{
    auto type = getMetaEventType();
    const char* result = nullptr;

    switch (type)
    {
        case 0x00:  result = "Sequence number";     break;
        case 0x01:  result = "Text";                break;
        case 0x02:  result = "Copyright notice";    break;
        case 0x03:  result = "Track name";          break;
        case 0x04:  result = "Instrument name";     break;
        case 0x05:  result = "Lyrics";              break;
        case 0x06:  result = "Marker";              break;
        case 0x07:  result = "Cue point";           break;
        case 0x20:  result = "Channel prefix";      break;
        case 0x2F:  result = "End of track";        break;
        case 0x51:  result = "Set tempo";           break;
        case 0x54:  result = "SMPTE offset";        break;
        case 0x58:  result = "Time signature";      break;
        case 0x59:  result = "Key signature";       break;
        case 0x7F:  result = "Sequencer specific";  break;
        default:    return std::to_string (type);
    }

    return result;
}

inline std::string_view Message::getMetaEventData() const
{
    CHOC_ASSERT (isMetaEvent()); // You must check that this is a meta-event before calling this method

    auto totalLength = content.length();

    if (totalLength < 4)
        return {}; // malformed data

    uint32_t contentLength = 0, lengthBytes = 0;
    auto d = data() + 2; // skip to the length field

    for (;;)
    {
        auto byte = *d++;
        ++lengthBytes;
        contentLength = (contentLength << 7) | (byte & 0x7fu);

        if (byte < 0x80)
            break;

        if (lengthBytes == 4 || lengthBytes + 2 == totalLength)
            return {}; // malformed data
    }

    auto contentStart = lengthBytes + 2;

    if (contentStart + contentLength > totalLength)
        return {}; // malformed data

    return std::string_view (content.data() + contentStart, contentLength);
}

inline std::string Message::getDescription() const
{
    if (isShortMessage())  return getShortMessage().getDescription();
    if (isSysex())         return "Sysex: " + toHexString();

    if (isMetaEvent())
    {
        auto metadataContent = getMetaEventData();
        return "Meta-event: " + getMetaEventTypeName()
                 + ", length: " + std::to_string (metadataContent.length())
                 + ", data: " + printHexMIDIData (reinterpret_cast<const uint8_t*> (metadataContent.data()),
                                                  metadataContent.length());
    }

    return toHexString();
}

inline std::string Message::toHexString() const     { return printHexMIDIData (data(), length()); }

inline bool Message::operator== (const Message& other) const   { return content == other.content; }
inline bool Message::operator!= (const Message& other) const   { return content != other.content; }


} // namespace choc::midi

#endif
