## The SOUL Patch Format

A *SOUL patch* is a bundle of files which contains everything needed to implement an audio synthesiser or effect. It contains no natively executable code - an application is needed to load and play a patch.

#### Comparison to traditional audio plugins

In many ways a patch resembles a traditional audio plugin (e.g. VST/AudioUnit/AAX), but has significant differences too.

Similarities:

- The end-user experience of opening and using patches and plugins in DAWs will be very similar.
- Plugins and patches share similar ways of publishing their parameters, audio i/o bus configuration, MIDI handling, etc.
- Plugins and patches all offer ways to declare an optional GUI editor for the host to display.

Differences:

- Patches contain no native code.
- The DSP and GUI sections of a patch are entirely decoupled - they're written in different languages and can only communicate via streams and events.
- Patches offer more than one option for defining their GUI, ranging from simple declarative faceplate formats to more powerful javascript-based GUIs.
- Patches are JIT-compiled and can be hot-reloaded while running.

A difference in the overall philosophy lies in the distinction between "sounds" and "plugins".

If you were delivering a set of 100 "sounds" using a traditional plugin format, you'd build a single large native plugin (the engine), and then supply a set of 100 programs (a.k.a. "presets", "patches", "sounds", etc) which can be loaded onto that engine.

Because SOUL patches are compact and script-based, there's less reason to structure things like that. A SOUL patch can be treated more like an individual sounds (hence the name "patch" rather than "plugin"). Delivering a set of 100 sounds using SOUL could be done by simply providing 100 separate SOUL patches. Internally they may all share most of the same DSP and GUI codebase (when loaded, the JIT compiler will detect and coalesce any duplicated code). However, the fact that the sounds needn't share *all* their engine code means that individual sounds can be fine-tuned to eliminate wasted processing overhead, or to tweak the underlying DSP for particular sounds.

### File format

A patch is identified by a manifest file with the suffix `.soulpatch` which contains information and links to the set of files and resources that it requires.

The `.soulpatch` file is JSON, and contains details about this particular patch, e.g.

```json
{
  "soulPatchV1": {
    "ID":               "com.mycompany.sounds.awesome_synth_sound_34",
    "version":          "1.0",
    "name":             "Awesome Synth Sound 34",
    "description":      "This is an awesome sound",
    "category":         "synth",
    "manufacturer":     "The Cool Synth Company Inc",
    "icon":             "AwesomeSoundIcon.svg",
    "website":          "https://mycoolsynthcompany.com",
    "isInstrument":     true,

    "source":           [ "AwesomeSound34.soul", "SomeSharedLibraryCode.soul" ],

    "externals": {
      "SampleSet::kickdrum": "sounds/kick1.ogg",
      "SampleSet::hihat":    "sounds/hihat.flac",
      "SampleSet::bass":     "sounds/bass.wav",
      "SampleSet::crash":    "sounds/crash22.wav"
    },

    "view":             [ "AwesomeGUI.js", "AwesomeGUI.faceplate" ]
  }
}
```

The main duties of this file are to provide:

- Enough descriptive details for a host to present to a user who's browsing for patches
- One or more `.soul` source files to build and link (the "source" parameter can be either a single path or an array of paths). One of the processors must be annotated as `[[ main ]]` to tell the host which processor should be loaded and run
- A set of `external` variable bindings if needed by the program.
- An optional list of GUI implementations which a host may be able to display

The folder that contains the manifest file may also contain any resource files that are needed (e.g. audio files, image files for the GUI, etc).

The set of inputs and outputs declared in the main SOUL processor are used by a host to determine the i/o bus layout and set of parameters for the patch. e.g.

```C++
processor MySynth
{
    input event midi::Message midiIn;  // a MIDI input channel

    input event float cutoff    [[ name: "Cutoff",    min: 0, max: 127, init: 80, unit: "semi", step: 0.1 ]];
    input event float resonance [[ name: "Resonance", min: 0, max: 100, init: 20, unit: "%",    step: 1   ]];

    output stream float<2> audioOut; // a stereo audio output
```

The `externals` object in the manifest contains a set of values which need to be provided for external variables in the program (see the SOUL language guide for more details about how to declare an external variable). Each item in the externals list maps the (fully-qualified) name of a SOUL external variable to either a value (primitive or an object or array, etc), or a filename which should be loaded and turned into a float array. The patch loader uses JUCE for reading audio files, so will load WAV, AIFF, FLAC, Ogg-Vorbis and MP3 on all platforms, and maybe other formats on MacOS and Windows where the OS APIs support it.

### Hosting Support and Utilities

To let you create a host that can load and run SOUL patches, we have the following resources:

- A shared library that can be imported into any C++ project (with some dependency-free vanilla C++ classes to interface to it). This provides the functionality to load and play instances of SOUL patches with very high performance using the LLVM JIT engine internally.
- A set of header-only helper classes which make it easy to use this library in JUCE applications. These classes create an abstraction layer over the SOUL patches to present them using the juce::AudioPluginInstance class, which is easy to load into existing JUCE-based apps and plugins.
- The `soul` command-line utility can load and play patches, hot-reloading them when the code is modified.
- The latest versions of Tracktion Waveform provide SOUL patch support, allowing them to be live-recompiled while running inside a session.

The binaries and libraries can be downloaded as github releases: https://github.com/soul-lang/SOUL/releases/latest


### Annotations for parameters

In SOUL, each input declaration can have an arbitrary set of properties ('annotations') attached to it, e.g.

```C++
input event float cutoff  [[ name: "Cut-off", min: 0, max: 127, init: 80, unit: "semi", step: 0.1 ]];
input event float turbo   [[ name: "Turbo Boost", boolean, text: "off|on" ]]
```

If you'd like an input to be treated by a host as a parameter rather than an audio stream, you can attach some of the following annotations to it:

- `name:` The name that a host should display for this parameter. If not specified, the endpoint name is used.
- `unit:` If supplied, this string indicates the units for this value, e.g. `"dB"` or `"%"` or `"Hz"`.
- `min:` The minimum value of the range of a parameter (inclusive). If not specified, the default is 0.
- `max:` The maximum value of the range of a parameter (inclusive). If not specified, the default is 1.
- `step:` A snapping interval to which the value should be quantised. If not specified, the default is 0, which means the value is continuous.
- `init:` The initial, or default value for a parameter. This should be between the `min` and `max` values. If not specified, the default is the same as `min`.
- `boolean:` If this flag is present (either just as `boolean` with no value, or explicitly `boolean: true` or `boolean: false`), it tells the host whether the parameter should be treated as a two-position switch. If so, the host may want to show the user a button rather than a slider. If you set this flag, it's best to not set the `min`, `max` or `step`, and the value will be toggled between 0 and 1 by default.
- `hidden:` If the `hidden` property is present, hosts should ignore this parameter and not show it to the user. (But don't be surprised if some hosts don't respect this flag).
- `automatable:` Tells the host whether to allow this parameter to be automated. If not specified, the default is `true`. (It'd be optimistic to expect all hosts to recognise this flag).
- `text:` This can be used for two main purposes:
  - To give more control over the exact number formatting for displaying the value. The string provided is preprocessed like a very minimal printf, where a few substitutions are made before printing it:
    - `%d` is replaced by an integer version of the parameter value.
    - `%f` is replaced by a floating point display of the value, where number of decimal places can be specified, e.g. `%3f` = "up to 3 decimal places", `%02f` = "exactly 2 decimal places".
    - `%+d` or `%+f` will always print a `+` or `-` sign before the number.
  - Alternatively, the `text` property can be a list of items separated by a vertical pipe character `|`. These will be evenly spread across the range of the parameter, e.g. `text: "low|medium|high", max: 9` will show "low" for values 0 -> 3, "medium" for 3 -> 6, and "high" for 6 -> 9. If you omit a `max` setting, then `max` will set to the number of items - 1. If you omit `step`, it will be set to snap to the size of each item. If you provide a list of items like this, some hosts may choose to display them as a drop-down list rather than using a slider.
- `group:` An optional tag for the host to use to group sets of related parameters together when displaying them to the user. Use forward-slashes to create nested hierarchies of groups.

### API Resources

The SOUL Patch API interfaces and helper classes are found [here](../include/soul/patch).