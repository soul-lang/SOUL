/*
     _____ _____ _____ __
    |   __|     |  |  |  |
    |__   |  |  |  |  |  |__
    |_____|_____|_____|_____|

    Copyright (c) 2018 - ROLI Ltd.
*/

/*
    This is the header you should add to your project in order to get all the patch-related
    classes from this folder.

    If you're trying to learn your way around this library, the important classes to start
    with are soul::patch::SOULPatchLibrary and soul::patch::PatchInstance.
*/

#pragma once

#include <cstdint>

#if defined (WIN32)
 #if !defined (_WINDOWS_)
  // Since windows.h hasn't been included, we will need to declare these functions
  // If you see an error related to these declarations, it's likely you've included windows.h after including these
  // To resolve, move your windows.h include to the first header in your compilation unit
  typedef void* HMODULE;
  extern "C" __declspec(dllimport) HMODULE __stdcall LoadLibraryA (const char*);
  extern "C" __declspec(dllimport) int     __stdcall FreeLibrary (HMODULE);
  extern "C" __declspec(dllimport) void*   __stdcall GetProcAddress (HMODULE, const char*);
 #endif
#else
 #include <dlfcn.h>
#endif

#include "common/soul_ProgramDefinitions.h"

#pragma pack (push, 1)
#define SOUL_PATCH_MAIN_INCLUDE_FILE 1

#if __clang__
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#endif

#include "patch/soul_patch_ObjectModel.h"
#include "patch/soul_patch_VirtualFile.h"
#include "patch/soul_patch_Player.h"
#include "patch/soul_patch_Instance.h"
#include "patch/soul_patch_Library.h"

#pragma pack (pop)
#undef SOUL_PATCH_MAIN_INCLUDE_FILE

#if __clang__
 #pragma clang diagnostic pop
#endif
