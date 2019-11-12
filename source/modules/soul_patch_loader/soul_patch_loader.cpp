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

#include "../../API/soul_patch/API/soul_patch.h"
#include "JuceHeader.h"

#include "../../API/soul_patch/helper_classes/soul_patch_Utilities.h"

#include "classes/soul_patch_helpers.h"
#include "classes/soul_patch_FileList.h"
#include "classes/soul_patch_BelaTransformation.h"
#include "classes/soul_patch_PlayerImpl.h"
#include "classes/soul_patch_InstanceImpl.h"
#include "classes/soul_patch_DefaultFile.h"

namespace soul::patch
{
    int getLibraryVersion()
    {
        return soul::patch::currentLibraryAPIVersion;
    }

    PatchInstance* createPatchInstance (std::unique_ptr<soul::PerformerFactory> performerFactory,
                                        soul::patch::VirtualFile* file)
    {
        if (file != nullptr && performerFactory != nullptr)
            return new soul::patch::PatchInstanceImpl (std::move (performerFactory),
                                                       soul::patch::VirtualFile::Ptr (file));

        return {};
    }

    PatchInstance* createPatchInstance (std::unique_ptr<soul::PerformerFactory> performerFactory,
                                        const char* path)
    {
        if (soul::patch::isValidPathString (path))
            return createPatchInstance (std::move (performerFactory),
                                        createLocalOrRemoteFile (juce::CharPointer_UTF8 (path)));

        return {};
    }
}
