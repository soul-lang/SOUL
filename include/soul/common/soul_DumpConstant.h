
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

#pragma once

namespace soul
{

//==============================================================================
/** Prints out a one-line dump of a value, and looks for things like MIDI messages that
    can be printed appropriately.
*/
inline std::string dump (const choc::value::ValueView& c)
{
    try
    {
        if (c.isInt32())    { auto v = c.getInt32(); return v > 0xffff ? "0x" + choc::text::createHexString (v) : std::to_string (v); }
        if (c.isInt64())    { auto v = c.getInt64(); return v > 0xffff ? "0x" + choc::text::createHexString (v) : std::to_string (v); }

        auto printFloat32 = [] (float value) -> std::string
        {
            if (value == 0)             return "0";
            if (std::isnan (value))     return "_nan32";
            if (std::isinf (value))     return value > 0 ? "_inf32" : "_ninf32";

            return choc::text::floatToString (value) + "f";
        };

        auto printFloat64 = [] (double value) -> std::string
        {
            if (value == 0)             return "0";
            if (std::isnan (value))     return "_nan64";
            if (std::isinf (value))     return value > 0 ? "_inf64" : "_ninf64";

            return choc::text::floatToString (value);
        };

        if (c.isFloat32())  return printFloat32 (c.getFloat32());
        if (c.isFloat64())  return printFloat64 (c.getFloat64());
        if (c.isBool())     return c.getBool() ? "true" : "false";
        if (c.isString())   return std::string (c.getString());
        if (c.isVoid())     return "void";

        if (c.isVector())
        {
            std::string s ("vector (");
            auto num = c.size();

            for (uint32_t i = 0; i < num; ++i)
            {
                if (i != 0)
                    s += ", ";

                s += dump (c[i]);
            }

            return s + ")";
        }

        if (c.isArray())
        {
            std::string s ("array (");
            auto num = c.size();

            for (uint32_t i = 0; i < num; ++i)
            {
                if (i != 0)
                    s += ", ";

                s += dump (c[i]);
            }

            return s + ")";
        }

        if (c.isObject())
        {
            auto s = "object " + std::string (c.getObjectClassName()) + " {";
            auto num = c.size();

            for (uint32_t i = 0; i < num; ++i)
            {
                if (i != 0)
                    s += ", ";
                else
                    s += " ";

                auto m = c.getObjectMemberAt(i);
                s += std::string (m.name) + " = " + dump (m.value);
            }

            s += " }";

            if (num == 1 && c.getObjectClassName() == "midi::Message")
            {
                auto m = c.getObjectMemberAt (0);

                if (std::strcmp (m.name, "midiBytes") == 0 && m.value.isInt32())
                {
                    auto v = m.value.getInt32();

                    s += " = " + choc::midi::ShortMessage ((uint8_t) (v >> 16),
                                                           (uint8_t) (v >> 8),
                                                           (uint8_t) v).getDescription();
                }
            }

            return s;
        }
    }
    catch (choc::value::Error e)
    {
        return e.description;
    }

    return "?";
}

/** Dumps a printout of a type. */
inline std::string dump (const choc::value::Type& type)
{
    if (type.isInt32())     return "int32";
    if (type.isInt64())     return "int64";
    if (type.isFloat32())   return "float32";
    if (type.isFloat64())   return "float64";
    if (type.isBool())      return "bool";
    if (type.isString())    return "string";

    if (type.isVector())        return dump (type.getElementType()) + "<" + std::to_string (type.getNumElements()) + ">";
    if (type.isUniformArray())  return dump (type.getElementType()) + "[" + std::to_string (type.getNumElements()) + "]";

    if (type.isObject())
    {
        std::string s (type.getObjectClassName());
        auto num = type.getNumElements();

        if (num == 0)
            return s + " {}";

        s += " { ";

        for (uint32_t i = 0; i < num; ++i)
        {
            if (i != 0)
                s += ", ";

            auto m = type.getObjectMember (i);
            s += dump (m.type);
            s += " ";
            s += m.name;
        }

        return s + " }";
    }

    return "?";
}


} // namespace soul
