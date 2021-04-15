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

#ifndef CHOC_MIDIFILE_HEADER_INCLUDED
#define CHOC_MIDIFILE_HEADER_INCLUDED

#include <functional>
#include "choc_MIDISequence.h"

namespace choc::midi
{

//==============================================================================
/**
    A reader for MIDI (.mid) file data.
*/
struct File
{
    File() = default;
    ~File() = default;

    void clear();

    /// Attempts to load the given data as a MIDI file. If errors occur,
    /// a ReadError exception will be thrown
    void load (const void* midiFileData, size_t dataSize);

    /// Exception which is thrown by load() if errors occur
    struct ReadError {};

    struct Event
    {
        Message message;
        uint32_t tickPosition = 0;
    };

    struct Track
    {
        std::vector<Event> events;
    };

    /// Iterates all the events on all tracks, returning each one with its playback time in seconds.
    void iterateEvents (const std::function<void(const Message&, double timeInSeconds)>&) const;

    /// Merges all the events from this file into a single MIDI Sequence object.
    choc::midi::Sequence toSequence() const;

    //==============================================================================
    std::vector<Track> tracks;

    /// This is the standard MIDI file time format:
    ///  If positive, this is the number of ticks per quarter-note.
    ///  If negative, this is a SMPTE timecode type
    int16_t timeFormat = 60;
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

namespace
{
    struct Reader
    {
        const uint8_t* data;
        size_t size;

        void expectSize (size_t num)
        {
            if (size < num)
                throw File::ReadError();
        }

        void skip (size_t num)
        {
            data += num;
            size -= num;
        }

        template <typename Type>
        Type read()
        {
            uint32_t n = 0;
            expectSize (sizeof (Type));

            for (size_t i = 0; i < sizeof (Type); ++i)
                n = (n << 8) | data[i];

            skip (sizeof (Type));
            return static_cast<Type> (n);
        }

        std::string_view readString (size_t length)
        {
            expectSize (length);
            std::string_view s (reinterpret_cast<const char*> (data), length);
            skip (length);
            return s;
        }

        uint32_t readVariableLength()
        {
            uint32_t n = 0, numUsed = 0;

            for (;;)
            {
                auto byte = read<uint8_t>();
                n = (n << 7) | (byte & 0x7fu);

                if (byte < 0x80)
                    return n;

                if (++numUsed == 4)
                    throw File::ReadError();
            }
        }
    };

    //==============================================================================
    struct Header
    {
        uint16_t fileType = 0, numTracks = 0, timeFormat = 0;
    };

    inline static Header readHeader (Reader& reader)
    {
        auto chunkName = reader.readString (4);

        if (chunkName == "RIFF")
        {
            for (int i = 8; --i >= 0;)
            {
                chunkName = reader.readString (4);

                if (chunkName == "MThd")
                    break;
            }
        }

        if (chunkName != "MThd")
            throw File::ReadError();

        auto length = reader.read<uint32_t>();
        reader.expectSize (length);

        Header header;
        header.fileType   = reader.read<uint16_t>();
        header.numTracks  = reader.read<uint16_t>();
        header.timeFormat = reader.read<uint16_t>();

        if (header.fileType > 2)
            throw File::ReadError();

        if (header.fileType == 0 && header.numTracks != 1)
            throw File::ReadError();

        return header;
    }

    //==============================================================================
    inline static std::vector<File::Event> readTrack (Reader& reader)
    {
        std::vector<File::Event> result;
        uint32_t tickPosition = 0;
        uint8_t statusByte = 0;

        while (reader.size > 0)
        {
            auto interval = reader.readVariableLength();
            tickPosition += interval;

            if (reader.data[0] >= 0x80)
            {
                statusByte = reader.data[0];
                reader.skip(1);
            }

            if (statusByte < 0x80)
                throw File::ReadError();

            if (statusByte == 0xff) // meta-event
            {
                auto start = reader.data;
                reader.skip (1); // skip the type
                auto length = reader.readVariableLength();
                reader.skip (length);

                Message meta (std::addressof (statusByte), 1);
                meta.appendData (start, static_cast<size_t> (reader.data - start));
                result.push_back ({ std::move (meta), tickPosition });
            }
            else if (statusByte == 0xf0) // sysex
            {
                Message sysex (std::addressof (statusByte), 1);
                auto start = reader.data;

                while (reader.read<uint8_t>() < 0x80)
                {}

                sysex.appendData (start, static_cast<size_t> (reader.data - start));
                result.push_back ({ std::move (sysex), tickPosition });
            }
            else
            {
                ShortMessage m (statusByte, 0, 0);
                auto length = m.length();

                if (length > 1)  m.data[1] = reader.read<uint8_t>();
                if (length > 2)  m.data[2] = reader.read<uint8_t>();

                result.push_back ({ Message (m), tickPosition });
            }
        }

        return result;
    }
}

inline void File::clear()
{
    tracks.clear();
}

inline void File::load (const void* midiFileData, size_t dataSize)
{
    clear();

    if (dataSize == 0)
        return;

    if (midiFileData == nullptr)
        throw ReadError();

    Reader reader { static_cast<const uint8_t*> (midiFileData), dataSize };

    auto header = readHeader (reader);
    timeFormat = static_cast<int16_t> (header.timeFormat);

    for (uint16_t track = 0; track < header.numTracks; ++track)
    {
        auto chunkType = reader.readString (4);
        auto chunkSize = reader.read<uint32_t>();
        reader.expectSize (chunkSize);

        if (chunkType == "MTrk")
        {
            Reader chunkReader { reader.data, chunkSize };
            tracks.push_back ({ readTrack (chunkReader) });
        }

        reader.skip (chunkSize);
    }
}

inline void File::iterateEvents (const std::function<void(const Message&, double timeInSeconds)>& handleEvent) const
{
    std::vector<Event> allEvents;

    for (auto& t : tracks)
        allEvents.insert (allEvents.end(), t.events.begin(), t.events.end());

    std::stable_sort (allEvents.begin(), allEvents.end(),
                      [] (const Event& e1, const Event& e2) { return e1.tickPosition < e2.tickPosition; });

    uint32_t lastTempoChangeTick = 0;
    double lastTempoChangeSeconds = 0, secondsPerTick = 0;

    if (timeFormat < 0)
        secondsPerTick = 1.0 / (-(timeFormat >> 8) * (timeFormat & 0xff));
    else
        secondsPerTick = 0.5 / (timeFormat & 0x7fff);

    for (auto& event : allEvents)
    {
        CHOC_ASSERT (event.tickPosition >= lastTempoChangeTick);
        auto eventTimeSeconds = lastTempoChangeSeconds + secondsPerTick * (event.tickPosition - lastTempoChangeTick);

        if (event.message.isMetaEventOfType (0x51)) // tempo meta-event
        {
            auto content = event.message.getMetaEventData();

            if (content.length() != 3)
                throw File::ReadError();

            uint32_t microsecondsPerQuarterNote = (uint8_t) content[0];
            microsecondsPerQuarterNote = (microsecondsPerQuarterNote << 8) | (uint8_t) content[1];
            microsecondsPerQuarterNote = (microsecondsPerQuarterNote << 8) | (uint8_t) content[2];

            if (timeFormat > 0)
            {
                lastTempoChangeTick = event.tickPosition;
                lastTempoChangeSeconds = eventTimeSeconds;
                auto secondsPerQuarterNote = microsecondsPerQuarterNote / 1000000.0;
                secondsPerTick = secondsPerQuarterNote / (timeFormat & 0x7fff);
            }
        }
        else
        {
            handleEvent (event.message, eventTimeSeconds);
        }
    }
}

inline choc::midi::Sequence File::toSequence() const
{
    choc::midi::Sequence sequence;

    iterateEvents ([&] (const Message& m, double time)
    {
        sequence.events.push_back ({ time, m });
    });

    return sequence;
}



} // namespace choc::midi

#endif // CHOC_MIDIFILE_HEADER_INCLUDED
