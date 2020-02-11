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

#if ! SOUL_INSIDE_CORE_CPP
 #error "Don't add this cpp file to your build, it gets included indirectly by soul_core.cpp"
#endif

//==============================================================================
Structure::Structure (std::string nm, void* backlink)
  : name (std::move (nm)), backlinkToASTObject (backlink)
{
    SOUL_ASSERT (! containsChar (name, '#'));
}

const Structure::Member& Structure::getMemberWithName (std::string_view memberName) const
{
    for (auto& m : members)
        if (m.name == memberName)
            return m;

    SOUL_ASSERT_FALSE;
    return members.front();
}

void Structure::addMember (Type type, std::string memberName)
{
    members.push_back ({ type, std::move (memberName) });
}

bool Structure::hasMemberWithName (std::string_view memberName) const
{
    for (auto& m : members)
        if (m.name == memberName)
            return true;

    return false;
}

size_t Structure::getMemberIndex (std::string_view memberName) const
{
    size_t index = 0;

    for (auto& member : members)
    {
        if (member.name == memberName)
            return index;

        ++index;
    }

    SOUL_ASSERT_FALSE;
    return index;
}

std::string Structure::addMemberWithUniqueName (Type type, const std::string& memberName)
{
    auto newName = addSuffixToMakeUnique (memberName.empty() ? "temp" : memberName,
                                          [this] (const std::string& nm) { return hasMemberWithName (nm); });
    addMember (type, newName);
    return newName;
}

bool Structure::isEmpty() const noexcept
{
    return members.empty();
}

size_t Structure::getPackedSizeInBytes() const
{
    size_t total = 0;

    for (auto& m : members)
        total += m.type.getPackedSizeInBytes();

    return std::max ((size_t) 1, total);
}

static void checkStructRecursion (Structure* structToCheck, const CodeLocation& location,
                                  ArrayWithPreallocation<Structure*, 8>& parentStructs)
{
    SOUL_ASSERT (structToCheck != nullptr);
    parentStructs.push_back (structToCheck);

    for (auto& m : structToCheck->members)
    {
        if (m.type.isStruct())
        {
            auto s = m.type.getStruct();

            if (contains (parentStructs, s.get()))
            {
                // Break the circular reference to avoid leaking the structure
                m.type = {};

                if (s == structToCheck)
                    location.throwError (Errors::typeContainsItself (structToCheck->name));

                location.throwError (Errors::typesReferToEachOther (structToCheck->name, s->name));
            }

            checkStructRecursion (s.get(), location, parentStructs);
        }
    }

    parentStructs.pop_back();
}

void Structure::checkForRecursiveNestedStructs (const CodeLocation& location)
{
    ArrayWithPreallocation<Structure*, 8> parentStructs;
    checkStructRecursion (this, location, parentStructs);
}

} // namespace soul
