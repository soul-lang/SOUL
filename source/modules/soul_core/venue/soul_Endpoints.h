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
template <typename ArrayType>
const EndpointDetails& findDetailsForID (const ArrayType& endpoints, const EndpointID& endpointID)
{
    for (auto& e : endpoints)
        if (e.endpointID == endpointID)
            return e;

    SOUL_ASSERT_FALSE;
    return endpoints.front();
}

template <typename ArrayType>
bool containsEndpoint (const ArrayType& endpoints, const EndpointID& endpointID)
{
    for (auto& e : endpoints)
        if (e.endpointID == endpointID)
            return true;

    return false;
}

template <typename VenueOrPerformer>
EndpointID findFirstInputOfType (VenueOrPerformer& p, EndpointType t)
{
    for (auto& i : p.getInputEndpoints())
        if (i.endpointType == t)
            return i.endpointID;

    return {};
}

template <typename VenueOrPerformer>
EndpointID findFirstOutputOfType (VenueOrPerformer& p, EndpointType t)
{
    for (auto& o : p.getOutputEndpoints())
        if (o.endpointType == t)
            return o.endpointID;

    return {};
}

bool isMIDIMessageStruct (const choc::value::Type&);
bool isMIDIEventEndpoint (const EndpointDetails&);
bool isMIDIEventEndpoint (const Endpoint&);
Type createMIDIEventEndpointType();
bool isParameterInput (const EndpointDetails&);
bool isConsoleEndpoint (const std::string& endpointName);
bool isAudioEndpoint (const EndpointDetails&);
uint32_t getNumAudioChannels (const EndpointDetails&);

enum class InputEndpointType
{
    audio,
    parameter,
    midi,
    event,
    other
};

enum class OutputEndpointType
{
    audio,
    midi,
    event,
    other
};

InputEndpointType getInputEndpointType (const EndpointDetails&);
OutputEndpointType getOutputEndpointType (const EndpointDetails&);

template <typename PerformerOrSession>
std::vector<EndpointDetails> getInputEndpointsOfType (PerformerOrSession& p, InputEndpointType type)
{
    std::vector<EndpointDetails> results;

    for (auto& e : p.getInputEndpoints())
        if (getInputEndpointType (e) == type)
            results.push_back (e);

    return results;
}

template <typename PerformerOrSession>
std::vector<EndpointDetails> getOutputEndpointsOfType (PerformerOrSession& p, OutputEndpointType type)
{
    std::vector<EndpointDetails> results;

    for (auto& e : p.getOutputEndpoints())
        if (getOutputEndpointType (e) == type)
            results.push_back (e);

    return results;
}

} // namespace soul
