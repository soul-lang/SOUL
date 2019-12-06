### The `soul` command-line tool

This folder contains builds of the command (find it in the `soul_command` folder) lets you compile, play, and render soul code, and generate C++ and HEART code from it!

[Click here to download binaries for various platforms.](https://github.com/soul-lang/SOUL/releases/latest)


```
   _____ _____ _____ __ 
  |   __|     |  |  |  |     SOUL toolkit
  |__   |  |  |  |  |  |__   (C)2019 ROLI Ltd.
  |_____|_____|_____|_____|

  SOUL toolkit version 1.0.687
  Library version 0.8.0
  Build date: Nov 11 2019 12:19:45

Usage:

 soul help [command]                    Shows the list of commands, or gives details about a particular command
 soul version                           Prints the current version number
 soul play [--block-size=<size>] [--rate=<sample-rate>] [--nogui] <file or folder to watch>
                                        Watches for changes to a SOUL file or folder and plays it back, either locally or remotely
 soul render --length=<samples> --output=<target file> [--input=<source audio file>] [--rate=<sample-rate>] <soul file>
                                        Runs a SOUL program, reading and rendering its input and output to audio files
 soul errors <soul file> [--testToRun=N] [--runDisabled]
                                        Does a dry-run of compiling a SOUL file and outputs any errors encountered
 soul generate [--cpp] [--wasm] [--heart] [--graph] [--output=<output file>] <source file>
                                        Compiles a SOUL file and generates equivalent code in another target format such as C++ or WASM
 soul create new_patch_name [--synth] [--effect] [--output=<output folder>]
                                        Creates a patch manifest file and the boilerplate for a processor.
```
### `soul play`

To play some sound, probably the best start is to point it at the example `.soul` or `.soulpatch` files in the examples folder. 

..and then you can either run one like this:

`soul play ~/mystuff/SOUL/examples/ClassicRingtone.soul`

or leave it watching the entire folder for changes like this:

`soul play ~/mystuff/SOUL/examples`

..which means that as soon as you save a change to one of the files in there, it'll re-compile and play it, so you can tinker around with changes to the code. This is how we've been using it for things like our public demos.

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

We'll find a neater way of delivering support for IDEs soon, as well as some SOUL syntax highlighting support.

### `soul generate`

The `generate` function can take a .soul file and translate to C++ or show the HEART intermediate code that is used internally. WASM generation is also possible but currently relies on a suitable external clang executable (this will all become self-contained in a future release!) 
The `--graph` option will also create a chunk of HTML which displays a visual representation of the soul processor's topology.

### `soul create`

This command will generate the necessary boilerplate files for creating a SOUL Patch as either an effect or a synth.

### `soul render`

The render function will compile and render the output of a soul program to an audio file.
