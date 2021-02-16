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
/// A ref-counted holder for a source code string.
struct SourceCodeText  final : public RefCountedObject
{
    using Ptr = RefCountedPtr<SourceCodeText>;

    static Ptr createForFile (std::string filename, std::string text);
    static Ptr createInternal (std::string name, std::string text);

    const std::string filename, content;
    const choc::text::UTF8Pointer utf8;
    const bool isInternal;

private:
    SourceCodeText() = delete;
    SourceCodeText (const SourceCodeText&) = delete;
    SourceCodeText (std::string, std::string, bool internal);
};


//==============================================================================
/// Represents a source code location as a pointer into a SourceCodeText object.
struct CodeLocation   final
{
    CodeLocation() noexcept = default;
    CodeLocation (SourceCodeText::Ptr);

    CodeLocation (CodeLocation&&) = default;
    CodeLocation (const CodeLocation&) = default;
    CodeLocation& operator= (CodeLocation&&) = default;
    CodeLocation& operator= (const CodeLocation&) = default;

    /// This is the best way to convert a string to a CodeLocation as it'll also
    /// validate the UTF8 and throw an error if it's dodgy.
    static CodeLocation createFromString (std::string filename, std::string text);
    static CodeLocation createFromSourceFile (const SourceFile&);
    void validateUTF8() const;

    void emitMessage (CompileMessage) const;
    [[noreturn]] void throwError (CompileMessage) const;

    bool isEmpty() const;
    std::string getFilename() const;

    size_t getByteOffsetInFile() const;

    struct LineAndColumn
    {
        /// The line and column indexes begin at 1. If they are 0, it
        /// indicates that the object isn't initialised.
        uint32_t line = 0, column = 0;
    };

    LineAndColumn getLineAndColumn() const;

    /// Returns a new location which has the given number of lines and columns
    /// added to this position.
    CodeLocation getOffset (uint32_t linesToAdd, uint32_t columnsToAdd) const;

    /// Returns the start of the current line.
    CodeLocation getStartOfLine() const;
    /// Returns the end of the current line (not including any end-of-line characters).
    CodeLocation getEndOfLine() const;

    /// Returns the start of the next line, or a null location if this is the last one.
    CodeLocation getStartOfNextLine() const;
    /// Returns the start of the previous line, or a null location if this is the first one.
    CodeLocation getStartOfPreviousLine() const;

    /// Returns the content of the current line (not including any end-of-line characters).
    std::string getSourceLine() const;

    /// The original text into which this is a pointer.
    SourceCodeText::Ptr sourceCode;

    /// The raw pointer to the text.
    choc::text::UTF8Pointer location;
};

//==============================================================================
/// Holds a start/end CodeLocation for a lexical range
struct CodeLocationRange
{
    CodeLocation start, end;

    bool isEmpty() const;
    std::string toString() const;
};


} // namespace soul
