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
    null,
    value,
    stream,
    event
};

const char* getEndpointKindName (EndpointKind);
EndpointKind getEndpointKind (const std::string& name);

inline bool isNull   (EndpointKind kind)         { return kind == EndpointKind::null; }
inline bool isValue  (EndpointKind kind)         { return kind == EndpointKind::value; }
inline bool isStream (EndpointKind kind)         { return kind == EndpointKind::stream; }
inline bool isEvent  (EndpointKind kind)         { return kind == EndpointKind::event; }

template <typename TokeniserType>
static EndpointKind parseEndpointKind (TokeniserType& tokeniser)
{
    if (tokeniser.matchIfKeywordOrIdentifier ("null"))    return EndpointKind::null;
    if (tokeniser.matchIfKeywordOrIdentifier ("value"))   return EndpointKind::value;
    if (tokeniser.matchIfKeywordOrIdentifier ("stream"))  return EndpointKind::stream;
    if (tokeniser.matchIfKeywordOrIdentifier ("event"))   return EndpointKind::event;

    tokeniser.throwError (Errors::expectedStreamType());
    return EndpointKind::null;
}

template <typename TokeniserType>
static bool isNextTokenEndpointKind (TokeniserType& tokeniser)
{
    return tokeniser.matches ("null")
            || tokeniser.matches ("value")
            || tokeniser.matches ("stream")
            || tokeniser.matches ("event");
}

//==============================================================================
using EndpointID = std::string;

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
                     std::vector<Type> sampleTypes, uint32_t strideBytes,
                     Annotation);

    uint32_t getNumAudioChannels() const;
    const Type& getSingleSampleType() const;
    bool isConsoleOutput() const;

    EndpointID endpointID;
    std::string name;
    EndpointKind kind;
    std::vector<Type> sampleTypes;
    uint32_t strideBytes;
    Annotation annotation;
};

//==============================================================================
/**
    Gives status of an endpoint, such as whether it is connected, and what sort
    of connection is in use
*/
struct EndpointConnectionStatus
{
    bool connected = false;
    bool sparse = false;
};

} // namespace soul
