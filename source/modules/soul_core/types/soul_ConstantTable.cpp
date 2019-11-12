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
    ConstantTable::ConstantTable() = default;
    ConstantTable::~ConstantTable() = default;

    const ConstantTable::Item* ConstantTable::begin() const   { return items.begin(); }
    const ConstantTable::Item* ConstantTable::end() const     { return items.end(); }
    size_t ConstantTable::size() const                        { return items.size(); }

    ConstantTable::Handle ConstantTable::getHandleForValue (Value value)
    {
        if (! value.isValid())
            return 0;

        for (auto& i : items)
            if (value == *i.value)
                return i.handle;

        auto handle = nextIndex++;
        items.push_back ({ handle, std::make_unique<Value> (std::move (value)) });
        return handle;
    }

    const Value* ConstantTable::getValueForHandle (Handle handle) const
    {
        if (handle == 0)
            return {};

        for (auto& i : items)
            if (handle == i.handle)
                return i.value.get();

        SOUL_ASSERT_FALSE;
        return {};
    }

    void ConstantTable::addItem (Item i)
    {
        nextIndex = std::max (nextIndex, i.handle + 1);
        items.push_back (std::move (i));
    }
}
