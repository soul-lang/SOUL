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
/** Represents a structure.
    @see Type::createStruct()
*/
class Structure   : public RefCountedObject
{
public:
    Structure (std::string name, void* backlinkToASTObject);

    Structure (const Structure&) = default;
    Structure (Structure&&) = default;
    Structure& operator= (const Structure&) = delete;
    Structure& operator= (Structure&&) = delete;

    struct Member
    {
        Type type;
        std::string name;
        ReadWriteCount readWriteCount;
    };

    bool empty() const                                              { return members.empty(); }
    const std::string& getName() const noexcept                     { return name; }
    size_t getNumMembers() const                                    { return members.size(); }

    ArrayWithPreallocation<Member, 8>& getMembers()                 { return members; }
    const ArrayWithPreallocation<Member, 8>& getMembers() const     { return members; }

    const Type&           getMemberType (size_t i) const            { return members[i].type; }
    const std::string&    getMemberName (size_t i) const            { return members[i].name; }
    const ReadWriteCount& getMemberReadWriteCount (size_t i) const  { return members[i].readWriteCount; }

    void removeMember (std::string_view memberName);

    // Because the Structure class has no dependency on any AST classes,
    // this opaque pointer is a necessary evil for us to provide a way to
    // quickly trace a structure instance back to its originating AST object.
    void* backlinkToASTObject = nullptr;

    bool activeUseFlag = false;

    Member& getMemberWithName (std::string_view memberName);
    bool hasMemberWithName (std::string_view memberName) const;
    size_t getMemberIndex (std::string_view memberName) const;

    void addMember (Type, std::string memberName);
    std::string addMemberWithUniqueName (Type, const std::string& memberName);

    bool isEmpty() const noexcept;
    uint64_t getPackedSizeInBytes() const;

    void checkForRecursiveNestedStructs (const CodeLocation&);

    bool containsMemberOfType (const Type& type, bool checkSubStructs) const;
    void updateMemberType (std::string_view memberName, const Type& newType);

private:
    ArrayWithPreallocation<Member, 8> members;

    std::string name;
    std::unordered_map<std::string, size_t> memberIndexMap;
};


} // namespace soul
