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
struct Version
{
    uint32_t major, minor, point;

    std::string toString (const char* separator = ".") const
    {
        return std::to_string (major)
                + separator + std::to_string (minor)
                + separator + std::to_string (point);
    }

    bool operator== (const Version& other) const    { return compare (other) == 0; }
    bool operator!= (const Version& other) const    { return compare (other) != 0; }
    bool operator<  (const Version& other) const    { return compare (other) <  0; }
    bool operator<= (const Version& other) const    { return compare (other) <= 0; }
    bool operator>  (const Version& other) const    { return compare (other) >  0; }
    bool operator>= (const Version& other) const    { return compare (other) >= 0; }

    int compare (const Version& other) const
    {
        if (major != other.major)  return major < other.major ? -1 : 1;
        if (minor != other.minor)  return minor < other.minor ? -1 : 1;
        if (point != other.point)  return point < other.point ? -1 : 1;
        return 0;
    }

    static bool isValidElementValue (uint32_t v)
    {
        return v < 32768;
    }
};

//==============================================================================
static inline constexpr double pi = 3.141592653589793238;
static inline constexpr double twoPi = 2 * pi;

template <typename... Types>
void ignoreUnused (Types&&...) noexcept {}

/** Returns whether an exception is in the process of being unwound. */
bool inExceptionHandler();

//==============================================================================
struct ShortMIDIMessage
{
    uint8_t bytes[3] = {};

    uint8_t getChannel0to15() const                 { return bytes[0] & 0x0f; }
    uint8_t getChannel1to16() const                 { return static_cast<uint8_t> ((bytes[0] & 0x0f) + 1); }

    bool isNoteOn() const                           { return getMessageTypeBits() == 0x90 && getVelocity() != 0; }
    bool isNoteOff() const                          { return getMessageTypeBits() == 0x80 || (getVelocity() == 0 && getMessageTypeBits() == 0x90); }
    uint8_t getNoteNumber() const                   { return bytes[1]; }
    uint8_t getVelocity() const                     { return bytes[2]; }

    bool isProgramChange() const                    { return getMessageTypeBits() == 0xc0; }
    uint8_t getProgramChangeNumber() const          { return bytes[1]; }

    bool isPitchWheel() const                       { return getMessageTypeBits() == 0xe0; }
    uint32_t getPitchWheelValue() const             { return get14Bit (1); }
    bool isAftertouch() const                       { return getMessageTypeBits() == 0xa0; }
    uint8_t getAfterTouchValue() const              { return bytes[2]; }

    bool isChannelPressure() const                  { return getMessageTypeBits() == 0xd0; }
    uint8_t getChannelPressureValue() const         { return bytes[1]; }

    bool isController() const                       { return getMessageTypeBits() == 0xb0; }
    uint8_t getControllerNumber() const             { return bytes[1]; }
    uint8_t getControllerValue() const              { return bytes[2]; }
    bool isControllerNumber (uint8_t number) const  { return bytes[1] == number && isController(); }
    bool isAllNotesOff() const                      { return isControllerNumber (123); }
    bool isAllSoundOff() const                      { return isControllerNumber (120); }

    bool isActiveSense() const                      { return bytes[0] == 0xfe; }

    uint8_t getMessageTypeBits() const              { return bytes[0] & 0xf0; }
    uint32_t get14Bit (int index) const             { return bytes[index] | (static_cast<uint32_t> (bytes[index + 1]) << 7); }
};

const char* getMIDINoteName (uint8_t midiNote, bool useSharps);
int getMIDIOctaveNumber (uint8_t midiNote, int octaveForMiddleC = 3);
std::string getMIDIMessageDescription (const uint8_t* data, size_t numBytes);

//==============================================================================
template <typename Type>
constexpr bool isPowerOf2 (Type value)  { return (value & (value - 1)) == 0; }

/** Rounds-up a size to a value which is a multiple of the given granularity. */
template <int granularity, typename SizeType>
constexpr SizeType getAlignedSize (SizeType size)
{
    static_assert (isPowerOf2 (granularity), "granularity must be a power of 2");
    return (size + (SizeType) (granularity - 1)) & ~(SizeType)(granularity - 1);
}

/** Rounds-up a size to a value which is a multiple of the given granularity. */
template <int granularity, typename PointerType>
constexpr bool isAlignedPointer (PointerType p)
{
    static_assert (isPowerOf2 (granularity), "granularity must be a power of 2");
    return (((size_t) reinterpret_cast<const void*> (p)) & (size_t) (granularity - 1)) == 0;
}

/** Rounds-up a pointer to a value which is a multiple of the given granularity in bytes. */
template <int granularity, typename PointerType>
constexpr PointerType getAlignedPointer (PointerType p)
{
    static_assert (isPowerOf2 (granularity), "granularity must be a power of 2");
    return reinterpret_cast<PointerType> ((((size_t) reinterpret_cast<const void*> (p))
                                            + (size_t)(granularity - 1)) & ~(size_t)(granularity - 1));
}

template <typename Type>
Type readUnaligned (const void* srcPtr) noexcept
{
    Type value;
    memcpy (&value, srcPtr, sizeof (Type));
    return value;
}

template <typename Type>
Type readUnaligned (const void* srcPtr, size_t offsetBytes) noexcept
{
    Type value;
    memcpy (&value, static_cast<const char*> (srcPtr) + offsetBytes, sizeof (Type));
    return value;
}

template <typename Type>
void writeUnaligned (void* dstPtr, Type value) noexcept
{
    memcpy (dstPtr, &value, sizeof (Type));
}

template <typename DataType, int granularity>
struct AlignedBuffer
{
    void resize (size_t size, DataType initialValue = {})
    {
        buffer.resize (size + granularity, initialValue);
        ptr = getAlignedPointer<granularity> (buffer.data());
    }

    DataType* data() const      { return ptr; }
    bool empty() const          { return buffer.size() < granularity; }

    AlignedBuffer& operator= (const AlignedBuffer& other)
    {
        if (this != &other)
        {
            buffer = other.buffer;
            ptr = getAlignedPointer<granularity> (buffer.data());
        }

        return *this;
    }

private:
    std::vector<char> buffer;
    DataType* ptr = nullptr;
};

//==============================================================================
struct ScopedDisableDenormals
{
    ScopedDisableDenormals() noexcept;
    ~ScopedDisableDenormals() noexcept;

private:
    const intptr_t oldFlags;

    static intptr_t getFPMode() noexcept;
    static void setFPMode (intptr_t) noexcept;
};


} // namespace soul
