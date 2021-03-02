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

/// Helper classes and functions for various source code parsing tasks
struct SourceCodeUtilities
{
    //==============================================================================
    static CodeLocation findNextOccurrence (CodeLocation start, char character);
    static CodeLocation findEndOfExpression (CodeLocation start);

    static CodeLocation findEndOfMatchingBrace (CodeLocation openBrace);
    static CodeLocation findEndOfMatchingParen (CodeLocation openParen);

    static struct CodeLocationRange findRangeOfASTObject (AST::ASTObject&);

    static std::vector<pool_ref<AST::ASTObject>> findASTObjectsAt (choc::span<pool_ref<AST::ModuleBase>> modulesToSearch,
                                                                   CodeLocation targetLocation);

    //==============================================================================
    struct Comment
    {
        bool valid = false, isStarSlash = false, isDoxygenStyle = false, isReferringBackwards = false;
        std::vector<std::string> lines;
        CodeLocationRange range;
    };

    static CodeLocation findStartOfPrecedingComment (CodeLocation location);
    static Comment parseComment (CodeLocation startOfComment);
    static Comment findPrecedingComment (CodeLocation codeLocation);

    static Comment getFileSummaryComment (CodeLocation startOfFile);
    static std::string getFileSummaryTitle (const Comment&);
    static std::string getFileSummaryBody (const Comment&);

    //==============================================================================
    /// Iterates the tokens in some soul code, returning each text section and
    /// the CSS tag name that it should use (using the highlight.js types)
    static void iterateSyntaxTokens (CodeLocation start,
                                     const std::function<bool(std::string_view text,
                                                              std::string_view type)>& handleToken);

    //==============================================================================
    /// Returns a list of soul keywords and intrinsics which could be used as
    /// suggestions for code-completion
    static std::vector<std::string> getCommonCodeCompletionStrings();
};


} // namespace soul
