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
  : backlinkToASTObject (backlink), name (std::move (nm))
{
    SOUL_ASSERT (! containsChar (name, '#'));
}

Structure::Member& Structure::getMemberWithName (std::string_view memberName)
{
    return members[getMemberIndex (memberName)];
}

void Structure::addMember (Type type, std::string memberName)
{
    SOUL_ASSERT (! hasMemberWithName (memberName));

    // Ensure we won't create a recursive structure (one containing itself) if we add a member of this type
    SOUL_ASSERT (! (type.isStruct() && type.getStruct()->containsMemberOfType (Type::createStruct (*this), true)));

    memberIndexMap[memberName] = members.size();
    members.push_back ({ std::move (type), std::move (memberName) });

}

void Structure::removeMember (std::string_view memberName)
{
    auto i = getMemberIndex (memberName);

    for (auto& m : memberIndexMap)
        if (m.second >= i)
            m.second--;

    memberIndexMap.erase (std::string (memberName));
    members.erase (members.begin() + i);
}


bool Structure::hasMemberWithName (std::string_view memberName) const
{
    return (memberIndexMap.find (std::string (memberName)) != memberIndexMap.end());
}

size_t Structure::getMemberIndex (std::string_view memberName) const
{
    auto i = memberIndexMap.find (std::string (memberName));

    if (i != memberIndexMap.end())
        return i->second;

    SOUL_ASSERT_FALSE;
    return 0;
}

std::string Structure::addMemberWithUniqueName (Type type, const std::string& memberName)
{
    auto newName = addSuffixToMakeUnique (memberName.empty() ? "temp" : memberName,
                                          [this] (const std::string& nm) { return hasMemberWithName (nm); });
    addMember (std::move (type), newName);
    return newName;
}

bool Structure::isEmpty() const noexcept
{
    return members.empty();
}

uint64_t Structure::getPackedSizeInBytes() const
{
    uint64_t total = 0;

    for (auto& m : members)
        total += m.type.getPackedSizeInBytes();

    return std::max ((uint64_t) 1, total);
}

static void checkStructRecursion (Structure* structToCheck, const CodeLocation& location,
                                  ArrayWithPreallocation<Structure*, 8>& parentStructs)
{
    SOUL_ASSERT (structToCheck != nullptr);
    parentStructs.push_back (structToCheck);

    for (auto& m : structToCheck->getMembers())
    {
        if (m.type.isStruct())
        {
            auto s = m.type.getStruct();

            if (contains (parentStructs, s.get()))
            {
                // Break the circular reference to avoid leaking the structure
                m.type = {};

                if (s == structToCheck)
                    location.throwError (Errors::typeContainsItself (structToCheck->getName()));

                location.throwError (Errors::typesReferToEachOther (structToCheck->getName(), s->getName()));
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

bool Structure::containsMemberOfType (const Type& type, bool checkSubStructs) const
{
    for (auto& m : members)
    {
        if (m.type.isEqual (type, Type::failOnAllDifferences))
            return true;

        if (checkSubStructs && m.type.isStruct() && m.type.getStruct()->containsMemberOfType (type, checkSubStructs))
            return true;
    }

    return false;
}

void Structure::updateMemberType (std::string_view memberName, const Type& newType)
{
    auto index = getMemberIndex (memberName);
    members[index].type = newType;
}


} // namespace soul
