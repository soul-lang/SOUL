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

/** Used with IndentedStream */
struct NewLine final {};
/** Used with IndentedStream */
static const NewLine newLine {};

/** Used with IndentedStream - adds either one or two new-lines, to create a single blank line. */
struct BlankLine final {};
/** Used with IndentedStream - adds either one or two new-lines, to create a single blank line. */
static const BlankLine blankLine {};


//==============================================================================
/** Builds a string with control over the indentation level, to make it easy
    to print code.
*/
struct IndentedStream
{
    IndentedStream();

    /** Uses RAII to create a temporary indent. */
    struct ScopedIndent
    {
        ScopedIndent() = delete;
        ScopedIndent (ScopedIndent&&);
        ScopedIndent (IndentedStream&, int numSpacesToIndent, bool braced);
        ~ScopedIndent();

        IndentedStream& owner;
        int amount;
        bool isBraced;
    };

    /** Returns the finished string */
    std::string toString() const;

    ScopedIndent createIndent();
    ScopedIndent createIndent (int numSpacesToIndent);
    ScopedIndent createBracedIndent();
    ScopedIndent createBracedIndent (int numSpacesToIndent);

    void setTotalIndent (int numSpacesToIndent);
    int getTotalIndent() const noexcept;

    IndentedStream& operator<< (const std::string& text);
    IndentedStream& operator<< (const char* text);
    IndentedStream& operator<< (char character);
    IndentedStream& operator<< (double value);
    IndentedStream& operator<< (float value);
    IndentedStream& operator<< (const NewLine&);
    IndentedStream& operator<< (const BlankLine&);
    IndentedStream& operator<< (size_t value);

    void writeMultipleLines (const std::string& text);
    void writeMultipleLines (const char* text);

    void insertSectionBreak();

private:
    //==============================================================================
    int currentIndent = 0, indentSize = 4;
    bool indentNeeded = false, currentLineIsEmpty = false,
         lastLineWasBlank = false, sectionBreakNeeded = false;
    std::ostringstream content;

    void write (const std::string&);
    void writeLines (ArrayView<std::string>);
    void indent (int amount);
    void writeIndentIfNeeded();
    void printSectionBreakIfNeeded();

    IndentedStream (const IndentedStream&) = delete;
};


} // namespace soul
