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
const char* getEndpointTypeName (EndpointType);

inline bool isValue  (EndpointType t)         { return t == EndpointType::value; }
inline bool isStream (EndpointType t)         { return t == EndpointType::stream; }
inline bool isEvent  (EndpointType t)         { return t == EndpointType::event; }

template <typename Type> bool isValue  (const Type& t)    { return isValue (t.endpointType); }
template <typename Type> bool isStream (const Type& t)    { return isStream (t.endpointType); }
template <typename Type> bool isEvent  (const Type& t)    { return isEvent (t.endpointType); }

template <typename TokeniserType>
static EndpointType parseEndpointType (TokeniserType& tokeniser)
{
    if (tokeniser.matchIfKeywordOrIdentifier ("value"))   return EndpointType::value;
    if (tokeniser.matchIfKeywordOrIdentifier ("stream"))  return EndpointType::stream;
    if (tokeniser.matchIfKeywordOrIdentifier ("event"))   return EndpointType::event;

    tokeniser.throwError (Errors::expectedStreamType());
    return EndpointType::value;
}

template <typename TokeniserType>
static bool isNextTokenEndpointType (TokeniserType& tokeniser)
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
    static EndpointHandle create (EndpointType type, uint32_t rawHandle)
    {
        SOUL_ASSERT ((rawHandle >> 24) == 0);
        EndpointHandle h;
        h.handle = rawHandle | (static_cast<uint32_t> (type) << 24);
        return h;
    }

    using RawHandleType = uint32_t;

    RawHandleType getRawHandle() const                  { return handle & 0xffffff; }

    bool isValid() const                                { return handle != 0; }

    bool operator== (EndpointHandle other) const        { return other.handle == handle; }
    bool operator!= (EndpointHandle other) const        { return other.handle != handle; }

    EndpointType getType() const                        { return static_cast<EndpointType> (handle >> 24); }

    bool isValue() const                                { return getType() == EndpointType::value; }
    bool isStream() const                               { return getType() == EndpointType::stream; }
    bool isEvent() const                                { return getType() == EndpointType::event; }

private:
    RawHandleType handle = 0;
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

    EndpointDetails (EndpointID, std::string name, EndpointType,
                     const std::vector<Type>& dataTypes, Annotation);

    const choc::value::Type& getFrameType() const;
    const choc::value::Type& getValueType() const;
    const choc::value::Type& getSingleEventType() const;
    bool isConsoleOutput() const;

    EndpointID endpointID;
    std::string name;
    EndpointType endpointType;

    /** The types of the frames or events that this endpoint uses.
        For an event endpoint, there may be multiple data types for the different
        event types it can handle. For streams and values, there should be exactly
        one type in this array.
    */
    ArrayWithPreallocation<choc::value::Type, 2> dataTypes;
    Annotation annotation;
};

inline EndpointDetails endpointToEndpointDetails (const Endpoint& e)
{
    EndpointDetails d;
    d.endpointID = EndpointID::create (e.ID);
    d.name = e.name;
    d.endpointType = e.type;
    d.dataTypes = e.valueTypes;
    d.annotation = Annotation::fromExternalValue (e.annotation);
    return d;
}

inline std::vector<EndpointDetails> endpointToEndpointDetails (choc::span<Endpoint> endpoints)
{
    std::vector<EndpointDetails> results;
    results.reserve (endpoints.size());

    for (auto& e : endpoints)
        results.push_back (endpointToEndpointDetails (e));

    return results;
}

inline Endpoint endpointDetailsToEndpoint (const EndpointDetails& d)
{
    Endpoint e;
    e.ID = d.endpointID.toString();
    e.name = d.name;
    e.type = d.endpointType;

    for (auto& t : d.dataTypes)
        e.valueTypes.push_back (t);

    e.annotation = d.annotation.toExternalValue();
    return e;
}

inline std::vector<Endpoint> endpointDetailsToEndpoint (choc::span<EndpointDetails> endpoints)
{
    std::vector<Endpoint> results;
    results.reserve (endpoints.size());

    for (auto& e : endpoints)
        results.push_back (endpointDetailsToEndpoint (e));

    return results;
}


} // namespace soul

namespace std
{
    template <>
    struct hash<soul::EndpointHandle>
    {
        size_t operator() (const soul::EndpointHandle& p) const noexcept { return static_cast<size_t> (p.getRawHandle()); }
    };
}
