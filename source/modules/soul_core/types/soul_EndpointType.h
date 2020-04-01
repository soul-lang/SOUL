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
enum class EndpointKind
{
    value,
    stream,
    event
};

const char* getEndpointKindName (EndpointKind);

inline bool isValue  (EndpointKind kind)         { return kind == EndpointKind::value; }
inline bool isStream (EndpointKind kind)         { return kind == EndpointKind::stream; }
inline bool isEvent  (EndpointKind kind)         { return kind == EndpointKind::event; }

template <typename TokeniserType>
static EndpointKind parseEndpointKind (TokeniserType& tokeniser)
{
    if (tokeniser.matchIfKeywordOrIdentifier ("value"))   return EndpointKind::value;
    if (tokeniser.matchIfKeywordOrIdentifier ("stream"))  return EndpointKind::stream;
    if (tokeniser.matchIfKeywordOrIdentifier ("event"))   return EndpointKind::event;

    tokeniser.throwError (Errors::expectedStreamType());
    return EndpointKind::value;
}

template <typename TokeniserType>
static bool isNextTokenEndpointKind (TokeniserType& tokeniser)
{
    return tokeniser.matches ("value")
            || tokeniser.matches ("stream")
            || tokeniser.matches ("event");
}

//==============================================================================
/** Holds the name of an input or output endpoint. */
struct EndpointID
{
    static EndpointID create (std::string s)    { EndpointID i; i.ID = std::move (s); return i; }
    const std::string& toString() const         { return ID; }

    operator bool() const   { return ! ID.empty(); }
    bool isValid() const    { return ! ID.empty(); }

    bool operator== (const EndpointID& other) const     { return other.ID == ID; }
    bool operator!= (const EndpointID& other) const     { return other.ID != ID; }

private:
    std::string ID;

    template <typename Type> operator Type() const = delete;
};

//==============================================================================
/** A transient opaque reference to an input or output endpoint.
    Handles are created by a Performer or Venue to refer to an endpoint, and are
    only valid for the lifetime that a linked program is active.
*/
struct EndpointHandle
{
    static EndpointHandle create (uint32_t rawHandle)   { EndpointHandle h; h.handle = rawHandle; return h; }
    uint32_t getRawHandle() const                       { return handle; }

    operator bool() const   { return handle != 0; }
    bool isValid() const    { return handle != 0; }

    bool operator== (EndpointHandle other) const     { return other.handle == handle; }
    bool operator!= (EndpointHandle other) const     { return other.handle != handle; }

private:
    uint32_t handle = 0;

    template <typename Type> operator Type() const = delete;
};

//==============================================================================
/**
    Contains properties describing the unchanging characteristics of an input
    or output endpoint.
*/
struct EndpointDetails
{
    EndpointDetails() = default;
    EndpointDetails (const EndpointDetails&) = default;
    EndpointDetails (EndpointDetails&&) = default;
    EndpointDetails& operator= (const EndpointDetails&) = default;
    EndpointDetails& operator= (EndpointDetails&&) = default;

    EndpointDetails (EndpointID, std::string name, EndpointKind,
                     const std::vector<Type>& dataTypes, Annotation);

    uint32_t getNumAudioChannels() const;
    const Type& getFrameType() const;
    const Type& getValueType() const;
    const Type& getSingleEventType() const;
    bool isConsoleOutput() const;

    EndpointID endpointID;
    std::string name;
    EndpointKind kind;

    /** The types of the frames or events that this endpoint uses.
        For an event endpoint, there may be multiple data types for the different
        event types it can handle. For streams and values, there should be exactly
        one type in this array.
    */
    ArrayWithPreallocation<Type, 2> dataTypes;
    Annotation annotation;
};


} // namespace soul
