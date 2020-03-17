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

//==============================================================================
/** Represents a set of channels which are stored as a single array of frame blocks.
    By default these are non-owning of the data they refer to, but they can be wrapped
    in an AllocatedChannelSet object in order to also make them allocate.
*/
template <typename Sample>
struct InterleavedChannelSet
{
    using SampleType = Sample;
    SampleType* data = nullptr;
    uint32_t numChannels = 0, numFrames = 0, stride = 0;

    SampleType* getChannel (uint32_t channel) const
    {
        SOUL_ASSERT (channel < numChannels);
        return data + channel;
    }

    SampleType getSample (uint32_t channel, uint32_t frame) const
    {
        SOUL_ASSERT (channel < numChannels && frame < numFrames);
        return *(data + channel + frame * stride);
    }

    SampleType& getSample (uint32_t channel, uint32_t frame)
    {
        SOUL_ASSERT (channel < numChannels && frame < numFrames);
        return *(data + channel + frame * stride);
    }

    void getFrame (uint32_t frame, Sample* dest) const
    {
        SOUL_ASSERT (frame < numFrames);
        auto* src = data + frame * stride;

        for (uint32_t i = 0; i < numChannels; ++i)
            dest[i] = src[i];
    }

    InterleavedChannelSet getSlice (uint32_t start, uint32_t length) const
    {
        SOUL_ASSERT (start <= numFrames && start + length <= numFrames);
        return { data + start * stride, numChannels, length, stride };
    }

    InterleavedChannelSet getChannelSet (uint32_t firstChannel, uint32_t numChans) const
    {
        SOUL_ASSERT (firstChannel <= numChannels && firstChannel + numChans <= numChannels);
        return { data + firstChannel, numChans, numFrames, stride };
    }

    ArrayView<Sample> getPackedInterleavedData() const
    {
        SOUL_ASSERT (numChannels == stride);
        return { data, data + numFrames * stride };
    }

    void clear() const
    {
        auto d = data;

        for (uint32_t i = 0; i < numFrames; ++i)
        {
            for (uint32_t j = 0; j < numChannels; ++j)
                d[j] = {};

            d += stride;
        }
    }

    template <typename FunctionToApply>
    void applyToAllSamples (FunctionToApply&& function)
    {
        auto d = data;

        for (uint32_t i = 0; i < numFrames; ++i)
        {
            for (uint32_t chan = 0; chan < numChannels; ++chan)
                function (d[chan]);

            d += stride;
        }
    }

    static InterleavedChannelSet createAllocated (uint32_t channels, uint32_t frames)
    {
        InterleavedChannelSet c { nullptr, channels, frames, channels };
        c.allocateData();
        return c;
    }

    template <typename SourceChannelSetType>
    static InterleavedChannelSet createAllocatedCopy (SourceChannelSetType source)
    {
        auto c = createAllocated (source.numChannels, source.numFrames);
        copyChannelSet (c, source);
        return c;
    }

    void allocateData()
    {
        data = new SampleType[(size_t) (numFrames * stride)];
        std::fill_n (data, numFrames * stride, SampleType());
    }

    void freeData()
    {
        delete[] data;
    }

    void resetWithoutFreeingData()
    {
        data = {}; numChannels = 0; numFrames = 0; stride = 0;
    }
};

//==============================================================================
/** Represents a set of channels where each channel is stored in a separate chunk
    of contiguous data.
    By default these are non-owning of the data they refer to, but they can be wrapped
    in an AllocatedChannelSet object in order to also make them allocate.
*/
template <typename Sample>
struct DiscreteChannelSet
{
    using SampleType = Sample;
    SampleType* const* channels = nullptr;
    uint32_t numChannels = 0, offset = 0, numFrames = 0;
    static constexpr uint32_t stride = 1;

    uint32_t getAvailableSamples (uint32_t start) const
    {
        auto availableSamples = numFrames - offset;
        SOUL_ASSERT (start <= availableSamples);
        return availableSamples - start;
    }

    SampleType* getChannel (uint32_t channel) const
    {
        SOUL_ASSERT (channel < numChannels);
        return channels[channel] + offset;
    }

    SampleType& getSample (uint32_t channel, uint32_t frame)
    {
        SOUL_ASSERT (channel < numChannels && frame < numFrames);
        return channels[channel][offset + frame];
    }

    SampleType getSample (uint32_t channel, uint32_t frame) const
    {
        SOUL_ASSERT (channel < numChannels && frame < numFrames);
        return channels[channel][offset + frame];
    }

    void getFrame (uint32_t frame, Sample* dest) const
    {
        SOUL_ASSERT (frame < numFrames);

        for (uint32_t i = 0; i < numChannels; ++i)
            dest[i] = channels[i][offset + frame];
    }

    DiscreteChannelSet getSlice (uint32_t start, uint32_t length) const
    {
        SOUL_ASSERT (start <= numFrames && start + length <= numFrames);
        return { channels, numChannels, offset + start, length };
    }

    DiscreteChannelSet getChannelSet (uint32_t firstChannel, uint32_t numChans) const
    {
        SOUL_ASSERT (firstChannel <= numChannels && firstChannel + numChans <= numChannels);
        return { channels + firstChannel, numChans, offset, numFrames };
    }

    void clear() const
    {
        for (uint32_t i = 0; i < numChannels; ++i)
            memset (getChannel(i), 0, sizeof(SampleType) * numFrames);
    }

    template <typename FunctionToApply>
    void applyToAllSamples (FunctionToApply&& function)
    {
        for (uint32_t chan = 0; chan < numChannels; ++chan)
        {
            auto data = getChannel (chan);

            for (uint32_t i = 0; i < numFrames; ++i)
                function (data[i]);
        }
    }

    static DiscreteChannelSet createAllocated (uint32_t channels, uint32_t frames)
    {
        DiscreteChannelSet c { nullptr, channels, 0, frames };
        c.allocateData();
        return c;
    }

    template <typename SourceChannelSetType>
    static DiscreteChannelSet createAllocatedCopy (SourceChannelSetType source)
    {
        auto c = createAllocated (source.numChannels, source.numFrames);
        copyChannelSet (c, source);
        return c;
    }

    void allocateData()
    {
        auto channelList = new SampleType*[numChannels + 1];

        channels = channelList;
        channelList[numChannels] = nullptr;

        if (numChannels > 0)
        {
            auto channelStride = getAlignedSize<4> (numFrames);
            auto channelData = new SampleType[channelStride * numChannels];
            std::fill_n (channelData, channelStride * numChannels, SampleType());

            for (uint32_t i = 0; i < numChannels; ++i)
                channelList[i] = channelData + i * channelStride;
        }
    }

    void freeData()
    {
        if (channels != nullptr)
        {
            delete[] channels[0];
            delete[] channels;
        }
    }

    void resetWithoutFreeingData()
    {
        channels = {}; numChannels = 0; offset = 0; numFrames = 0;
    }
};

//==============================================================================
template <typename DestSampleType, typename SourceSampleType>
DestSampleType castSampleType (SourceSampleType sourceSample)
{
    return static_cast<DestSampleType> (sourceSample);
}

template<>
inline float castSampleType<float, const int> (const int sourceSample)
{
    return (static_cast<float> (sourceSample)) / 32767.0f;
}

template<>
inline int castSampleType<int, const float> (const float sourceSample)
{
    return static_cast<int> (sourceSample * 32767.0f);
}

template <typename Type1, typename Type2>
bool channelSetsAreSameSize (Type1 set1, Type2 set2)
{
    return set1.numChannels == set2.numChannels
            && set1.numFrames == set2.numFrames;
}

/** Copies the contents of one channel set to another, which must have exactly
    the same number of channels and samples.
*/
template <typename DestType, typename SourceType>
void copyChannelSet (DestType dest, SourceType src)
{
    SOUL_ASSERT (channelSetsAreSameSize (src, dest));

    for (uint32_t chan = 0; chan < src.numChannels; ++chan)
    {
        auto srcChan = src.getChannel (chan);
        auto dstChan = dest.getChannel (chan);

        for (uint32_t i = 0; i < src.numFrames; ++i)
        {
            *dstChan = castSampleType<typename DestType::SampleType, const typename SourceType::SampleType> (*srcChan);
            dstChan += dest.stride;
            srcChan += src.stride;
        }
    }
}

/** Copies a channel set to another with a different number of channels, using
    some simple default rules for mono->stereo conversion
*/
template <typename DestType, typename SourceType>
void copyChannelSetToFit (DestType dest, SourceType src)
{
    if (dest.numChannels == src.numChannels)
    {
        copyChannelSet (dest, src);
    }
    else if (dest.numChannels < src.numChannels)
    {
        copyChannelSet (dest, src.getChannelSet (0, dest.numChannels));
    }
    else if (src.numChannels == 1)
    {
        for (uint32_t i = 0; i < dest.numChannels; ++i)
            copyChannelSet (dest.getChannelSet (i, 1), src);
    }
    else
    {
        copyChannelSet (dest.getChannelSet (0, src.numChannels), src);
        dest.getChannelSet (src.numChannels, dest.numChannels - src.numChannels).clear();
    }
}

template <typename ChannelSetType>
static void widenOrNarrowChannelSet (ChannelSetType& data, uint32_t requiredNumChannels)
{
    if (data.numChannels != requiredNumChannels)
    {
        auto newCopy = ChannelSetType::createAllocated (requiredNumChannels, data.numFrames);
        copyChannelSetToFit (newCopy, data);
        data.freeData();
        data = newCopy;
    }
}

template <typename ChannelSetType>
bool isChannelSetAllZero (ChannelSetType channelSet)
{
    for (uint32_t chan = 0; chan < channelSet.numChannels; ++chan)
    {
        auto data = channelSet.getChannel (chan);

        for (uint32_t i = 0; i < channelSet.numFrames; ++i)
        {
            if (data[i] != 0)
                return false;

            data += channelSet.stride;
        }
    }

    return true;
}

template <typename Type1, typename Type2>
bool channelSetContentIsIdentical (Type1 set1, Type2 set2)
{
    if (! channelSetsAreSameSize (set1, set2))
        return false;

    for (uint32_t chan = 0; chan < set1.numChannels; ++chan)
    {
        auto d1 = set1.getChannel (chan);
        auto d2 = set2.getChannel (chan);

        for (uint32_t i = 0; i < set1.numFrames; ++i)
        {
            if (d1[i] != d2[i])
                return false;

            d1 += set1.stride;
            d2 += set2.stride;
        }
    }

    return true;
}

//==============================================================================
/** Adds allocation to an InterleavedChannelSet or DiscreteChannelSet */
template <typename ChannelSetType>
struct AllocatedChannelSet
{
    AllocatedChannelSet() = default;

    AllocatedChannelSet (uint32_t numChannels, uint32_t numFrames)
        : channelSet (ChannelSetType::createAllocated (numChannels, numFrames))
    {}

    template <typename OtherChannelSet>
    AllocatedChannelSet (const OtherChannelSet& source)
        : AllocatedChannelSet (source.numChannels, source.numFrames)
    {
        copyChannelSet (channelSet, source);
    }

    AllocatedChannelSet (const AllocatedChannelSet& other)
        : AllocatedChannelSet (other.channelSet)
    {
    }

    AllocatedChannelSet (AllocatedChannelSet&& other)  : channelSet (other.channelSet)
    {
        other.channelSet.resetWithoutFreeingData();
    }

    ~AllocatedChannelSet()
    {
        channelSet.freeData();
    }

    AllocatedChannelSet& operator= (const AllocatedChannelSet& other)
    {
        channelSet.freeData();
        channelSet = ChannelSetType::createAllocated (other.channelSet.numChannels, other.channelSet.numFrames);
        copyChannelSet (channelSet, other.channelSet);
        return *this;
    }

    AllocatedChannelSet& operator= (AllocatedChannelSet&& other)
    {
        channelSet.freeData();
        channelSet = other.channelSet;
        other.channelSet.resetWithoutFreeingData();
        return *this;
    }

    ChannelSetType channelSet;
};

template <typename ChannelSetType>
AllocatedChannelSet<ChannelSetType> createAllocatedCopy (const ChannelSetType& source)
{
    return AllocatedChannelSet<ChannelSetType> (source);
}

template <typename SampleType>
AllocatedChannelSet<InterleavedChannelSet<SampleType>> createAllocatedChannelSet (ArrayView<SampleType> samples, uint32_t numChannels)
{
    return createAllocatedCopy (InterleavedChannelSet<SampleType> { samples.data(), numChannels, (uint32_t) samples.size() / numChannels, numChannels });
}

template <typename SampleType>
AllocatedChannelSet<InterleavedChannelSet<SampleType>> createAllocatedChannelSet (const std::vector<SampleType>& samples, uint32_t numChannels)
{
    return createAllocatedChannelSet (ArrayView<SampleType> (samples.data(), samples.size()), numChannels);
}

} // namespace soul
