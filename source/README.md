## SOUL Implementation Files

This folder contains a subset of the internal codebase used to implement the SOUL compiler and various playback tools.

The low-level C++ source code for various compiler tasks is arranged as JUCE modules in the `modules` folder:

- `modules/soul_core` - this module contains
    - The front-end parser and compiler needed to compile a SOUL program into its equivalent HEART assembly format
    - Class declarations for abstract classes such as `soul::Venue` and `soul::Performer`
    - Numerous utility functions and classes
 Note that `soul_core` is dependency-free: although it's formatted as a JUCE module, it doesn't require JUCE or any other libraries to build.
- `modules/soul_venue_audioplayer` - this small module is a simple example of an implementation of a `soul::Venue` which uses the standard JUCE audio device classes to directly play the content
- `modules/soul_patch_loader` - this module implements a lot of the glue logic required to turn a `soul::Performer` into a set of COM classes which implement the SOUL Patch API

##### SOUL Patch APIs

- `include/soul/soul_patch.h` - This includes everything needed to use the COM-based classes which can load and run with the SOUL Patch DLL
- `include/soul/patch/helper_classes` - A set of header-only C++ utilities providing various client-side helpers for dealing with the SOUL Patch API. Some of these require JUCE, e.g there's a utility to load a patch as a `juce::AudioPluginInstance`.

### Current Project Status

Please note! This set of source files is far from being a complete snapshot of the entire SOUL codebase! The highly complex logic which transforms a HEART program into something actually playable and runs the LLVM JIT backend is still closed-source for the moment (although we'll be providing binary DLLs with EULAs that allow everyone to freely create SOUL hardware or other playback systems). But as the project moves forward, we'll be constantly revising and adding to the open source repository.