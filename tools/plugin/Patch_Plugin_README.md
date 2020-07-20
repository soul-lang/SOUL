#### Patch Loader Plugin

This folder contains a small JUCE project that uses the `soul::patch::SOULPatchLoaderPlugin` utility class to build a VST/AU native plugin which can be used natively in hosts, and which can dynamically load (and hot re-load) patches.

The resulting plugin lets you drag-and-drop a `.soulpatch` file onto its editor window, which will cause it to try to build and run that SOUL patch. Once loaded, any changes to the source code of that patch should cause the plugin to recompile and reload the patch while running.

#### How to build and use it

You'll need to have JUCE installed. To generate some build projects, just load the SOUL_Plugin.jucer into the Projucer, and export. If your JUCE global paths are set up correctly, it should "just work", and you'll get a project that will build various types of VST, AU, and a standalone test-harness app. You can customise the project in the Projucer if you need to tweak any of its settings.

###### Installing the patch DLL

When you first try to run or load any of the binaries that you build, you'll probably see a window saying that it can't find the SOULPatch DLL.

To get this DLL, you should get the latest version from the [Releases](https://github.com/soul-lang/SOUL/releases/latest) folder.

The project contains some logic for searching for this DLL: it'll look:
- In the folder that contains the plugin or app
- (On Mac) In the resources folder inside the plugin or app bundle
- In a folder called "SOUL" in the user's app data folder (e.g. on Mac this will be `~/Library/Application Support/SOUL`, on Windows, it'll look for `AppData/SOUL`, etc.

These search locations are easy to see and modify in the plugin code if you want to customise it.

#### Caveat!

Unfortunately, the VST/AU/AAX formats were never really designed to allow plugins to dynamically update their name, category, number of buses, number of parameters, or any of the other characteristics which normally remain fixed.

So it's inevitable that there will be all kinds of bizarre edge-case bugs if you load this plugin into a host and start hot-loading patches that change its topology in these ways. Very few of the common DAWs and hosts will have been stress-tested to deal robustly with that kind of behaviour.

Also, the number of combinations of host/format/platform/version/patch is too vast for us to possibly attempt to test or document even a fraction of the possible interactions, so we're hoping that enterprising developers will help us by debugging any problems they hit, especially if it seems like something where there might be a workaround. It'll be impossible to fix all the issues, as many would probably require fixes in the hosts, but we might be able to get quite a decent amount of coverage if enough people play with it.

Although the core JIT engine is inside a DLL, all the code that relates to the plugin APIs is completely open-source, so there should be nothing preventing anyone with the right skill-set debugging any host-specific problems.
