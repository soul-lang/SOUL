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
EndpointDetails::EndpointDetails (EndpointID id, std::string nm, EndpointType t,
                                  const std::vector<Type>& types, Annotation a)
   : endpointID (std::move (id)),
     name (std::move (nm)),
     endpointType (t),
     annotation (std::move (a))
{
    dataTypes.reserve (types.size());

    for (auto& type : types)
        dataTypes.push_back (type.getExternalType());
}

const choc::value::Type& EndpointDetails::getFrameType() const
{
    SOUL_ASSERT (isStream (endpointType) && dataTypes.size() == 1);
    return dataTypes.front();
}

const choc::value::Type& EndpointDetails::getValueType() const
{
    SOUL_ASSERT (isValue (endpointType) && dataTypes.size() == 1);
    return dataTypes.front();
}

const choc::value::Type& EndpointDetails::getSingleEventType() const
{
    SOUL_ASSERT (isEvent (endpointType) && dataTypes.size() == 1);
    return dataTypes.front();
}

bool EndpointDetails::isConsoleOutput() const
{
    return name == ASTUtilities::getConsoleEndpointInternalName();
}

const char* getEndpointTypeName (EndpointType type)
{
    auto t = endpointTypeToString (type);
    SOUL_ASSERT (t != nullptr);
    return t;
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
