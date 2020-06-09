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

    return endsWith (type.getObjectClassName(), "Message")
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

Type createMIDIEventEndpointType()
{
    StructurePtr s (*new Structure ("Message", nullptr));
    s->getMembers().push_back ({ PrimitiveType::int32, "midiBytes" });
    return Type::createStruct (*s);
}

bool isParameterInput (const EndpointDetails& details)
{
    if (isEvent (details) && ! isMIDIEventEndpoint (details))
        return true;

    if (isStream (details) && details.annotation.hasValue ("name"))
        return true;

    if (isValue (details) && details.annotation.hasValue ("name"))
        return true;

    return false;
}

} // namespace soul
