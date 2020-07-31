/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include <JuceHeader.h>
#include "../../../include/soul/patch/helper_classes/soul_patch_LoaderPlugin.h"



//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new soul::patch::SOULPatchLoaderPlugin<soul::patch::PatchLibraryDLL> ({});
}
