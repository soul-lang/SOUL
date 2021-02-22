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
/** An identifier is a pooled string which can be quickly compared. */
struct Identifier  final
{
    Identifier() = default;
    Identifier (const Identifier&) = default;
    Identifier& operator= (const Identifier&) = default;

    bool isValid() const noexcept                                   { return name != nullptr; }
    operator const std::string&() const                             { SOUL_ASSERT (isValid()); return *name; }
    operator std::string_view() const                               { SOUL_ASSERT (isValid()); return std::string_view (*name); }
    const std::string& toString() const                             { SOUL_ASSERT (isValid()); return *name; }
    size_t length() const                                           { SOUL_ASSERT (isValid()); return name->length(); }
    std::string toStringWithFallback (const std::string& fallback) const  { return isValid() ? *name : fallback; }

    bool operator== (const Identifier& other) const noexcept        { return name == other.name; }
    bool operator!= (const Identifier& other) const noexcept        { return name != other.name; }
    bool operator== (std::string_view other) const                  { SOUL_ASSERT (isValid()); return *name == other; }
    bool operator!= (std::string_view other) const                  { SOUL_ASSERT (isValid()); return *name != other; }

    //==============================================================================
    struct Pool  final
    {
        Pool() = default;
        Pool (const Pool&) = delete;
        Pool (Pool&&) = default;

        Identifier get (std::string_view newString)
        {
            SOUL_ASSERT (! newString.empty());

            size_t s = 0;
            size_t e = strings.size();

            while (s < e)
            {
                auto* elem = strings[s].get();

                if (*elem == newString)
                    return Identifier (elem);

                auto halfway = (s + e) / 2;
                bool isBeforeHalfway = (newString < *strings[halfway]);

                if (halfway == s)
                {
                    if (! isBeforeHalfway)
                        ++s;

                    break;
                }

                if (isBeforeHalfway)
                    e = halfway;
                else
                    s = halfway;
            }

            auto* sharedString = new std::string (newString);
            strings.insert (strings.begin() + (int) s, std::unique_ptr<std::string> (sharedString));
            return Identifier (sharedString);
        }

        Identifier get (const Identifier& i)
        {
            if (! i.isValid())
                return {};

            return get (static_cast<std::string_view> (i));
        }

        void clear()
        {
            strings.clear();
        }

    private:
        std::vector<std::unique_ptr<std::string>> strings;
    };

private:
    //==============================================================================
    friend struct Pool;
    const std::string* name = nullptr;

    explicit Identifier (const std::string* s) : name (s) {}
};

//==============================================================================
/** Utilities for parsing and concatenating qualified identifiers into paths
    where the separator is "::".
*/
struct TokenisedPathString
{
    TokenisedPathString (std::string path) : fullPath (std::move (path))
    {
        size_t currentSectionStart = 0;
        auto length = fullPath.length();

        while (currentSectionStart < length)
        {
            auto nextBreak = fullPath.find ("::", currentSectionStart);
            SOUL_ASSERT (nextBreak != 0);

            if (nextBreak == std::string::npos)
            {
                sections.push_back ({ currentSectionStart, length });
                break;
            }

            sections.push_back ({ currentSectionStart, nextBreak });
            currentSectionStart = nextBreak + 2;
        }
    }

    std::string getSection (size_t index) const
    {
        return getSection (sections[index]);
    }

    std::string getLastPart() const
    {
        if (sections.empty())
            return {};

        return getSection (sections.back());
    }

    std::string getParentPath() const
    {
        if (sections.size() <= 1)
            return {};

        return choc::text::trim (fullPath.substr (0, sections[sections.size() - 2].end));
    }

    static std::string join (const std::string& parent, const std::string& child)
    {
        return parent + "::" + child;
    }

    static std::string removeTopLevelNameIfPresent (const std::string& path, std::string nameToRemove)
    {
        nameToRemove += "::";

        if (choc::text::startsWith (path, nameToRemove))
            return path.substr (nameToRemove.length());

        return path;
    }

    std::string fullPath;

    struct Section { size_t start, end; };
    ArrayWithPreallocation<Section, 8> sections;

    std::string getSection (Section s) const        { return choc::text::trim (fullPath.substr (s.start, s.end - s.start)); }
};


//==============================================================================
/** Holds a list of identifiers for use as a qualified name, e.g. foo::bar::xyz */
struct IdentifierPath  final
{
    IdentifierPath() {}
    IdentifierPath (const IdentifierPath&) = default;
    IdentifierPath (IdentifierPath&&) = default;
    IdentifierPath& operator= (const IdentifierPath&) = default;
    IdentifierPath& operator= (IdentifierPath&&) = default;

    explicit IdentifierPath (Identifier i) { addSuffix (i); }
    IdentifierPath (IdentifierPath parent, Identifier i) : pathSections (std::move (parent.pathSections)) { addSuffix (i); }

    bool empty() const              { return size() == 0; }
    bool isValid() const            { return size() > 0; }
    bool isUnqualified() const      { return size() == 1; }
    bool isQualified() const        { return size() > 1; }

    bool isUnqualifiedName (const char* nameToCompare) const          { return isUnqualified() && pathSections.front() == nameToCompare; }
    bool isUnqualifiedName (const std::string& nameToCompare) const   { return isUnqualified() && pathSections.front() == nameToCompare; }
    bool isUnqualifiedName (const Identifier& nameToCompare) const    { return isUnqualified() && pathSections.front() == nameToCompare; }

    bool matchesLastSectionOf (const IdentifierPath& other) const
    {
        auto thisSize = size();
        auto otherSize = other.size();
        SOUL_ASSERT (thisSize != 0);
        SOUL_ASSERT (otherSize != 0);

        if (thisSize > otherSize)
            return false;

        for (size_t i = 0; i < thisSize; ++i)
            if (pathSections[i] != other.pathSections[i + otherSize - thisSize])
                return false;

        return true;
    }

    bool operator== (const IdentifierPath& other) const     { return pathSections == other.pathSections; }
    bool operator== (const Identifier& other) const         { return isUnqualifiedName (other); }
    bool operator!= (const IdentifierPath& other) const     { return pathSections != other.pathSections; }
    bool operator!= (const Identifier& other) const         { return ! isUnqualifiedName (other); }
    bool operator== (const std::string& other) const        { return toString() == other; }
    bool operator!= (const std::string& other) const        { return toString() != other; }

    IdentifierPath operator+ (const IdentifierPath& other) const
    {
        IdentifierPath result (*this);

        for (auto i : other.pathSections)
            result.addSuffix (i);

        return result;
    }

    size_t size() const                 { return pathSections.size(); }
    Identifier getFirstPart() const     { return pathSections.front(); }
    Identifier getLastPart() const      { return pathSections.back(); }

    IdentifierPath fromSecondPart() const
    {
        SOUL_ASSERT (size() > 1);
        IdentifierPath p;

        for (size_t i = 1; i < pathSections.size(); ++i)
            p.pathSections.push_back (pathSections[i]);

        return p;
    }

    IdentifierPath getParentPath() const
    {
        if (size() <= 1)
            return {};

        auto p = *this;
        p.pathSections.pop_back();
        return p;
    }

    void addSuffix (Identifier i)
    {
        SOUL_ASSERT (i.isValid());
        pathSections.push_back (i);
    }

    IdentifierPath withSuffix (Identifier i) const
    {
        SOUL_ASSERT (i.isValid());
        auto p = *this;
        p.pathSections.push_back (i);
        return p;
    }

    void removeFirst (size_t items)
    {
        SOUL_ASSERT (pathSections.size() >= items);

        pathSections.erase (pathSections.begin(), pathSections.begin() + items);
    }

    static IdentifierPath fromString (Identifier::Pool& allocator, std::string fullPath)
    {
        IdentifierPath result;
        TokenisedPathString tokenised (std::move (fullPath));

        for (size_t i = 0; i < tokenised.sections.size(); ++i)
            result.addSuffix (allocator.get (tokenised.getSection(i)));

        return result;
    }

    std::string toString() const
    {
        return choc::text::joinStrings (pathSections, "::");
    }

    ArrayWithPreallocation<Identifier, 8> pathSections;
};


} // namespace soul
