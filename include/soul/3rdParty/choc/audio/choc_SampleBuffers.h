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

/** The buffer classes use this type for referring to numbers of samples */
using SampleCount   = uint32_t;
/** The buffer classes use this type for referring to numbers of frames */
using FrameCount    = uint32_t;
/** The buffer classes use this type for referring to numbers of channels */
using ChannelCount  = uint32_t;

template <typename SampleType, template<typename> typename LayoutType> struct AllocatedBuffer;
template <typename SampleType> struct MonoLayout;

//==============================================================================
/** Represents a range of frame numbers. */
struct FrameRange
{
    FrameCount start = 0, end = 0;

    constexpr FrameCount size() const                                 { return end - start; }
    constexpr bool contains (FrameCount index) const                  { return index >= start && index < end; }
    constexpr bool contains (FrameRange range) const                  { return range.start >= start && range.end <= end; }
    constexpr FrameRange getIntersection (FrameRange other) const     { return { start <= other.start ? start : other.start,
                                                                                 end >= other.end ? end : other.end }; }
    constexpr bool operator== (const FrameRange& other) const         { return start == other.start && end == other.end; }
    constexpr bool operator!= (const FrameRange& other) const         { return start != other.start || end != other.end; }
};

//==============================================================================
/** Represents a range of channel numbers. */
struct ChannelRange
{
    ChannelCount start = 0, end = 0;

    constexpr ChannelCount size() const                                 { return end - start; }
    constexpr bool contains (ChannelCount index) const                  { return index >= start && index < end; }
    constexpr bool contains (ChannelRange range) const                  { return range.start >= start && range.end <= end; }
    constexpr FrameRange getIntersection (ChannelRange other) const     { return { start <= other.start ? start : other.start,
                                                                                   end >= other.end ? end : other.end }; }
    constexpr bool operator== (const ChannelRange& other) const         { return start == other.start && end == other.end; }
    constexpr bool operator!= (const ChannelRange& other) const         { return start != other.start || end != other.end; }
};

//==============================================================================
/** Represents the size of a buffer, i.e. the number of channels and frames it contains. */
struct Size
{
    ChannelCount numChannels = 0;
    FrameCount numFrames = 0;

    bool operator== (Size other) const                              { return numChannels == other.numChannels && numFrames == other.numFrames; }
    bool operator!= (Size other) const                              { return numChannels != other.numChannels || numFrames != other.numFrames; }

    ChannelRange getChannelRange() const                            { return { 0, numChannels }; }
    FrameRange getFrameRange() const                                { return { 0, numFrames }; }

    /** Returns true if either the number of channels or frames is zero. */
    bool isEmpty() const                                            { return numChannels == 0 || numFrames == 0; }
    /** Returns true if the given channel number and frame number lie within this size range. */
    bool contains (ChannelCount channel, FrameCount frame) const    { return channel < numChannels && frame < numFrames; }

    /** Returns the overlap section between two sizes. */
    Size getIntersection (Size other) const                         { return { numChannels < other.numChannels ? numChannels : other.numChannels,
                                                                               numFrames < other.numFrames ? numFrames : other.numFrames }; }

    /** Creates a size from a channel and frame count, allowing them to be passed as any kind of
        signed or unsigned integer types.
    */
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

/** This object contains a pointer to a sample, and can also be incremented to move to the next sample. */
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
/**
    Represents a view into a buffer of samples where the data is owned by something else.
    A BufferView never manages the data that it refers to - it simply acts as a lightweight
    pointer into some kind of data layout (as specified by the LayoutType template parameter,
    which could be MonoLayout, InterleavedLayout or SeparateChannelLayout).

    Rather than using BufferView directly, there are typedefs to make it easier, so you'll
    probably want to use InterleavedView, MonoView and ChannelArrayView in your own code.
    There are also various helper functions to create a BufferView, such as createInterleavedView(),
    createMonoView(), createChannelArrayView().

    If you need an object that also allocates and manages the memory needed for the buffer,
    see AllocatedBuffer, InterleavedBuffer, MonoBuffer, SeparateChannelBuffer.
 */
template <typename SampleType, template<typename> typename LayoutType>
struct BufferView
{
    using Sample         = SampleType;
    using Layout         = LayoutType<Sample>;
    using AllocatedType  = AllocatedBuffer<Sample, LayoutType>;

    Layout data;
    Size size;

    /** Returns the size of the view. */
    constexpr Size getSize() const                                              { return size; }
    /** Returns the number of frames in the view. */
    constexpr FrameCount getNumFrames() const                                   { return size.numFrames; }
    /** Returns the number of frames in the view as a range starting from zero . */
    constexpr FrameRange getFrameRange() const                                  { return size.getFrameRange(); }
    /** Returns the number of channels in the view. */
    constexpr ChannelCount getNumChannels() const                               { return size.numChannels; }
    /** Returns the number of channels in the view as a range starting from zero . */
    constexpr ChannelRange getChannelRange() const                              { return size.getChannelRange(); }

    /** Returns a reference to a sample in the view. This will assert if the position is out-of-range. */
    Sample& getSample (ChannelCount channel, FrameCount frame) const            { CHOC_ASSERT (size.contains (channel, frame)); return data.getSample (channel, frame); }
    /** Returns the value of a sample in the view, or zero if the position is out-of-range. */
    Sample getSampleIfInRange (ChannelCount channel, FrameCount frame) const    { return size.contains (channel, frame) ? data.getSample (channel, frame) : Sample(); }

    /** Copies the samples from a frame into a given packed destination array.
        It's up to the caller to make sure the destination has enough space for the number of channels in this view.
        This will assert if the position is out-of-range.
    */
    void getSamplesInFrame (FrameCount frame, Sample* dest) const
    {
        CHOC_ASSERT (frame < size.numFrames);
        return data.getSamplesInFrame (frame, dest, size.numChannels);
    }

    /** Returns an iterator that points to the start of a given channel. */
    SampleIterator<SampleType> getIterator (ChannelCount channel) const         { CHOC_ASSERT (channel < size.numChannels); return data.getIterator (channel); }

    /** Returns a view of a single channel. This will assert if the channel number is out-of-range. */
    BufferView<Sample, MonoLayout> getChannel (ChannelCount channel) const
    {
         CHOC_ASSERT (channel < size.numChannels);
         return { data.getChannelLayout (channel), { 1, size.numFrames } };
    }

    /** Returns a view of a subset of channels. This will assert if the channels requested are out-of-range. */
    BufferView getChannelRange (ChannelRange channels) const
    {
        CHOC_ASSERT (getChannelRange().contains (channels));
        return { data.fromChannel (channels.start), { channels.end - channels.start, size.numFrames } };
    }

    /** Returns a view of the first N channels. This will assert if the channels requested are out-of-range. */
    BufferView getFirstChannels (ChannelCount numChannels) const
    {
        CHOC_ASSERT (numChannels <= size.numChannels);
        return { data, { numChannels, size.numFrames } };
    }

    /** Returns a view of a subset of frames. This will assert if the frame numbers are out-of-range. */
    BufferView getFrameRange (FrameRange range) const
    {
        CHOC_ASSERT (getFrameRange().contains (range));
        return { data.fromFrame (range.start), { size.numChannels, range.end - range.start } };
    }

    /** Returns a view of the start section of this view, up to the given number of frames. This will assert if the frame count is out-of-range. */
    BufferView getStart (FrameCount numberOfFrames) const
    {
        CHOC_ASSERT (numberOfFrames <= size.numFrames);
        return { data, { size.numChannels, numberOfFrames } };
    }

    /** Returns a view of the last N frames in this view. This will assert if the frame count is out-of-range. */
    BufferView getEnd (FrameCount numberOfFrames) const
    {
        CHOC_ASSERT (numberOfFrames <= size.numFrames);
        return { data.fromFrame (size.numFrames - numberOfFrames), { size.numChannels, numberOfFrames } };
    }

    /** Returns a section of this view, from the given frame number to the end. This will assert if the frame count is out-of-range. */
    BufferView fromFrame (FrameCount startFrame) const
    {
        CHOC_ASSERT (startFrame <= size.numFrames);
        return { data.fromFrame (startFrame), { size.numChannels, size.numFrames - startFrame } };
    }

    /** Returns a view of a sub-section of this view. This will assert if the range is out-of-range. */
    BufferView getSection (ChannelRange channels, FrameRange range) const
    {
        CHOC_ASSERT (getFrameRange().contains (range) && getChannelRange().contains (channels));
        return { data.fromFrame (range.start).fromChannel (channels.start), { channels.end - channels.start, range.end - range.start } };
    }

    /** Sets all samples in the view to zero. */
    void clear() const                                          { data.clear (size); }

    /** Allows a view of non-const samples to be cast to one of const samples. */
    operator BufferView<const Sample, LayoutType>() const       { return { static_cast<LayoutType<const Sample>> (data), size }; }
};


//==============================================================================
/**
    Allocates and manages a buffer of samples.

    AllocatedBuffer and BufferView have similar interfaces, but AllocatedBuffer owns
    the sample data that it refers to, so when copied, it takes a full copy of its data
    with it.
    Like for BufferView, the LayoutType template parameter controls the type of data
    layout that should be used: this could be MonoLayout, InterleavedLayout or
    SeparateChannelLayout.

    Rather than using AllocatedBuffer directly, there are typedefs to make it easier, so you'll
    probably want to use InterleavedBuffer, MonoBuffer and ChannelArrayBuffer in your own code.
    There are also various helper functions to create an AllocatedBuffer, such as
    createInterleavedBuffer(), createMonoBuffer(), createChannelArrayBuffer().
 */
template <typename SampleType, template<typename> typename LayoutType>
struct AllocatedBuffer
{
    using Sample         = SampleType;
    using Layout         = LayoutType<Sample>;
    using AllocatedType  = AllocatedBuffer<Sample, LayoutType>;

    /** Creates an empty buffer with a zero size. */
    AllocatedBuffer() = default;
    ~AllocatedBuffer()                  { view.data.freeAllocatedData(); }

    explicit AllocatedBuffer (const AllocatedBuffer& other)  : AllocatedBuffer (other.view) {}
    AllocatedBuffer (AllocatedBuffer&& other)  : view (other.view)    { other.view = {}; }
    AllocatedBuffer& operator= (const AllocatedBuffer&);
    AllocatedBuffer& operator= (AllocatedBuffer&&);

    /** Allocates a buffer of the given size (without clearing its content!)
        For efficiency, this will allocate but not clear the data needed, so be sure to call
        clear() after construction if you need an empty buffer.
     */
    AllocatedBuffer (Size size)  : view { size.isEmpty() ? Layout() : Layout::createAllocated (size), size } {}

    /** Allocates a buffer of the given size (without clearing its content!)
        For efficiency, this will allocate but not clear the data needed, so be sure to call
        clear() after construction if you need an empty buffer.
     */
    AllocatedBuffer (ChannelCount numChannels, FrameCount numFrames)  : AllocatedBuffer (Size { numChannels, numFrames }) {}

    /** Creats a buffer which is a copy of the given view. */
    template <typename SourceView>
    AllocatedBuffer (const SourceView& viewToCopy);

    /** Allows the buffer to be cast to a compatible view. */
    template <typename TargetSampleType>
    operator BufferView<TargetSampleType, LayoutType>() const                   { return static_cast<BufferView<TargetSampleType, LayoutType>> (view); }

    /** Provides a version of this buffer as a view. */
    BufferView<Sample, LayoutType> getView() const                              { return view; }

    //==============================================================================
    /** Returns the size of the buffer. */
    constexpr Size getSize() const                                              { return view.getSize(); }
    /** Returns the number of frames in the buffer. */
    constexpr FrameCount getNumFrames() const                                   { return view.getNumFrames(); }
    /** Returns the number of frames in the buffer as a range starting from zero . */
    constexpr FrameRange getFrameRange() const                                  { return view.getFrameRange(); }
    /** Returns the number of channels in the buffer. */
    constexpr ChannelCount getNumChannels() const                               { return view.getNumChannels(); }
    /** Returns the number of channels in the buffer as a range starting from zero . */
    constexpr ChannelRange getChannelRange() const                              { return view.getChannelRange(); }

    /** Returns a reference to a sample in the buffer. This will assert if the position is out-of-range. */
    Sample& getSample (ChannelCount channel, FrameCount frame) const            { return view.getSample (channel, frame); }
    /** Returns the value of a sample in the buffer, or zero if the position is out-of-range. */
    Sample getSampleIfInRange (ChannelCount channel, FrameCount frame) const    { return view.getSampleIfInRange (channel, frame); }

    /** Copies the samples from a frame into a given packed destination array.
        It's up to the caller to make sure the destination has enough space for the number of channels in this buffer.
        This will assert if the position is out-of-range.
    */
    void getSamplesInFrame (FrameCount frame, Sample* dest) const               { return view.getSamplesInFrame (frame, dest); }

    /** Returns an iterator that points to the start of a given channel. */
    SampleIterator<SampleType> getIterator (ChannelCount channel) const         { return view.getIterator (channel); }

    //==============================================================================
    /** Returns a view of a single channel from the buffer. This will assert if the channel number is out-of-range. */
    BufferView<Sample, MonoLayout> getChannel (ChannelCount channel) const      { return view.getChannel (channel); }
    /** Returns a view of a subset of channels. This will assert if the channels requested are out-of-range. */
    BufferView<Sample, LayoutType> getChannelRange (ChannelRange range) const   { return view.getChannelRange (range); }
    /** Returns a view of the first N channels. This will assert if the channels requested are out-of-range. */
    BufferView<Sample, LayoutType> getFirstChannels (ChannelCount num) const    { return view.getFirstChannels (num); }
    /** Returns a view of a subset of frames. This will assert if the frame numbers are out-of-range. */
    BufferView<Sample, LayoutType> getFrameRange (FrameRange range) const       { return view.getFrameRange (range); }
    /** Returns a view of the start section of this buffer, up to the given number of frames. This will assert if the frame count is out-of-range. */
    BufferView<Sample, LayoutType> getStart (FrameCount numberOfFrames) const   { return view.getStart (numberOfFrames); }
    /** Returns a view of the last N frames in this view. This will assert if the frame count is out-of-range. */
    BufferView<Sample, LayoutType> getEnd (FrameCount numberOfFrames) const     { return view.getEnd (numberOfFrames); }
    /** Returns a section of this view, from the given frame number to the end. This will assert if the frame count is out-of-range. */
    BufferView<Sample, LayoutType> fromFrame (FrameCount startFrame) const      { return view.fromFrame (startFrame); }
    /** Returns a view of a sub-section of this buffer. This will assert if the range is out-of-range. */
    BufferView<Sample, LayoutType> getSection (ChannelRange channels, FrameRange range) const   { return view.getSection (channels, range); }

    /** Sets all samples in the buffer to zero. */
    void clear() const                                                          { view.clear(); }

    /** Resizes the buffer. This will try to preserve as much of the existing content as will fit into
        the new size, and will clear any newly-allocated areas.
    */
    void resize (Size newSize);

private:
    BufferView<Sample, LayoutType> view;
};

//==============================================================================
/** Handles single-channel layouts (with arbitrary stride).
    The layout classes are used in conjunction with the BufferView class.
*/
template <typename SampleType>
struct MonoLayout
{
    SampleType* data = nullptr;
    SampleCount stride = 0;

    SampleType& getSample (ChannelCount, FrameCount frame) const   { return data[stride * frame]; }
    MonoLayout getChannelLayout (ChannelCount) const               { return *this; }
    MonoLayout fromChannel (ChannelCount) const                    { return *this; }
    MonoLayout fromFrame (FrameCount start) const                  { return { data + start * stride, stride }; }
    operator MonoLayout<const SampleType>() const                  { return { data, stride }; }
    SampleIterator<SampleType> getIterator (ChannelCount) const    { return { data, stride }; }
    static constexpr size_t getBytesNeeded (Size size)             { return sizeof (SampleType) * size.numFrames; }
    static MonoLayout createAllocated (Size size)                  { return { new SampleType[size.numFrames], 1u }; }
    void freeAllocatedData()                                       { delete[] data; }

    void clear (Size size) const
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
/** Handles multi-channel layouts where packed frames of samples are laid out
    sequentially in memory.
    The layout classes are used in conjunction with the BufferView class.
*/
template <typename SampleType>
struct InterleavedLayout
{
    SampleType* data = nullptr;
    SampleCount stride = 0;

    SampleType& getSample (ChannelCount channel, FrameCount frame) const  { return data[channel + stride * frame]; }
    MonoLayout<SampleType> getChannelLayout (ChannelCount channel) const  { return { data + channel, stride }; }
    InterleavedLayout fromChannel (ChannelCount start) const              { return { data + start, stride }; }
    InterleavedLayout fromFrame (FrameCount start) const                  { return { data + start * stride, stride }; }
    operator InterleavedLayout<const SampleType>() const                  { return { data, stride }; }
    SampleIterator<SampleType> getIterator (ChannelCount channel) const   { return { data + channel, stride }; }
    static constexpr size_t getBytesNeeded (Size size)                    { return sizeof (SampleType) * size.numFrames * size.numChannels; }
    static InterleavedLayout createAllocated (Size size)                  { return { new SampleType[size.numFrames * size.numChannels], size.numChannels }; }
    void freeAllocatedData()                                              { delete[] data; }

    void clear (Size size) const
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
/** Handles layouts where each channel is packed into a separate block, and there is a master
    array of pointers to these channel blocks.
    The layout classes are used in conjunction with the BufferView class.
*/
template <typename SampleType>
struct SeparateChannelLayout
{
    SampleType* const* channels = nullptr;
    uint32_t offset = 0;

    SampleType& getSample (ChannelCount channel, FrameCount frame) const  { return channels[channel][offset + frame]; }
    MonoLayout<SampleType> getChannelLayout (ChannelCount channel) const  { return { channels[channel] + offset, 1u }; }
    SeparateChannelLayout fromChannel (ChannelCount start) const          { return { channels + start, offset }; }
    SeparateChannelLayout fromFrame (FrameCount start) const              { return { channels, offset + start }; }
    operator SeparateChannelLayout<const SampleType>() const              { return { const_cast<const SampleType* const*> (channels), offset }; }
    SampleIterator<SampleType> getIterator (ChannelCount channel) const   { return { channels[channel] + offset, 1u }; }

    void clear (Size size) const
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
        if (size.numChannels == 0)
        {
            auto allocated = new void*[1];
            *allocated = allocated;
            return { reinterpret_cast<SampleType* const*> (allocated), 0 };
        }

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
/** A handy typedef which is a more readable way to create a interleaved view. */
template <typename SampleType>
using InterleavedView = BufferView<SampleType, InterleavedLayout>;

/** A handy typedef which is a more readable way to create an allocated interleaved buffer. */
template <typename SampleType>
using InterleavedBuffer = AllocatedBuffer<SampleType, InterleavedLayout>;

/** A handy typedef which is a more readable way to create a channel-array view. */
template <typename SampleType>
using ChannelArrayView = BufferView<SampleType, SeparateChannelLayout>;

/** A handy typedef which is a more readable way to create an allocated channel-array buffer. */
template <typename SampleType>
using ChannelArrayBuffer = AllocatedBuffer<SampleType, SeparateChannelLayout>;

/** A handy typedef which is a more readable way to create a mono view. */
template <typename SampleType>
using MonoView = BufferView<SampleType, MonoLayout>;

/** A handy typedef which is a more readable way to create an allocated mono buffer. */
template <typename SampleType>
using MonoBuffer = AllocatedBuffer<SampleType, MonoLayout>;

//==============================================================================
/** Iterates each sample in a view or buffer, applying a user-supplied functor to generate
    or moodify their values.

    The functor must return a value of the appropriate type for the sample, but there are
    several options for the parameters that it can take:
     - Sample() : the functor can have no parameters if none are needed.
     - Sample(Sample) : a functor with 1 parameter will be passed the current value of that
                        sample in the destination, so it can return a modified version.
     - Sample(ChannelCount, FrameCount)  : if there are 2 parameters, these will be the channel
                        and frame number, so that the generator can use these to compute its result
     - Sample(ChannelCount, FrameCount, Sample) : if there are 3 parameters, it will be given the
                        channel, frame, and current values
*/
template <typename BufferType, typename SampleGenerator>
void setAllSamples (BufferType&& buffer, SampleGenerator&& getSampleValue);

/** Iterates each frame in a view or buffer, setting all samples in a frame to a single
    generated value.

    The functor is called once per frame, and the value it returns is used to set all the
    samples within that frame.

    The functor must return a value of the appropriate type for the sample, but can take these
    parameters:
     - Sample()            : the functor can have no parameters if none are needed.
     - Sample(FrameCount)  : if there is 1 parameter, it will be the frame number, so
                             that the generator can use it to compute its result
*/
template <typename BufferType, typename SampleGenerator>
void setAllFrames (BufferType&& buffer, SampleGenerator&& getSampleValue);

/** Copies the contents of one view or buffer to a destination.
    This will assert if the two views do not have exactly the same size.
*/
template <typename DestBuffer, typename SourceBuffer>
static void copy (DestBuffer&& dest, const SourceBuffer& source);

/** Adds the contents of one view or buffer to a destination.
    This will assert if the two views do not have exactly the same size.
*/
template <typename DestBuffer, typename SourceBuffer>
static void add (DestBuffer&& dest, const SourceBuffer& source);

/** Copies from one view or buffer to another with a potentially different number of channels,
    and attempts to do some basic remapping and clearing of channels.
    This expects that both views are the same length. If the destination has more channels,
    then a mono input will be copied to all of them, or a multi-channel input will be copied
    across with any subsequent channels being cleared. If the destination has fewer channels
    then it will copy as many as can fit into the destination.
*/
template <typename DestBuffer, typename SourceBuffer>
static void copyRemappingChannels (DestBuffer&& dest, const SourceBuffer& source);

/** Copies as much of the source as will fit into the destination.
*/
template <typename DestBuffer, typename SourceBuffer>
static void copyIntersection (DestBuffer&& dest, const SourceBuffer& source);

/** Copies as much of the source as will fit into the destination, and clears any
    destination areas outside that area.
*/
template <typename DestBuffer, typename SourceBuffer>
static void copyIntersectionAndClearOutside (DestBuffer&& dest, const SourceBuffer& source);

/** Applies a multiplier to all samples in the given view or buffer. */
template <typename BufferType, typename GainType>
void applyGain (BufferType&& buffer, GainType gainMultiplier);

/** Iterates each frame in a view or buffer, multiplying all samples in a frame by a single
    generated value.

    The functor is called once per frame, and the value it returns is used to set all the
    samples within that frame.

    The functor must return a value of the appropriate type for the sample, but can take these
    parameters:
     - Sample()            : the functor can have no parameters if none are needed.
     - Sample(FrameCount)  : if there is 1 parameter, it will be the frame number, so
                             that the generator can use it to compute its result
*/
template <typename BufferType, typename GetGainFunction>
void applyGainPerFrame (BufferType&& buffer, GetGainFunction&& getGainForFrame);

/** Takes a BufferView or AllocatedBuffer and returns true if all its samples are zero. */
template <typename BufferType>
bool isAllZero (const BufferType& buffer);

/** Takes two views or buffers and returns true if their size and content are identical. */
template <typename Buffer1, typename Buffer2>
static bool contentMatches (const Buffer1& buffer1, const Buffer2& buffer2);

//==============================================================================
/** Returns a view onto a single channel of samples with the given length.
    The data provided is expected to be a valid chunk of samples, and will not be owned
    or copied by the view object, so the caller must manage its lifetime safely.
*/
template <typename SampleType, typename FrameCountType>
MonoView<SampleType> createMonoView (SampleType* packedSamples,
                                     FrameCountType numFrames);

/** Returns an interleaved view with the given size, pointing to the given set of channels.
    The data provided is expected to be a valid chunk of packed, interleaved samples, and will
    not be owned or copied by the view object, so the caller must manage its lifetime safely.
*/
template <typename SampleType, typename ChannelCountType, typename FrameCountType>
InterleavedView<SampleType> createInterleavedView (SampleType* packedSamples,
                                                   ChannelCountType numChannels,
                                                   FrameCountType numFrames);

/** Returns a view into a set of channel pointers with the given size.
    The channel list provided is expected to be valid data, and will not be owned or
    copied by the view object, so the caller must be sure to manage its lifetime safely.
*/
template <typename SampleType>
ChannelArrayView<SampleType> createChannelArrayView (SampleType* const* channelPointers,
                                                     ChannelCount numChannels,
                                                     FrameCount numFrames);

/** Returns an allocated copy of the given view. */
template <typename SourceBufferView>
auto createAllocatedCopy (const SourceBufferView& source);

/** Returns an allocated mono buffer with the given size. Note that the buffer is left
    uninitialised, so call clear() on it if you need a clear one.
*/
template <typename FrameCountType, typename GeneratorFunction>
auto createMonoBuffer (FrameCountType numFrames, GeneratorFunction&& generateSample);

/** Returns an allocated interleaved buffer with the given size. Note that the buffer is left
    uninitialised, so call clear() on it if you need a clear one.
*/
template <typename ChannelCountType, typename FrameCountType, typename GeneratorFunction>
auto createInterleavedBuffer (ChannelCountType numChannels, FrameCountType numFrames, GeneratorFunction&& generateSample);

/** Returns an allocated channel-array buffer with the given size. Note that the buffer is left
    uninitialised, so call clear() on it if you need a clear one.
*/
template <typename ChannelCountType, typename FrameCountType, typename GeneratorFunction>
auto createChannelArrayBuffer (ChannelCountType numChannels, FrameCountType numFrames, GeneratorFunction&& generateSample);



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

template <typename Functor, typename Sample>
static auto invokeGetSample (Functor&& fn, ChannelCount c, FrameCount f, Sample s) -> decltype(fn (f, c, s))  { return fn (c, f, s); }

template <typename Functor, typename Sample>
static auto invokeGetSample (Functor&& fn, ChannelCount c, FrameCount f, Sample)   -> decltype(fn (f, c))  { return fn (c, f); }

template <typename Functor, typename Sample>
static auto invokeGetSample (Functor&& fn, ChannelCount,   FrameCount,   Sample s) -> decltype(fn (s))  { return fn (s); }

template <typename Functor, typename Sample>
static auto invokeGetSample (Functor&& fn, ChannelCount,   FrameCount,   Sample)   -> decltype(fn())  { return fn(); }

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

template <typename Functor>
static auto invokeGetSample (Functor&& fn, FrameCount f) -> decltype(fn (f))  { return fn (f); }

template <typename Functor>
static auto invokeGetSample (Functor&& fn, FrameCount)   -> decltype(fn())  { return fn(); }

template <typename BufferType, typename SampleGenerator>
void setAllFrames (BufferType&& buffer, SampleGenerator&& getSampleValue)
{
    auto size = buffer.getSize();

    for (decltype (size.numFrames) i = 0; i < size.numFrames; ++i)
    {
        auto sample = invokeGetSample (getSampleValue, i);

        for (decltype (size.numChannels) chan = 0; chan < size.numChannels; ++chan)
            buffer.getSample (chan, i) = sample;
    }
}

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

template <typename DestBuffer, typename SourceBuffer>
static void copyRemappingChannels (DestBuffer&& dest, const SourceBuffer& source)
{
    if (auto dstChans = dest.getNumChannels())
    {
        auto srcChans = source.getNumChannels();

        if (dstChans == srcChans)
            return copy (dest, source);

        if (dstChans < srcChans)
            return copy (dest, source.getFirstChannels (dstChans));

        // if asked to map a mono buffer to a bigger one, just copy the same source to all dest channels
        if (srcChans == 1)
        {
            for (decltype (dstChans) chan = 0; chan <  dstChans; ++chan)
                copy (dest.getChannel (chan), source);
        }
        // For anything else, just copy as many channels as will fit, and clear any others
        else
        {
            copy (dest.getFirstChannels (srcChans), source);
            dest.getChannelRange ({ srcChans, dstChans }).clear();
        }
    }
}

template <typename DestBuffer, typename SourceBuffer>
static void copyIntersection (DestBuffer&& dest, const SourceBuffer& source)
{
    auto overlap = dest.getSize().getIntersection (source.getSize());

    if (! overlap.isEmpty())
        copy (dest.getSection (overlap.getChannelRange(), overlap.getFrameRange()),
              source.getSection (overlap.getChannelRange(), overlap.getFrameRange()));
}

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

template <typename DestBuffer, typename SourceBuffer>
static void addIntersection (DestBuffer&& dest, const SourceBuffer& source)
{
    auto overlap = dest.getSize().getIntersection (source.getSize());

    if (! overlap.isEmpty())
        add (dest.getSection (overlap.getChannelRange(), overlap.getFrameRange()),
             source.getSection (overlap.getChannelRange(), overlap.getFrameRange()));
}

template <typename BufferType, typename GainType>
void applyGain (BufferType&& buffer, GainType gainMultiplier)
{
    setAllSamples (buffer, [=] (auto sample) { return static_cast<decltype (sample)> (sample * gainMultiplier); });
}

template <typename BufferType, typename GetGainFunction>
void applyGainPerFrame (BufferType&& buffer, GetGainFunction&& getGainForFrame)
{
    auto size = buffer.getSize();

    for (decltype (size.numFrames) i = 0; i < size.numFrames; ++i)
    {
        auto gain = invokeGetSample (getGainForFrame, i);

        for (decltype (size.numChannels) chan = 0; chan < size.numChannels; ++chan)
            buffer.getSample (chan, i) *= gain;
    }
}

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

template <typename SampleType, typename FrameCountType>
MonoView<SampleType> createMonoView (SampleType* data, FrameCountType numFrames)
{
    return { { data, 1u }, { 1, static_cast<FrameCount> (numFrames) } };
}

template <typename SampleType, typename ChannelCountType, typename FrameCountType>
InterleavedView<SampleType> createInterleavedView (SampleType* data, ChannelCountType numChannels, FrameCountType numFrames)
{
    return { { data, static_cast<uint32_t> (numChannels) }, Size::create (numChannels, numFrames) };
}

template <typename SampleType>
ChannelArrayView<SampleType> createChannelArrayView (SampleType* const* channels, ChannelCount numChannels, FrameCount numFrames)
{
    return { { channels, 0 }, Size::create (numChannels, numFrames) };
}

template <typename SourceBufferView>
auto createAllocatedCopy (const SourceBufferView& source)
{
    return SourceBufferView::AllocatedType (source);
}

template <typename FrameCountType, typename GeneratorFunction>
auto createMonoBuffer (FrameCountType numFrames, GeneratorFunction&& generateSample)
{
    using Sample = decltype (invokeGetSample (generateSample, 0, 0, 0));
    InterleavedBuffer<Sample> result (Size::create (1, numFrames));
    setAllSamples (result, generateSample);
    return result;
}

template <typename ChannelCountType, typename FrameCountType, typename GeneratorFunction>
auto createInterleavedBuffer (ChannelCountType numChannels, FrameCountType numFrames, GeneratorFunction&& generateSample)
{
    using Sample = decltype (invokeGetSample (generateSample, 0, 0, 0));
    InterleavedBuffer<Sample> result (Size::create (numChannels, numFrames));
    setAllSamples (result, generateSample);
    return result;
}

template <typename ChannelCountType, typename FrameCountType, typename GeneratorFunction>
auto createChannelArrayBuffer (ChannelCountType numChannels, FrameCountType numFrames, GeneratorFunction&& generateSample)
{
    using Sample = decltype (invokeGetSample (generateSample, 0, 0, 0));
    ChannelArrayBuffer<Sample> result (Size::create (numChannels, numFrames));
    setAllSamples (result, generateSample);
    return result;
}

template <typename SampleType, template<typename> typename LayoutType>
template <typename SourceView>
AllocatedBuffer<SampleType, LayoutType>::AllocatedBuffer (const SourceView& viewToCopy)  : AllocatedBuffer (viewToCopy.getSize())
{
    copy (view, viewToCopy);
}

template <typename SampleType, template<typename> typename LayoutType>
AllocatedBuffer<SampleType, LayoutType>& AllocatedBuffer<SampleType, LayoutType>::operator= (const AllocatedBuffer& other)
{
    view.data.freeAllocatedData();
    view = { Layout::createAllocated (other.view.size), other.view.size };
    copy (view, other.view);
    return *this;
}

template <typename SampleType, template<typename> typename LayoutType>
AllocatedBuffer<SampleType, LayoutType>& AllocatedBuffer<SampleType, LayoutType>::operator= (AllocatedBuffer&& other)
{
    view.data.freeAllocatedData();
    view = other.view;
    other.view = {};
    return *this;
}

template <typename SampleType, template<typename> typename LayoutType>
void AllocatedBuffer<SampleType, LayoutType>::resize (Size newSize)
{
    if (view.getSize() != newSize)
    {
        auto newView = decltype(view) { Layout::createAllocated (newSize), newSize };
        copyIntersectionAndClearOutside (newView, view);
        view.data.freeAllocatedData();
        view = newView;
    }
}


} // namespace choc::buffer

#endif
