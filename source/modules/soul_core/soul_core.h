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

/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:               soul_core
  vendor:           SOUL
  version:          0.0.1
  name:             Core SOUL classes
  description:      Fundamental SOUL classes for compiling SOUL code into a Program object
  website:          https://soul.dev/
  license:          ISC

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/


#pragma once
#define SOUL_CORE_H_INCLUDED 1

#if defined (__arm64__) || defined (__aarch64__)
 #define SOUL_ARM64 1
#elif (defined (__arm__) || __ARM_NEON__)
 #define SOUL_ARM32 1
#elif __wasm__
 #define SOUL_WASM 1
#else
 #define SOUL_INTEL 1
#endif

#include <vector>
#include <string>
#include <sstream>
#include <array>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <mutex>
#include <memory>
#include <cstring>
#include <cmath>
#include <cstddef>
#include <atomic>
#include <limits>
#include <condition_variable>
#include <cassert>
#include <random>
#include <optional>
#include <complex>

#include "utilities/soul_DebugUtilities.h"

#define CHOC_ASSERT(x)  SOUL_ASSERT(x)

#include "../../../include/soul/3rdParty/choc/containers/choc_Span.h"
#include "../../../include/soul/3rdParty/choc/containers/choc_Value.h"
#include "../../../include/soul/3rdParty/choc/text/choc_StringUtilities.h"
#include "../../../include/soul/3rdParty/choc/text/choc_JSON.h"
#include "../../../include/soul/3rdParty/choc/math/choc_MathHelpers.h"
#include "../../../include/soul/3rdParty/choc/audio/choc_SampleBuffers.h"
#include "../../../include/soul/3rdParty/choc/text/choc_CodePrinter.h"
#include "../../../include/soul/3rdParty/choc/containers/choc_DirtyList.h"
#include "../../../include/soul/3rdParty/choc/containers/choc_VariableSizeFIFO.h"
#include "../../../include/soul/3rdParty/choc/containers/choc_PoolAllocator.h"
#include "../../../include/soul/common/soul_ProgramDefinitions.h"
#include "../../../include/soul/common/soul_DumpConstant.h"

#include "utilities/soul_MiscUtilities.h"

//==============================================================================
namespace soul
{
    static inline constexpr Version getLibraryVersion()                   { return { 1, 0, 0 }; }
    static inline constexpr int64_t getHEARTFormatVersion()               { return 1; }
    static inline constexpr const char* getHEARTFormatVersionPrefix()     { return "SOUL"; }

    struct Identifier;
    struct IdentifierPath;
    struct Annotation;
    struct Value;
    struct ValuePrinter;
    struct CompileMessage;
    class Structure;
    class Performer;
}

//==============================================================================
#include "utilities/soul_ContainerUtilities.h"
#include "utilities/soul_RefCountedObject.h"
#include "utilities/soul_ArrayWithPreallocation.h"
#include "utilities/soul_StringUtilities.h"
#include "utilities/soul_Identifier.h"
#include "utilities/soul_ObjectHandleList.h"
#include "utilities/soul_PoolAllocator.h"
#include "utilities/soul_Resampler.h"
#include "utilities/soul_AccessCount.h"

#include "diagnostics/soul_Logging.h"
#include "diagnostics/soul_Timing.h"
#include "diagnostics/soul_CodeLocation.h"
#include "diagnostics/soul_CompileMessageList.h"
#include "diagnostics/soul_Errors.h"

#include "types/soul_PrimitiveType.h"
#include "types/soul_Type.h"
#include "types/soul_Struct.h"
#include "types/soul_StringDictionary.h"
#include "types/soul_ConstantTable.h"
#include "types/soul_Value.h"
#include "types/soul_Annotation.h"
#include "types/soul_TypeRules.h"
#include "types/soul_EndpointType.h"
#include "types/soul_InterpolationType.h"

#include "heart/soul_Operators.h"
#include "heart/soul_Intrinsics.h"
#include "heart/soul_heart_AST.h"
#include "heart/soul_Program.h"
#include "heart/soul_Module.h"
#include "heart/soul_heart_Checker.h"
#include "heart/soul_heart_Utilities.h"
#include "heart/soul_heart_FunctionBuilder.h"
#include "heart/soul_heart_CallFlowGraph.h"
#include "heart/soul_heart_Optimisations.h"
#include "heart/soul_heart_DelayCompensation.h"
#include "heart/soul_heart_FunctionNames.h"

#include "compiler/soul_AST.h"
#include "compiler/soul_Compiler.h"

#include "venue/soul_Endpoints.h"
#include "venue/soul_Performer.h"
#include "venue/soul_Venue.h"
#include "venue/soul_RenderingVenue.h"

#include "utilities/soul_MultiEndpointFIFO.h"
#include "utilities/soul_AudioDataGeneration.h"
#include "utilities/soul_AudioMIDIWrapper.h"
#include "utilities/soul_AudioFiles.h"

#include "test/soul_TestFileParser.h"

#include "documentation/soul_SourceCodeUtilities.h"
#include "documentation/soul_SourceCodeOperations.h"
#include "documentation/soul_SourceCodeModel.h"
#include "documentation/soul_HTMLGeneration.h"

#include "code_generation/soul_CPPGenerator.h"
#include "code_generation/soul_PatchGenerator.h"
