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

#if ! SOUL_INSIDE_CORE_CPP
 #error "Don't add this cpp file to your build, it gets included indirectly by soul_core.cpp"
#endif

namespace soul
{

//==============================================================================
EndpointProperties::EndpointProperties (double rate, uint32_t size)
   : sampleRate (rate), blockSize (size), initialised (true)
{
    SOUL_ASSERT (sampleRate != 0);
    SOUL_ASSERT (blockSize != 0);
}

EndpointDetails::EndpointDetails (EndpointID id, std::string nm, EndpointKind kind_,
                                  Type sampleType_, uint32_t stride, Annotation a)
   : endpointID (std::move (id)), name (std::move (nm)), kind (kind_),
     sampleType (sampleType_), strideBytes (stride),
     annotation (std::move (a))
{
}

uint32_t EndpointDetails::getNumAudioChannels() const
{
    if (isStream (kind))
    {
        if (sampleType.isFloatingPoint())
        {
            if (sampleType.isPrimitive())
                return 1;

            if (sampleType.isVector())
                return (uint32_t) sampleType.getVectorSize();
        }
    }

    return 0;
}

const char* getEndpointKindName (EndpointKind kind)
{
    switch (kind)
    {
        case EndpointKind::value:   return "value";
        case EndpointKind::stream:  return "stream";
        case EndpointKind::null:    return "null";
        case EndpointKind::event:   return "event";
    }

    SOUL_ASSERT_FALSE;
    return "";
}

EndpointKind getEndpointKind (const std::string& name)
{
    if (name == "value")   return soul::EndpointKind::value;
    if (name == "stream")  return soul::EndpointKind::stream;
    if (name == "null")    return soul::EndpointKind::null;
    if (name == "event")   return soul::EndpointKind::event;

    return soul::EndpointKind::null;
}

const char* getInterpolationDescription (InterpolationType type)
{
    switch (type)
    {
        case InterpolationType::none:    return "none";
        case InterpolationType::latch:   return "latch";
        case InterpolationType::linear:  return "linear";
        case InterpolationType::sinc:    return "sinc";
        case InterpolationType::fast:    return "fast";
        case InterpolationType::best:    return "best";
    }

    SOUL_ASSERT_FALSE;
    return "";
}

bool isSpecificInterpolationType (InterpolationType t)
{
    return t == InterpolationType::latch
        || t == InterpolationType::linear
        || t == InterpolationType::sinc;
}

template <typename TokeniserType>
static InterpolationType parseInterpolationType (TokeniserType& tokeniser)
{
    if (tokeniser.matchIfKeywordOrIdentifier ("none"))   return InterpolationType::none;
    if (tokeniser.matchIfKeywordOrIdentifier ("latch"))  return InterpolationType::latch;
    if (tokeniser.matchIfKeywordOrIdentifier ("linear")) return InterpolationType::linear;
    if (tokeniser.matchIfKeywordOrIdentifier ("sinc"))   return InterpolationType::sinc;
    if (tokeniser.matchIfKeywordOrIdentifier ("fast"))   return InterpolationType::fast;
    if (tokeniser.matchIfKeywordOrIdentifier ("best"))   return InterpolationType::best;

    tokeniser.throwError (Errors::expectedInterpolationType());
    return InterpolationType::none;
}

}
