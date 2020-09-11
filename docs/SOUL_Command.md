### The `soul` command-line tool

The `soul` command-line utility lets you compile, play, render, and generate C++ and HEART code.

[Click here to download binaries for various platforms.](https://github.com/soul-lang/SOUL/releases/latest)


```
   _____ _____ _____ __
  |   __|     |  |  |  |     SOUL toolkit
  |__   |  |  |  |  |  |__   (C)2020 ROLI Ltd.
  |_____|_____|_____|_____|

  SOUL version 0.9.49
  Build date: Sep 10 2020 09:32:01

Usage:

 soul help [command]                    Shows the list of commands, or gives details about a particular command
 soul version                           Prints the current version number
 soul play [--block-size=<size>] [--rate=<sample-rate>] [--inputs=<channels>] [--outputs=<channels>] [--audio-device-type=<type>] [--input-device=<name>] [--output-device=<name>] [--list-device-types] [--list-devices] [--nogui] [--nopatch] [--libraryDir=<path>] <Patch file to play>
                                        Plays a SOUL patch
 soul render --length=<samples> --output=<target file> [--input=<source audio file>] [--rate=<sample-rate>] [--libraryDir=<path>] <soul file>
                                        Runs a SOUL program, reading and rendering its input and output to audio files
 soul errors <soul file> [--testToRun=N] [--libraryDir=<path>] [--runDisabled]
                                        Does a dry-run of compiling a SOUL file and outputs any errors encountered
 soul generate [--cpp] [--wasm] [--heart] [--juce] [--juce-header] [--juce-cpp] [--graph] [--raw] [--output=<output file/folder>] <soul file>
                                        Compiles a SOUL file and generates equivalent code in another target format such as C++ or WASM. The --juce option will create a folder containing a complete JUCE project which implements a patch as pure C++. The --juce-header and --juce-cpp options let you selectively create the C++ declarations and definitions for a juce plugin class.
 soul create new_patch_name [--synth] [--effect] [--output=<output folder>]
                                        Creates a patch manifest file and the boilerplate for a processor.
```
### `soul play`

To play some sound, probably the best start is to point it at the example `.soulpatch` files in the examples folder.

..and then you can run one like this:

`soul play ~/mystuff/SOUL/examples/patches/ClassicRingtone.soulpatch`

While running, it will monitor all the patch files, and if any are modified, it will re-compile and re-load the patch.

Note that it can be usd to compile and play just a specific `.soul` file on its own, rather than as part of a `.soulpatch` bundle, but to do so you must add the `--nopatch` argument to the command. This avoids confusion when people see a folder containing both `.soulpatch` and `.soul` files, and get unexpected results when they try to play one of the `.soul` files which were intended only to be used as part of a patch.

The `play` command can take quite a few other arguments to set things like the audio device, sample rate, and channels needed.

### `soul generate`

The `generate` function can take a `.soul` or `.soulpatch` file and translate to C++ or show the HEART intermediate code that is used internally.

The `--cpp` options generates a dependency-free C++ class which implements the core rendering functionality of a SOUL patch. It has methods that can be used to read and write to its input and output endpoints at a low-level, and also provides higher-level audio/MIDI rendering functions that can process audio in a more familiar "processor" style.

The `--juce` option will create a folder (set its location with `--output=<path>`) containing a ready-to-go JUCE C++ project which can be used to build native VST/AU/AAX plugins. You should be able to simply open the generated `.jucer` file using the Projucer, and build/customise it as you would a normal JUCE plugin project.

The `--wasm` option will emit a chunk of WASM that implements the given patch.

The `--graph` option will also create a chunk of HTML which displays a visual representation of the soul processor's topology.

### `soul errors`

If you use the `soul errors` command, it'll do a dry-run compilation of a file and spit out any errors, so you can use this in an IDE to get it to report errors and jump to the offending line.

We've been using this in Atom and VScode.

A VSCode `tasks.json` file might look like this:

```
{
    // See https://go.microsoft.com/fwlink/?LinkId=733558
    // for the documentation about the tasks.json format
    "version": "2.0.0",
    "tasks": [
        {
            "label": "build soul",
            "type": "shell",
            "command": "soul errors ${file}",
            "presentation": {
                "reveal": "always",
                "panel": "new"
            },
            "problemMatcher": {
                "owner": "soul",
                "fileLocation": [
                    "absolute"
                ],
                "pattern": [
                    {
                        "regexp": "^(.*):(\\d+):(\\d+):\\s+(warning|error):\\s+(.*)$",
                        "file": 1,
                        "line": 2,
                        "column": 3,
                        "severity": 4,
                        "message": 5
                    }
                ]
            },
            "group": {
                "kind": "build",
                "isDefault": true
            }
        }
    ]
}
```

In Atom, here's a simple `.atom-build.yaml`

```
cmd: "soul errors {FILE_ACTIVE}"
name: SOUL
errorMatch: (?<file>[\\/0-9a-zA-Z-\\._]+):(?<line>\d+):(?<col>\d+):\s*error:\s*(?<message>.+)
```

##### Running unit tests

The `soul errors` command can also be given a `.soultest` unit test file to run - see the [language document](SOUL_Language.md) for details on the format of these files.

### `soul create`

This command will generate the necessary boilerplate files for creating a SOUL Patch as either an effect or a synth.

### `soul render`

The render function will compile and render the output of a soul program to an audio file.
