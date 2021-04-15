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

#ifndef CHOC_FILE_UTILS_HEADER_INCLUDED
#define CHOC_FILE_UTILS_HEADER_INCLUDED

#include <fstream>

namespace choc::file
{

/// A file handling error, thrown by some of the functions in this namespace.
struct Error
{
    std::string description;
};

/// Attempts to load the contents of the given filename into a string,
/// throwing an Error exception if anything goes wrong.
std::string loadFileAsString (const std::string& filename);

/// Attempts to create or overwrite the specified file with some new data.
/// This will attempt to create and parent folders needed for the file, and will
/// throw an Error exception if something goes wrong.
void replaceFileWithContent (const std::string& filename,
                             std::string_view newContent);

/// Iterates the files in a folder, optionally recursing into sub-folders.
/// The iterator function should return true to continue, or false to stop.
void iterateFiles (const std::string& folder,
                   bool recurse,
                   const std::function<bool(std::string_view)>&);


//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================

inline std::string loadFileAsString (const std::string& filename)
{
    if (filename.empty())
        throw Error { "Illegal filename" };

    try
    {
        std::ifstream stream;
        stream.exceptions (std::ofstream::failbit | std::ofstream::badbit);
        stream.open (filename, std::ios::binary | std::ios::ate);

        if (! stream.is_open())
            throw Error { "Failed to open file: " + filename };

        auto fileSize = stream.tellg();

        if (fileSize < 0)
            throw Error { "Failed to read from file: " + filename };

        if (fileSize == 0)
            return {};

        std::string content (static_cast<std::string::size_type> (fileSize),
                             std::string::value_type());
        stream.seekg (0);

        if (stream.read (content.data(), static_cast<std::streamsize> (fileSize)))
            return content;

        throw Error { "Failed to read from file: " + filename };
    }
    catch (const std::ios_base::failure& e)
    {
        throw Error { "Failed to read from file: " + filename + ": " + e.what() };
    }

    return {};
}

inline void replaceFileWithContent (const std::string& filename, std::string_view newContent)
{
    try
    {
        std::ofstream stream;
        stream.exceptions (std::ofstream::failbit | std::ofstream::badbit);
        stream.open (filename, std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
        stream.write (newContent.data(), static_cast<std::streamsize> (newContent.size()));
        return;
    }
    catch (const std::ios_base::failure& e)
    {
        throw Error { "Failed to write to file: " + filename + ": " + e.what() };
    }

    throw Error { "Failed to open file: " + filename };
}

} // namespace choc::file

#endif // CHOC_FILE_UTILS_HEADER_INCLUDED
