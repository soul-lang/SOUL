/*
     _____ _____ _____ __
    |   __|     |  |  |  |
    |__   |  |  |  |  |  |__
    |_____|_____|_____|_____|

    Copyright (c) 2018 - ROLI Ltd.
*/

#include <JuceHeader.h>
#include "../../../include/soul/patch/helper_classes/soul_patch_LoaderPlugin.h"


//==============================================================================
/**
    All the classes that do the actual work here are header-only classes in
    the soul include folder, so this is all we need to do here is include them
    and use them to instantiate a plugin...
*/
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new soul::patch::SOULPatchLoaderPlugin<soul::patch::PatchLibraryDLL> ({});
}
