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

choc::value::ValueView getChannelSetAsArrayView (choc::buffer::InterleavedView<const float>);
choc::value::ValueView getChannelSetAsArrayView (choc::buffer::InterleavedView<float>);

choc::buffer::InterleavedView<float> getChannelSetFromArray (const choc::value::ValueView& sourceArray);

choc::value::Value convertAudioDataToObject (choc::buffer::ChannelArrayView<const float> data, double sampleRate);
choc::value::Value convertAudioDataToObject (choc::buffer::InterleavedView<const float> data, double sampleRate);

/** Builds a suitable type of value from a generated waveform, where the
    generator function takes a phase value 0->1 and returns the amplitude -1 to 1.
*/
choc::value::Value generateWaveform (double frequency, double sampleRate, int64_t numFrames,
                                     const std::function<double(double phase)>& waveGenerator);

/** Looks at a set of annotations and tries to create the type of built-in wave
    that the user was asking for. If the annotation can't be interpreted, this
    will just return a void Value.
*/
choc::value::Value generateWaveform (const Annotation&);

choc::value::Value coerceAudioFileObjectToTargetType (const Type& targetType, const choc::value::ValueView& sourceValue);

}
