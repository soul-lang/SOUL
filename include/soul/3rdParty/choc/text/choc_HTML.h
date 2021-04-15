//
//    ██████ ██   ██  ██████   ██████
//   ██      ██   ██ ██    ██ ██            ** Clean Header-Only Classes **
//   ██      ███████ ██    ██ ██
//   ██      ██   ██ ██    ██ ██           https://github.com/Tracktion/choc
//    ██████ ██   ██  ██████   ██████
//
//   CHOC is (C)2021 Tracktion Corporation, and is offered under the terms of the ISC license:
//
//   Permission to use, copy, modify, and/or distribute this software for any purpose with or
//   without fee is hereby granted, provided that the above copyright notice and this permission
//   notice appear in all copies. THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
//   WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
//   AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
//   CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
//   WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
//   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#ifndef CHOC_HTML_HEADER_INCLUDED
#define CHOC_HTML_HEADER_INCLUDED

#include <sstream>
#include <string_view>

namespace choc::html
{

//==============================================================================
/**
    A very minimal helper class for building trees of HTML elements which can
    then be printed as text.
*/
struct HTMLElement
{
    HTMLElement() = default;
    explicit HTMLElement (std::string elementName) : name (std::move (elementName)) {}

    /** Creates, adds and returns a reference to a new child element inside this one. */
    HTMLElement& addChild (std::string elementName)
    {
        children.emplace_back (std::move (elementName));
        return children.back();
    }

    /** Appends and returns a given child element to this one. */
    HTMLElement& addChild (HTMLElement&& childToAdd)
    {
        children.emplace_back (std::move (childToAdd));
        return children.back();
    }

    /** Adds and returns a 'a' element with the given href property. */
    HTMLElement& addLink (std::string_view linkURL)     { return addChild ("a").setProperty ("href", linkURL); }
    /** Adds and returns a 'div' element. */
    HTMLElement& addDiv()                               { return addChild ("div"); }
    /** Adds and returns a 'div' element with a given class name. */
    HTMLElement& addDiv (std::string_view className)    { return addDiv().setClass (className); }
    /** Adds and returns a 'p' element. */
    HTMLElement& addParagraph()                         { return addChild ("p").setInline (true); }
    /** Adds and returns a 'span' element with the given class property. */
    HTMLElement& addSpan (std::string_view classToUse)  { return addChild ("span").setInline (true).setClass (classToUse); }

    /** Adds a property for this element. */
    HTMLElement& setProperty (const std::string& propertyName, std::string_view value)
    {
        properties.push_back (propertyName + "=\"" + escapeHTMLString (value, true) + '"');
        return *this;
    }

    /** Sets the 'id' property of this element. */
    HTMLElement& setID (std::string_view value)         { return setProperty ("id", std::move (value)); }
    /** Sets the 'class' property of this element. */
    HTMLElement& setClass (std::string_view value)      { return setProperty ("class", std::move (value)); }

    /** Appends a content element to this element's list of children.
        Note that this returns the parent object, not the new child, to allow chaining.
    */
    HTMLElement& addContent (std::string_view text)     { return addRawContent (escapeHTMLString (text, false)); }
    /** Appends a 'br' element to this element's content. */
    HTMLElement& addLineBreak()                         { return addRawContent ("<br>"); }
    /** Appends an &nbsp; to this element's content. */
    HTMLElement& addNBSP (size_t number = 1)            { std::string s; for (size_t i = 0; i < number; ++i) s += "&nbsp;"; return addRawContent (std::move (s)); }

    /** Sets the element to be "inline", which means that it won't add any space or newlines
        between its child elements. For things like spans or 'p' elements, you probably want them
        to be inline.
    */
    HTMLElement& setInline (bool shouldBeInline)        { contentIsInline = shouldBeInline; return *this; }

    /** Returns a text version of this element. */
    std::string toDocument (bool includeHeader) const
    {
        std::ostringstream out;
        writeToStream (out, includeHeader);
        return out.str();
    }

    /** Writes this element to some kind of stream object. */
    template <typename Output>
    void writeToStream (Output& out, bool includeHeader) const
    {
        if (includeHeader)
            out << "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">" << std::endl;

        print (out, 0, { true, false });
    }

    const std::vector<HTMLElement>& getChildren() const         { return children; }
    std::vector<HTMLElement>& getChildren()                     { return children; }

private:
    std::string name;
    bool isContent = false, contentIsInline = false;
    std::vector<std::string> properties;
    std::vector<HTMLElement> children;

    HTMLElement& addRawContent (std::string text)
    {
        auto& child = addChild (std::move (text));
        child.isContent = true;
        return *this;
    }

    struct PrintStatus
    {
        bool isAtStartOfLine, isFollowingContent;
    };

    template <typename Output>
    PrintStatus print (Output& out, size_t indent, PrintStatus status) const
    {
        if (! (status.isAtStartOfLine || status.isFollowingContent))
        {
            if (! contentIsInline)
                out << '\n';

            status.isAtStartOfLine = true;
        }

        bool openTagIndented = status.isAtStartOfLine && ! contentIsInline;

        if (openTagIndented)
            out << std::string (indent, ' ');

        status.isAtStartOfLine = false;
        out << '<' << name;

        for (auto& p : properties)
            out << ' ' << p;

        out << '>';

        status.isFollowingContent = false;

        for (auto& c : children)
        {
            if (c.isContent)
            {
                out << c.name;
                status.isFollowingContent = true;
            }
            else
            {
                status = c.print (out, indent + 1, status);
            }
        }

        if (openTagIndented && ! (children.empty() || status.isFollowingContent))
            out << "\n" << std::string (indent, ' ');

        out << "</" << name << ">";

        status.isFollowingContent = false;
        return status;
    }

    static bool isCharLegal (uint32_t c)
    {
        return (c >= 'a' && c <= 'z')
            || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9')
            || (c < 127 && std::string_view (" .,;:-()_+=?!$#@[]/|*%~{}\\").find ((char) c) != std::string_view::npos);
    }

    static std::string escapeHTMLString (std::string_view text, bool escapeNewLines)
    {
        std::string result;

        for (auto character : text)
        {
            auto unicodeChar = static_cast<uint32_t> (character);

            if (isCharLegal (unicodeChar))
            {
                result += character;
            }
            else
            {
                switch (unicodeChar)
                {
                    case '<':   result += "&lt;";   break;
                    case '>':   result += "&gt;";   break;
                    case '&':   result += "&amp;";  break;
                    case '"':   result += "&quot;"; break;

                    default:
                        if (! escapeNewLines && (unicodeChar == '\n' || unicodeChar == '\r'))
                            result += character;
                        else
                            result += "&#" + std::to_string (unicodeChar) + ';';

                        break;
                }
            }
        }

        return result;
    }
};

} // namespace choc::html

#endif // CHOC_HTML_HEADER_INCLUDED
