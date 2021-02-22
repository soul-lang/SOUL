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

struct FunctionNames
{
    static constexpr const char* getPrepareFunctionName()           { return "_prepare"; }
    static constexpr const char* getNumXRunsFunctionName()          { return "_get_num_xruns"; }

    static std::string addInputEvent (const heart::InputDeclaration& input, const Type& t)   { SOUL_ASSERT (input.isEventEndpoint());  return "_addInputEvent" + heart::getEventFunctionName (input.name.toString(), t); }
    static std::string getInputFrameArrayRef (const heart::InputDeclaration& input)          { SOUL_ASSERT (input.isStreamEndpoint()); return "_getInputFrameArrayRef_" + input.name.toString(); }
    static std::string setSparseInputTarget (const heart::InputDeclaration& input)           { SOUL_ASSERT (input.isStreamEndpoint()); return "_setSparseInputTarget_" + input.name.toString(); }
    static std::string setInputValue (const heart::InputDeclaration& input)                  { SOUL_ASSERT (input.isValueEndpoint());  return "_setInputValue_" + input.name.toString(); }

    static std::string getOutputFrameArrayRef (const heart::OutputDeclaration& output)       { SOUL_ASSERT (output.isStreamEndpoint()); return "_getOutputFrameArrayRef_" + output.name.toString(); }
    static std::string getNumOutputEvents (const heart::OutputDeclaration& output)           { SOUL_ASSERT (output.isEventEndpoint());  return "_getNumOutputEvents_" + output.name.toString(); }
    static std::string getOutputEventRef (const heart::OutputDeclaration& output)            { SOUL_ASSERT (output.isEventEndpoint());  return "_getOutputEventRef_" + output.name.toString(); }
    static std::string getOutputValue (const heart::OutputDeclaration& output)               { SOUL_ASSERT (output.isValueEndpoint());  return "_getOutputValue_" + output.name.toString(); }
};

}
