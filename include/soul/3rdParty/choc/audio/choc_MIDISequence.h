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

#ifndef CHOC_MIDISEQUENCE_HEADER_INCLUDED
#define CHOC_MIDISEQUENCE_HEADER_INCLUDED

#include "choc_MIDI.h"
#include "../containers/choc_Span.h"

namespace choc::midi
{

/// Contains a sequence of timed MIDI events, and provides iterators for them.
struct Sequence
{
    /// A time-stamped MIDI event
    struct Event
    {
        double timeInSeconds;
        Message message;

        bool operator< (const Event& other) const    { return timeInSeconds < other.timeInSeconds; }
    };

    /// The raw events in the sequence. Although this vector is public to allow access,
    /// the class expects the list to always remain sorted by time, and the timestamps
    /// must not be negative.
    std::vector<Event> events;

    /// If you've added events to the list, you can use this method to sort it by time.
    void sortEvents();

    auto begin() const  { return events.cbegin(); }
    auto end() const    { return events.cend(); }

    auto begin()        { return events.begin(); }
    auto end()          { return events.end(); }

    /// An iterator for a choc::midi::Sequence.
    /// Note that if the sequence is modified while any iterators are active,
    /// their subsequent behaviour is undefined.
    struct Iterator
    {
        /// Creates an iterator positioned at the start of the sequence.
        Iterator (const Sequence&);
        Iterator (const Iterator&) = default;
        Iterator (Iterator&&) = default;

        /// Seeks the iterator to the given time
        void setTime (double newTime);

        /// Returns the current iterator time
        double getTime() const noexcept             { return currentTime; }

        /// Returns a set of events which lie between the current time, up to (but not
        /// including) the given duration. This function then increments the iterator to
        /// set its current time to the end of this block.
        choc::span<const Event> readNextEvents (double blockDuration);

    private:
        const Sequence& owner;
        double currentTime = 0;
        size_t nextIndex = 0;
    };

    /// Returns an iterator for this sequence
    Iterator getIterator() const        { return Iterator (*this); }
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

inline void Sequence::sortEvents()  { std::stable_sort (events.begin(), events.end()); }

inline Sequence::Iterator::Iterator (const Sequence& s) : owner (s) {}

inline void Sequence::Iterator::setTime (double newTime)
{
    auto eventData = owner.events.data();

    while (nextIndex != 0 && eventData[nextIndex - 1].timeInSeconds >= newTime)
        --nextIndex;

    while (nextIndex < owner.events.size() && eventData[nextIndex].timeInSeconds < newTime)
        ++nextIndex;

    currentTime = newTime;
}

inline choc::span<const Sequence::Event> Sequence::Iterator::readNextEvents (double duration)
{
    auto start = nextIndex;
    auto eventData = owner.events.data();
    auto end = start;
    auto total = owner.events.size();
    auto endTime = currentTime + duration;
    currentTime = endTime;

    while (end < total && eventData[end].timeInSeconds < endTime)
        ++end;

    nextIndex = end;

    return { eventData + start, eventData + end };
}

} // namespace choc::midi

#endif // CHOC_MIDISEQUENCE_HEADER_INCLUDED
