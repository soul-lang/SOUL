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

#ifndef CHOC_SAMPLE_BUFFERS_HEADER_INCLUDED
#define CHOC_SAMPLE_BUFFERS_HEADER_INCLUDED

#include <memory>
#include <cassert>

#ifndef CHOC_ASSERT
 #define CHOC_ASSERT(x)  assert(x);
#endif

/**
    A collection of classes for creating views and buffers to operate on multichannel sample data.

    This set of classes can create holders for multichannel data which offer flexibility in terms of:
      - whether or not they own and manage the storage for the data
      - sample type can be float, double, or integers
      - the layout can be interleaved or based on an array of pointers to individual channels
*/
namespace choc::buffer
{

using SampleCount   = uint32_t;
using FrameCount    = uint32_t;
using ChannelCount  = uint32_t;

template <typename SampleType, template<typename> typename LayoutType> struct AllocatedBuffer;

//==============================================================================
/** */
struct FrameRange
{
    FrameCount start = 0, end = 0;

    constexpr bool contains (FrameCount index) const                  { return index >= start && index < end; }
    constexpr bool contains (FrameRange range) const                  { return range.start < end && range.end > start; }
    constexpr FrameRange getIntersection (FrameRange other) const     { return { start <= other.start ? start : other.start,
                                                                                 end >= other.end ? end : other.end }; }
};

//==============================================================================
/** */
struct ChannelRange
{
    ChannelCount start = 0, end = 0;

    constexpr bool contains (ChannelCount index) const                  { return index >= start && index < end; }
    constexpr bool contains (ChannelRange range) const                  { return range.start < end && range.end > start; }
    constexpr FrameRange getIntersection (ChannelRange other) const     { return { start <= other.start ? start : other.start,
                                                                                   end >= other.end ? end : other.end }; }
};

//==============================================================================
/** */
struct Size
{
    ChannelCount numChannels;
    FrameCount numFrames;

    bool operator== (Size other) const                              { return numChannels == other.numChannels && numFrames == other.numFrames; }
    bool operator!= (Size other) const                              { return numChannels != other.numChannels || numFrames != other.numFrames; }

    ChannelRange getChannelRange() const                            { return { 0, numChannels }; }
    FrameRange getFrameRange() const                                { return { 0, numFrames }; }

    bool isEmpty() const                                            { return numChannels == 0 || numFrames == 0; }
    bool contains (ChannelCount channel, FrameCount frame) const    { return channel < numChannels && frame < numFrames; }

    Size getIntersection (Size other) const                         { return { numChannels < other.numChannels ? numChannels : other.numChannels,
                                                                               numFrames < other.numFrames ? numFrames : other.numFrames }; }

    template <typename ChannelCountType, typename FrameCountType>
    static Size create (ChannelCountType numChannels, FrameCountType numFrames)
    {
        static_assert (std::is_integral<ChannelCountType>::value && std::is_integral<FrameCountType>::value, "Need to pass integers into this method");

        if constexpr (std::is_signed<ChannelCountType>::value)  { CHOC_ASSERT (numChannels >= 0); }
        if constexpr (std::is_signed<FrameCountType>::value)    { CHOC_ASSERT (numFrames >= 0); }

        return { static_cast<ChannelCount> (numChannels),
                 static_cast<FrameCount> (numFrames) };
    }
};

template <typename SampleType>
struct SampleIterator
{
    SampleType* sample = nullptr;
    SampleCount stride = 0;

    SampleType get() const              { return *sample; }
    SampleType operator*() const        { return *sample; }
    SampleType& operator*()             { return *sample; }
    SampleIterator& operator++()        { sample += stride; return *this; }
    SampleIterator operator++ (int)     { auto old = sample; sample += stride; return { old, stride }; }
};

//==============================================================================
/** */
template <typename SampleType>
struct MonoLayout
{
    SampleType* data = nullptr;
    SampleCount stride = 0;

    SampleType& getSample (ChannelCount, FrameCount frame)         { return data[stride * frame]; }
    SampleType  getSample (ChannelCount, FrameCount frame) const   { return data[stride * frame]; }
    MonoLayout getChannelLayout (ChannelCount) const               { return *this; }
    MonoLayout getChannelRange (ChannelRange) const                { return *this; }
    MonoLayout getFrameRange (FrameRange range) const              { return { data + range.start, stride }; }
    operator MonoLayout<const SampleType>() const                  { return { data, stride }; }
    SampleIterator<SampleType> getIterator (ChannelCount) const    { return { data, stride }; }
    static constexpr size_t getBytesNeeded (Size size)             { return sizeof (SampleType) * size.numFrames; }
    static MonoLayout createAllocated (Size size)                  { return { new SampleType[size.numFrames], 1u }; }
    void freeAllocatedData()                                       { delete[] data; }

    void clear (Size size)
    {
        if (stride == 1)
            std::fill_n (data, size.numFrames, SampleType());
        else
            for (auto i = data, end = data + stride * size.numFrames; i != end; i += stride)
                i = {};
    }

    void getSamplesInFrame (FrameCount frame, SampleType* dest, ChannelCount) const
    {
        *dest = getSample (frame);
    }
};

//==============================================================================
/** */
template <typename SampleType>
struct InterleavedLayout
{
    SampleType* data = nullptr;
    SampleCount stride = 0;

    SampleType& getSample (ChannelCount channel, FrameCount frame)        { return data[channel + stride * frame]; }
    SampleType  getSample (ChannelCount channel, FrameCount frame) const  { return data[channel + stride * frame]; }
    MonoLayout<SampleType> getChannelLayout (ChannelCount channel) const  { return { data + channel, stride }; }
    InterleavedLayout getChannelRange (ChannelRange channels) const       { return { data + channels.start, stride }; }
    InterleavedLayout getFrameRange (FrameRange range) const              { return { data + range.start * stride, stride }; }
    operator InterleavedLayout<const SampleType>() const                  { return { data, stride }; }
    SampleIterator<SampleType> getIterator (ChannelCount channel) const   { return { data + channel, stride }; }
    static constexpr size_t getBytesNeeded (Size size)                    { return sizeof (SampleType) * size.numFrames * size.numChannels; }
    static InterleavedLayout createAllocated (Size size)                  { return { new SampleType[size.numFrames * size.numChannels], size.numChannels }; }
    void freeAllocatedData()                                              { delete[] data; }

    void clear (Size size)
    {
        if (size.numChannels == stride)
            std::fill_n (data, size.numChannels * size.numFrames, SampleType());
        else
            for (auto i = data, end = data + stride * size.numFrames; i != end; i += stride)
                std::fill_n (i, size.numChannels, SampleType());
    }

    void getSamplesInFrame (FrameCount frame, SampleType* dest, ChannelCount numChans) const
    {
        auto* src = data + frame * stride;

        for (decltype (numChans) i = 0; i < numChans; ++i)
            dest[i] = src[i];
    }
};

//==============================================================================
/** */
template <typename SampleType>
struct SeparateChannelLayout
{
    SampleType* const* channels = nullptr;
    uint32_t offset = 0;

    SampleType& getSample (ChannelCount channel, FrameCount frame)        { return channels[channel][offset + frame]; }
    SampleType  getSample (ChannelCount channel, FrameCount frame) const  { return channels[channel][offset + frame]; }
    MonoLayout<SampleType> getChannelLayout (ChannelCount channel) const  { return { channels[channel] + offset, 1u }; }
    SeparateChannelLayout getChannelRange (ChannelRange range) const      { return { channels + range.start, offset }; }
    SeparateChannelLayout getFrameRange (FrameRange range) const          { return { channels, offset + range.start }; }
    operator SeparateChannelLayout<const SampleType>() const              { return { const_cast<const SampleType* const*> (channels), offset }; }
    SampleIterator<SampleType> getIterator (ChannelCount channel) const   { return { channels[channel] + offset, 1u }; }

    void clear (Size size)
    {
        for (decltype(size.numChannels) i = 0; i < size.numChannels; ++i)
            std::fill_n (channels[i] + offset, size.numFrames, SampleType());
    }

    void getSamplesInFrame (FrameCount frame, SampleType* dest, ChannelCount numChans) const
    {
        for (decltype (numChans) i = 0; i < numChans; ++i)
            dest[i] = channels[i][offset + frame];
    }

    static constexpr size_t getBytesNeeded (Size size)
    {
        auto dataSize = getChannelDataSize (size.numFrames) * size.numChannels;
        auto listSize = sizeof (SampleType*) * size.numChannels;
        return dataSize + listSize;
    }

    static SeparateChannelLayout createAllocated (Size size)
    {
        auto channelDataSize = getChannelDataSize (size.numFrames);
        auto dataSize = channelDataSize * size.numChannels;
        auto listSize = sizeof (SampleType*) * size.numChannels;
        auto allocated = new char[dataSize + listSize];
        auto list = reinterpret_cast<SampleType**> (allocated + dataSize);

        for (decltype (size.numChannels) i = 0; i < size.numChannels; ++i)
            list[i] = reinterpret_cast<SampleType*> (allocated + i * channelDataSize);

        return { list, 0 };
    }

    void freeAllocatedData()
    {
        if (channels != nullptr)
            delete[] channels[0];
    }

    static constexpr size_t getChannelDataSize (FrameCount numFrames)    { return ((sizeof (SampleType) * numFrames) + 15u) & ~15u; }
};

//==============================================================================
/** */
template <typename SampleType, template<typename> typename LayoutType>
struct BufferView
{
    using Sample = SampleType;
    using Layout = LayoutType<Sample>;
    using AllocatedType = AllocatedBuffer<Sample, LayoutType>;

    Layout data;
    Size size;

    /** */
    constexpr Size getSize() const                                              { return size; }
    /** */
    constexpr FrameCount getNumFrames() const                                   { return size.numFrames; }
    /** */
    constexpr FrameRange getFrameRange() const                                  { return size.getFrameRange(); }
    /** */
    constexpr ChannelCount getNumChannels() const                               { return size.numChannels; }
    /** */
    constexpr ChannelRange getChannelRange() const                              { return size.getChannelRange(); }

    /** */
    Sample& getSample (ChannelCount channel, FrameCount frame)                  { CHOC_ASSERT (size.contains (channel, frame)); return data.getSample (channel, frame); }
    /** */
    const Sample getSample (ChannelCount channel, FrameCount frame) const       { CHOC_ASSERT (size.contains (channel, frame)); return data.getSample (channel, frame); }

    /** */
    Sample getSampleIfInRange (ChannelCount channel, FrameCount frame) const    { return size.contains (channel, frame) ? data.getSample (channel, frame) : Sample(); }

    /** */
    void getSamplesInFrame (FrameCount frame, Sample* dest) const
    {
        CHOC_ASSERT (frame < size.numFrames);
        return data.getSamplesInFrame (frame, dest, size.numChannels);
    }

    /** */
    SampleIterator<SampleType> getIterator (ChannelCount channel) const         { CHOC_ASSERT (channel < size.numChannels); return data.getIterator (channel); }

    /** */
    BufferView<Sample, MonoLayout> getChannel (ChannelCount channel) const
    {
         CHOC_ASSERT (channel < size.numChannels);
         return { data.getChannelLayout (channel), { 1, size.numFrames } };
    }

    /** */
    BufferView getChannelRange (ChannelRange channels) const
    {
        CHOC_ASSERT (getChannelRange().contains (channels));
        return { data.getChannelRange (channels), { channels.end - channels.start, size.numFrames } };
    }

    /** */
    BufferView getFrameRange (FrameRange range) const
    {
        CHOC_ASSERT (getFrameRange().contains (range));
        return { data.getFrameRange (range), { size.numChannels, range.end - range.start } };
    }

    /** */
    BufferView getStart (FrameCount numberOfFrames) const
    {
        CHOC_ASSERT (numberOfFrames <= size.numFrames);
        return { data, { size.numChannels, numberOfFrames } };
    }

    /** */
    BufferView getSection (ChannelRange channels, FrameRange range) const
    {
        CHOC_ASSERT (getFrameRange().contains (range) && getChannelRange().contains (channels));
        return { data.getFrameRange (range).getChannelRange (channels), { channels.end - channels.start, range.end - range.start } };
    }

    /** */
    void clear()       { data.clear (size); }

    operator BufferView<const Sample, LayoutType>() const                 { return { static_cast<LayoutType<const Sample>> (data), size }; }
};

//==============================================================================
template <typename Functor, typename Sample>
static auto invokeGetSample (Functor&& fn, ChannelCount c, FrameCount f, Sample s) -> decltype(fn (f, c, s))  { return fn (c, f, s); }

template <typename Functor, typename Sample>
static auto invokeGetSample (Functor&& fn, ChannelCount c, FrameCount f, Sample)   -> decltype(fn (f, c))  { return fn (c, f); }

template <typename Functor, typename Sample>
static auto invokeGetSample (Functor&& fn, ChannelCount,   FrameCount,   Sample s) -> decltype(fn (s))  { return fn (s); }

template <typename Functor, typename Sample>
static auto invokeGetSample (Functor&& fn, ChannelCount,   FrameCount,   Sample)   -> decltype(fn())  { return fn(); }

/** */
template <typename BufferType, typename SampleGenerator>
void setAllSamples (BufferType&& buffer, SampleGenerator&& getSampleValue)
{
    auto size = buffer.getSize();

    for (decltype (size.numChannels) chan = 0; chan < size.numChannels; ++chan)
    {
        auto d = buffer.getIterator (chan);

        for (decltype (size.numFrames) i = 0; i < size.numFrames; ++i)
        {
            *d = invokeGetSample (getSampleValue, chan, i, *d);
            ++d;
        }
    }
}

/** */
template <typename DestBuffer, typename SourceBuffer>
static void copy (DestBuffer&& dest, const SourceBuffer& source)
{
    auto size = source.getSize();
    CHOC_ASSERT (size == dest.getSize());

    for (decltype (size.numChannels) chan = 0; chan < size.numChannels; ++chan)
    {
        auto src = source.getIterator (chan);
        auto dst = dest.getIterator (chan);

        for (decltype (size.numFrames) i = 0; i < size.numFrames; ++i)
        {
            *dst = static_cast<decltype (dst.get())> (src.get());
            ++dst;
            ++src;
        }
    }
}

/** */
template <typename DestBuffer, typename SourceBuffer>
static void add (DestBuffer&& dest, const SourceBuffer& source)
{
    auto size = source.getSize();
    CHOC_ASSERT (size == dest.getSize());

    for (decltype (size.numChannels) chan = 0; chan < size.numChannels; ++chan)
    {
        auto src = source.getIterator (chan);
        auto dst = dest.getIterator (chan);

        for (decltype (size.numFrames) i = 0; i < size.numFrames; ++i)
        {
            *dst += static_cast<decltype (dst.get())> (src.get());
            ++dst;
            ++src;
        }
    }
}

/** */
template <typename DestBuffer, typename SourceBuffer>
static void copyRemappingChannels (DestBuffer&& dest, const SourceBuffer& source)
{
    if (auto dstChans = dest.getNumChannels())
    {
        auto srcChans = source.getNumChannels();

        if (dstChans == srcChans)
            return copy (dest, source);

        if (dstChans < srcChans)
            return copy (dest, source.getChannelRange ({ 0, dstChans }));

        // if asked to map a mono buffer to a bigger one, just copy the same source to all dest channels
        if (srcChans == 1)
        {
            for (decltype (dstChans) chan = 0; chan <  dstChans; ++chan)
                copy (dest.getChannel (chan), source);
        }
        // For anything else, just copy as many channels as will fit, and clear any others
        else
        {
            copy (dest.getChannelRange ({ 0, srcChans }), source);
            dest.getChannelRange ({ srcChans, dstChans }).clear();
        }
    }
}

/** Copies as much of the source as will fit into the destination, and clears any
    destination areas outside that area.
*/
template <typename DestBuffer, typename SourceBuffer>
static void copyIntersectionAndClearOutside (DestBuffer&& dest, const SourceBuffer& source)
{
    auto dstSize = dest.getSize();
    auto srcSize = source.getSize();
    auto overlap = dstSize.getIntersection (srcSize);

    if (overlap.isEmpty())
        return dest.clear();

    copy (dest.getSection (overlap.getChannelRange(), overlap.getFrameRange()),
          source.getSection (overlap.getChannelRange(), overlap.getFrameRange()));

    if (overlap.numFrames < dstSize.numFrames)
        dest.getChannelRange (overlap.getChannelRange()).getFrameRange ({ overlap.numFrames, dstSize.numFrames }).clear();

    if (overlap.numChannels < dstSize.numChannels)
        dest.getChannelRange ({ overlap.numChannels, dstSize.numChannels }).clear();
}

/** */
template <typename BufferType, typename GainType>
void applyGain (BufferType&& buffer, GainType gainMultiplier)
{
    setAllSamples (buffer, [=] (auto sample) { return static_cast<decltype (sample)> (sample * gainMultiplier); });
}

/** */
template <typename BufferType>
bool isAllZero (const BufferType& buffer)
{
    auto size = buffer.getSize();

    for (decltype (size.numChannels) chan = 0; chan < size.numChannels; ++chan)
    {
        auto d = buffer.getIterator (chan);

        for (decltype (size.numFrames) i = 0; i < size.numFrames; ++i)
        {
            if (*d != 0)
                return false;

            ++d;
        }
    }

    return true;
}

/** */
template <typename Buffer1, typename Buffer2>
static bool contentMatches (const Buffer1& buffer1, const Buffer2& buffer2)
{
    auto size = buffer1.getSize();

    if (size != buffer2.getSize())
        return false;

    for (decltype (size.numChannels) chan = 0; chan < size.numChannels; ++chan)
    {
        auto d1 = buffer1.getIterator (chan);
        auto d2 = buffer2.getIterator (chan);

        for (decltype (size.numFrames) i = 0; i < size.numFrames; ++i)
        {
            if (*d1 != *d2)
                return false;

            ++d1;
            ++d2;
        }
    }

    return true;
}


//==============================================================================
/** */
template <typename SampleType, template<typename> typename LayoutType>
struct AllocatedBuffer
{
    using Sample = SampleType;
    using Layout = LayoutType<Sample>;
    using AllocatedType = AllocatedBuffer<Sample, LayoutType>;

    /** */
    AllocatedBuffer() = default;

    /** */
    AllocatedBuffer (Size size)  : view { size.isEmpty() ? Layout() : Layout::createAllocated (size), size } {}

    /** */
    AllocatedBuffer (ChannelCount numChannels, FrameCount numFrames)  : AllocatedBuffer (Size { numChannels, numFrames }) {}

    /** */
    ~AllocatedBuffer()      { view.data.freeAllocatedData(); }

    /** */
    template <typename SourceView>
    AllocatedBuffer (const SourceView& viewToCopy)  : AllocatedBuffer (viewToCopy.getSize())
    {
        copy (view, viewToCopy);
    }

    /** */
    explicit AllocatedBuffer (const AllocatedBuffer& other)  : AllocatedBuffer (other.view) {}

    /** */
    AllocatedBuffer (AllocatedBuffer&& other)  : view (other.view)
    {
        other.view = {};
    }

    /** */
    AllocatedBuffer& operator= (const AllocatedBuffer& other)
    {
        view.data.freeAllocatedData();
        view = { Layout::createAllocated (other.view.size), other.view.size };
        copy (view, other.view);
        return *this;
    }

    /** */
    AllocatedBuffer& operator= (AllocatedBuffer&& other)
    {
        view.data.freeAllocatedData();
        view = other.view;
        other.view = {};
        return *this;
    }

    /** */
    BufferView<Sample, LayoutType> view;

    /** */
    template <typename TargetSampleType>
    operator BufferView<TargetSampleType, LayoutType>() const                   { return static_cast<BufferView<TargetSampleType, LayoutType>> (view); }

    //==============================================================================
    /** */
    constexpr Size getSize() const                                              { return view.getSize(); }
    /** */
    constexpr FrameCount getNumFrames() const                                   { return view.getNumFrames(); }
    /** */
    constexpr FrameRange getFrameRange() const                                  { return view.getFrameRange(); }
    /** */
    constexpr ChannelCount getNumChannels() const                               { return view.getNumChannels(); }
    /** */
    constexpr ChannelRange getChannelRange() const                              { return view.getChannelRange(); }

    /** */
    Sample& getSample (ChannelCount channel, FrameCount frame)                  { return view.getSample (channel, frame); }
    /** */
    const Sample getSample (ChannelCount channel, FrameCount frame) const       { return view.getSample (channel, frame); }

    /** */
    Sample getSampleIfInRange (ChannelCount channel, FrameCount frame) const    { return view.getSampleIfInRange (channel, frame); }

    /** */
    void getSamplesInFrame (FrameCount frame, Sample* dest) const               { return view.getSamplesInFrame (frame, dest); }

    /** */
    SampleIterator<SampleType> getIterator (ChannelCount channel) const         { return view.getIterator (channel); }

    //==============================================================================
    /** */
    BufferView<Sample, MonoLayout> getChannel (ChannelCount channel) const      { return view.getChannel (channel); }
    /** */
    BufferView<Sample, LayoutType> getChannelRange (ChannelRange range) const   { return view.getChannelRange (range); }
    /** */
    BufferView<Sample, LayoutType> getFrameRange (FrameRange range) const       { return view.getFrameRange (range); }
    /** */
    BufferView<Sample, LayoutType> getStart (FrameCount numberOfFrames) const   { return view.getStart (numberOfFrames); }
    /** */
    BufferView<Sample, LayoutType> getSection (ChannelRange channels, FrameRange range) const   { return view.getSection (channels, range); }

    /** */
    void clear()                                                                { view.clear(); }

    /** */
    void resize (Size newSize)
    {
        if (view.getSize() != newSize)
        {
            auto newView = decltype(view) { Layout::createAllocated (newSize), newSize };
            copyIntersectionAndClearOutside (newView, view);
            view.data.freeAllocatedData();
            view = newView;
        }
    }
};

//==============================================================================
template <typename SampleType>
using InterleavedView = BufferView<SampleType, InterleavedLayout>;

template <typename SampleType>
using InterleavedBuffer = AllocatedBuffer<SampleType, InterleavedLayout>;

template <typename SampleType>
using ChannelArrayView = BufferView<SampleType, SeparateChannelLayout>;

template <typename SampleType>
using ChannelArrayBuffer = AllocatedBuffer<SampleType, SeparateChannelLayout>;

template <typename SampleType>
using MonoView = BufferView<SampleType, MonoLayout>;

template <typename SampleType>
using MonoBuffer = AllocatedBuffer<SampleType, MonoLayout>;

//==============================================================================
/** */
template <typename SampleType, typename ChannelCountType, typename FrameCountType>
InterleavedView<SampleType> createInterleavedView (SampleType* data,
                                                   ChannelCountType numChannels,
                                                   FrameCountType numFrames)
{
    return { { data, static_cast<uint32_t> (numChannels) }, Size::create (numChannels, numFrames) };
}

/** */
template <typename SampleType>
ChannelArrayView<SampleType> createChannelArrayView (SampleType* const* channels,
                                                     ChannelCount numChannels,
                                                     FrameCount numFrames)
{
    return { { channels, 0 }, Size::create (numChannels, numFrames) };
}

/** */
template <typename SampleType, typename FrameCountType>
MonoView<SampleType> createMonoView (SampleType* data, FrameCountType numFrames)
{
    return { { data, 1u }, { 1, static_cast<FrameCount> (numFrames) } };
}

/** */
template <typename SourceBufferType>
auto createAllocatedCopy (const SourceBufferType& source)
{
    return SourceBufferType::AllocatedType (source);
}

/** */
template <typename ChannelCountType, typename FrameCountType, typename GeneratorFunction>
auto createInterleavedBuffer (ChannelCountType numChannels, FrameCountType numFrames, GeneratorFunction&& generateSample)
{
    using Sample = decltype (invokeGetSample (generateSample, 0, 0, 0));
    InterleavedBuffer<Sample> result (Size::create (numChannels, numFrames));
    setAllSamples (result, generateSample);
    return result;
}

/** */
template <typename ChannelCountType, typename FrameCountType, typename GeneratorFunction>
auto createChannelArrayBuffer (ChannelCountType numChannels, FrameCountType numFrames, GeneratorFunction&& generateSample)
{
    using Sample = decltype (invokeGetSample (generateSample, 0, 0, 0));
    ChannelArrayBuffer<Sample> result (Size::create (numChannels, numFrames));
    setAllSamples (result, generateSample);
    return result;
}



} // namespace choc::buffer

#endif
