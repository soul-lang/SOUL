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
bool isConsoleEndpoint (const std::string& name)
{
    return name == ASTUtilities::getConsoleEndpointInternalName();
}

bool isMIDIMessageStruct (const choc::value::Type& type)
{
    if (! type.isObject())
        return false;

    return choc::text::endsWith (type.getObjectClassName(), "Message")
            && type.getNumElements() == 1
            && type.getObjectMember (0).name == "midiBytes"
            && type.getObjectMember (0).type.isInt32();
}

bool isMIDIEventEndpoint (const EndpointDetails& details)
{
    return isEvent (details)
            && details.dataTypes.size() == 1
            && isMIDIMessageStruct (details.dataTypes.front())
            && ! isConsoleEndpoint (details.name);
}

bool isMIDIEventEndpoint (const Endpoint& details)
{
    return isEvent (details.type)
            && details.valueTypes.size() == 1
            && isMIDIMessageStruct (details.valueTypes.front())
            && ! isConsoleEndpoint (details.name);
}

Type createMIDIEventEndpointType()
{
    StructurePtr s (*new Structure ("Message", nullptr));
    s->getMembers().push_back ({ PrimitiveType::int32, "midiBytes" });
    return Type::createStruct (*s);
}

bool isParameterInput (const EndpointDetails& details)
{
    if (isEvent (details))
    {
        if (isConsoleEndpoint (details.name))
            return false;

        if (details.dataTypes.size() != 1)
            return false;

        return details.dataTypes.front().isPrimitive();
    }

    if (isStream (details) && details.annotation.hasValue ("name"))
        return true;

    if (isValue (details) && details.annotation.hasValue ("name"))
        return true;

    return false;
}

uint32_t getNumAudioChannels (const EndpointDetails& details)
{
    if (isStream (details))
        return details.getFrameType().getNumElements();

    return 0;
}

bool isAudioEndpoint (const EndpointDetails& details)
{
    return getNumAudioChannels (details) != 0;
}

InputEndpointType getInputEndpointType (const EndpointDetails& details)
{
    if (isParameterInput (details))         return InputEndpointType::parameter;
    if (isMIDIEventEndpoint (details))      return InputEndpointType::midi;
    if (isAudioEndpoint (details))          return InputEndpointType::audio;
    if (isEvent (details))                  return InputEndpointType::event;

    return InputEndpointType::other;
}

OutputEndpointType getOutputEndpointType (const EndpointDetails& details)
{
    if (isMIDIEventEndpoint (details))   return OutputEndpointType::midi;
    if (isAudioEndpoint (details))       return OutputEndpointType::audio;
    if (isEvent (details))               return OutputEndpointType::event;

    return OutputEndpointType::other;
}


} // namespace soul
