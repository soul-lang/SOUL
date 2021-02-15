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

struct OfflineRenderOptions
{
    /// An optional audio filename to read into the processor's input stream.
    /// Note that this is expected to be a full, absolute pathname
    std::string inputFilename;

    /// A (non-optional!) filename where the output will be written.
    /// The file must not already exist, but its parent folder must exist.
    /// Note that this is expected to be a full, absolute pathname
    std::string outputFilename;

    /// These properties are used when opening up the destination audio file.
    /// If there is an input file, then numFrames and sampleRate can be left as 0, and
    /// their values will be copied from those of the input file.
    /// If numChannels is left at 0, then the number of channels will be decided based on
    /// the processor's output endpoint types.
    AudioFileProperties outputFileProperties;
};

/// Compiles a program and synchronously renders the output.
bool offlineRender (OfflineRenderOptions,
                    PerformerFactory&,
                    AudioFileFactory&,
                    Program,
                    CompileMessageList&,
                    const BuildSettings&,
                    std::function<bool(double)> handleProgress);

}
