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
/** A single-reader, single-writer threaded FIFO.
*/
struct FIFO
{
    FIFO (int size) : totalSize (size), freeSize (size - 1) {}

    std::mutex lock;
    std::condition_variable changed;
    int totalSize, validStart = 0, validSize = 0, freeSize = 0;
    bool isCancelled = false;

    int getTotalSize() const noexcept    { return totalSize; }
    int getFreeSpace() const noexcept    { return freeSize; }
    int getNumReady() const noexcept     { return validSize; }

    void reset()
    {
        std::lock_guard<std::mutex> l (lock);
        validStart = 0;
        validSize = 0;
        freeSize = totalSize - 1;
        isCancelled = false;
        changed.notify_all();
    }

    void cancel()
    {
        std::lock_guard<std::mutex> l (lock);
        isCancelled = true;
        changed.notify_all();
    }

    //==============================================================================
    struct ReadOperation
    {
        ReadOperation (FIFO& f, int numWanted, std::chrono::high_resolution_clock::time_point deadline) : fifo (f)
        {
            SOUL_ASSERT (numWanted > 0 && numWanted <= f.totalSize);
            std::unique_lock<std::mutex> l (f.lock);

            while (f.validSize < numWanted)
                if (! f.waitForChange (l, deadline))
                    return;

            startIndex1 = f.validStart;
            blockSize1 = std::min (f.totalSize - startIndex1, numWanted);
            blockSize2 = std::max (0, numWanted - blockSize1);
        }

        ~ReadOperation()
        {
            std::lock_guard<std::mutex> l (fifo.lock);
            auto numDone = blockSize1 + blockSize2;
            auto newStart = fifo.validStart + numDone;
            fifo.validStart = newStart >= fifo.totalSize ? (newStart - fifo.totalSize) : newStart;
            fifo.validSize -= numDone;
            fifo.freeSize += numDone;
            fifo.changed.notify_all();
        }

        bool failed() const         { return blockSize1 == 0; }

        FIFO& fifo;
        int startIndex1 = 0, blockSize1 = 0, blockSize2 = 0;
    };

    //==============================================================================
    struct WriteOperation
    {
        WriteOperation (FIFO& f, int numToWrite, std::chrono::high_resolution_clock::time_point deadline) : fifo (f)
        {
            SOUL_ASSERT (numToWrite > 0 && numToWrite <= f.totalSize);
            std::unique_lock<std::mutex> l (f.lock);

            while (f.freeSize < numToWrite)
                if (! f.waitForChange (l, deadline))
                    return;

            auto freeStart = f.validStart + f.validSize;
            startIndex1 = freeStart >= f.totalSize ? freeStart - f.totalSize : freeStart;
            blockSize1 = std::min (f.totalSize - startIndex1, numToWrite);
            blockSize2 = std::max (0, numToWrite - blockSize1);
        }

        ~WriteOperation()
        {
            std::lock_guard<std::mutex> l (fifo.lock);
            auto numDone = blockSize1 + blockSize2;
            fifo.validSize += numDone;
            fifo.freeSize -= numDone;
            fifo.changed.notify_all();
        }

        bool failed() const         { return blockSize1 == 0; }

        FIFO& fifo;
        int startIndex1 = 0, blockSize1 = 0, blockSize2 = 0;
    };

private:
    bool waitForChange (std::unique_lock<std::mutex>& l,
                        std::chrono::high_resolution_clock::time_point deadline)
    {
        return ! (isCancelled || changed.wait_until (l, deadline) == std::cv_status::timeout);
    }
};

} // namespace soul
