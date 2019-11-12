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
/**
    Contains the built-in library functions.
*/
static inline CodeLocation getDefaultLibraryCode()
{
    return SourceCodeText::createInternal ("SOUL built-in library",
                                           #include "soul_library_intrinsics.h"
                                           #include "soul_library_trig.h"
                                           );

}

static inline const char* getSystemModuleCode (const std::string& moduleName)
{
    if (moduleName == "soul.audio.utils") return
        #include "soul_library_audio_utils.h"
        ;

    if (moduleName == "soul.midi") return
        #include "soul_library_midi.h"
        ;

    if (moduleName == "soul.notes") return
        #include "soul_library_note_events.h"
        ;

    if (moduleName == "soul.frequency") return
        #include "soul_library_frequency.h"
        ;

    return nullptr;
}

static inline CodeLocation getSystemModule (const std::string& moduleName)
{
    if (auto code = getSystemModuleCode (moduleName))
        return SourceCodeText::createInternal (moduleName, code);

    return {};
}

} // namespace soul
