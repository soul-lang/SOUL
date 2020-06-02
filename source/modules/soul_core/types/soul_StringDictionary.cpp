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
    StringDictionary::StringDictionary() = default;
    StringDictionary::~StringDictionary() = default;

    StringDictionary::Handle StringDictionary::getHandleForString (std::string_view text)
    {
        if (text.empty())
            return {};

        for (auto& s : strings)
            if (s.text == text)
                return s.handle;

        auto handle = StringDictionary::Handle { nextIndex++ };
        strings.push_back ({ handle, std::string (text) });
        return handle;
    }

    std::string_view StringDictionary::getStringForHandle (Handle handle) const
    {
        if (handle == Handle())
            return {};

        for (auto& s : strings)
            if (s.handle == handle)
                return s.text;

        SOUL_ASSERT_FALSE;
        return {};
    }
}
