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

#ifdef SOUL_CORE_H_INCLUDED
 /* When you add this cpp file to your project, you mustn't include it in a file where you've
    already included any other headers - just put it inside a file on its own, possibly with your config
    flags preceding it, but don't include anything else. That also includes avoiding any automatic prefix
    header files that the compiler may be using.
 */
 #error "Incorrect use of SOUL cpp file"
#endif

#include <thread>
#include <iomanip>
#include <fstream>
#include <cctype>
#include <cwctype>
#include <future>

#include "soul_core.h"

#include "../../../include/soul/3rdParty/choc/text/choc_JSON.h"
#include "../../../include/soul/3rdParty/choc/text/choc_HTML.h"
#include "../../../include/soul/3rdParty/choc/audio/choc_Oscillators.h"

#include "../../../include/soul/patch/helper_classes/soul_patch_Utilities.h"

#if SOUL_INTEL
 #include <xmmintrin.h>
#endif

#ifdef __APPLE__
 #include <AvailabilityMacros.h>
#endif

#define SOUL_INSIDE_CORE_CPP 1
#define printf NO_PRINTFS_TODAY_THANKYOU

#ifdef __clang__
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Wswitch-enum"
#elif WIN32
#else
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wswitch-enum"
#endif

#include "utilities/soul_Tokeniser.h"
#include "utilities/soul_StringUtilities.cpp"
#include "utilities/soul_MiscUtilities.cpp"
#include "utilities/soul_AudioDataGeneration.cpp"
#include "utilities/soul_AudioFiles.cpp"
#include "types/soul_Struct.cpp"
#include "types/soul_StringDictionary.cpp"
#include "types/soul_ConstantTable.cpp"
#include "types/soul_Value.cpp"
#include "types/soul_Annotation.cpp"
#include "compiler/soul_ASTUtilities.h"
#include "types/soul_EndpointType.cpp"
#include "heart/soul_heart_Printer.h"
#include "heart/soul_heart_Parser.h"
#include "heart/soul_heart_Checker.cpp"
#include "types/soul_Type.cpp"
#include "compiler/soul_StandardLibrary.h"
#include "compiler/soul_ASTVisitor.h"
#include "compiler/soul_SanityCheckPass.h"
#include "compiler/soul_Parser.h"
#include "compiler/soul_ResolutionPass.h"
#include "compiler/soul_ConvertComplexPass.h"
#include "compiler/soul_HeartGenerator.h"
#include "compiler/soul_Compiler.cpp"
#include "heart/soul_Intrinsics.cpp"
#include "heart/soul_heart_FunctionBuilder.cpp"
#include "heart/soul_ModuleCloner.h"
#include "heart/soul_Module.cpp"
#include "heart/soul_Program.cpp"
#include "venue/soul_RenderingVenue.cpp"
#include "diagnostics/soul_CodeLocation.cpp"
#include "diagnostics/soul_Logging.cpp"
#include "diagnostics/soul_CompileMessageList.cpp"
#include "diagnostics/soul_Timing.cpp"
#include "venue/soul_Endpoints.cpp"
#include "test/soul_TestFileParser.cpp"

#include "documentation/soul_SourceCodeUtilities.cpp"
#include "documentation/soul_SourceCodeOperations.cpp"
#include "documentation/soul_SourceCodeModel.cpp"
#include "documentation/soul_HTMLGeneration.cpp"

#include "code_generation/soul_CPPGenerator_resources.h"
#include "code_generation/soul_CPPGenerator.cpp"
#include "code_generation/soul_JUCEProjectGenerator.cpp"
#include "code_generation/soul_PatchGenerator.cpp"

#ifdef __clang__
 #pragma clang diagnostic pop
#elif WIN32
#else
 #pragma GCC diagnostic pop
#endif
