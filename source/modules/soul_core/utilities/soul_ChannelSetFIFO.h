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
struct ChannelSetFIFO
{
    ChannelSetFIFO (uint32_t numChannels, uint32_t fifoSize)
        : buffer (numChannels, fifoSize),
          fifo ((int) fifoSize)
    {
        buffer.channelSet.clear();
    }

    ~ChannelSetFIFO()
    {
        cancel();
    }

    /** Resets the FIFO to an initial state. */
    void reset()
    {
        cancel();
        fifo.reset();
    }

    /** Disables the FIFO, blocking until everything currently waiting on it has
        been cancelled. All subsequent read/writes will fail immediately.
    */
    void cancel()
    {
        fifo.cancel();
        std::lock_guard<std::mutex> lock1 (writeLock);
        std::lock_guard<std::mutex> lock2 (readLock);
        buffer.channelSet.clear();
    }

    /** Attempts to write a number of samples to the FIFO.
        This fails if there's not enough space or the FIFO has been cancelled, or the timeout is passed
    */
    template <typename SourceType>
    bool writeBlocking (SourceType sourceData, std::chrono::high_resolution_clock::time_point deadline)
    {
        std::lock_guard<std::mutex> lock (writeLock);

        FIFO::WriteOperation w (fifo, (int) sourceData.numFrames, deadline);

        if (w.failed())
            return false;

        copyChannelSetToFit (buffer.channelSet.getSlice ((uint32_t) w.startIndex1, (uint32_t) w.blockSize1),
                             sourceData.getSlice (0, (uint32_t) w.blockSize1));

        if (w.blockSize2 != 0)
            copyChannelSetToFit (buffer.channelSet.getSlice (0, (uint32_t) w.blockSize2),
                                 sourceData.getSlice ((uint32_t) w.blockSize1, (uint32_t) w.blockSize2));

        return true;
    }

    /** Attempts to read a number of samples to the FIFO.
        This fails immediately if there's not enough data ready or the FIFO has been cancelled, or
        the timeout is passed.
    */
    template <typename DestType>
    bool readBlocking (DestType dest, std::chrono::high_resolution_clock::time_point deadline)
    {
        std::lock_guard<std::mutex> lock (readLock);

        FIFO::ReadOperation r (fifo, (int) dest.numFrames, deadline);

        if (r.failed())
        {
            dest.clear();
            return false;
        }

        copyChannelSetToFit (dest.getSlice (0, (uint32_t) r.blockSize1),
                             buffer.channelSet.getSlice ((uint32_t) r.startIndex1, (uint32_t) r.blockSize1));

        if (r.blockSize2 != 0)
            copyChannelSetToFit (dest.getSlice ((uint32_t) r.blockSize1, (uint32_t) r.blockSize2),
                                 buffer.channelSet.getSlice (0, (uint32_t) r.blockSize2));

        return true;
    }

private:
    //==============================================================================
    AllocatedChannelSet<InterleavedChannelSet<float>> buffer;
    FIFO fifo;
    std::mutex readLock, writeLock;
};

}
