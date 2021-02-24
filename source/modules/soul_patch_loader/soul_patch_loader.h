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

  ID:               soul_patch_loader
  vendor:           SOUL
  version:          0.0.1
  name:             SOUL patch hosting helper classes
  description:      SOUL patch hosting helper classes
  website:          https://soul.dev/
  license:          ISC

  dependencies:     soul_core, juce_audio_formats

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#pragma once
#define SOUL_PATCH_LOADER_H_INCLUDED 1

#include <juce_audio_formats/juce_audio_formats.h>

#include "../../../include/soul/soul_patch.h"

/**
    This module contains a set of classes which implement the SOUL Patch
    API interfaces in a way that's suitable for building into a DLL to
    be accessed by the soul::patch::SOULPatchLibrary wrapper class.
*/
namespace soul::patch
{
    /// This function can be used as an implementation for the getSOULPatchLibraryVersion() function
    /// which a patch-loader DLL must export.
    int getLibraryVersion();

    /// This function can be used as an implementation for the createSOULPatchBundle() function
    /// which a patch-loader DLL must export. It just requires a PerformerFactory to provide
    /// some kind of JIT engine back-end.
    PatchInstance* createPatchInstance (std::unique_ptr<soul::PerformerFactory>,
                                        const soul::BuildSettings&,
                                        const char* path);

    /// This function can be used as an implementation for the createSOULPatchBundle() function
    /// which a patch-loader DLL must export. It just requires a PerformerFactory to provide
    /// some kind of JIT engine back-end.
    PatchInstance* createPatchInstance (std::unique_ptr<soul::PerformerFactory>,
                                        const soul::BuildSettings&,
                                        soul::patch::VirtualFile::Ptr);

    /// Creates a VirtualFile instance for a file or URL.
    VirtualFile::Ptr createLocalOrRemoteFile (const char* fileOrURL);

    /// Creates a fake VirtualFile with the given path but the supplied content
    VirtualFile::Ptr createFakeFileWithContent (const char* path, std::string content);
}
