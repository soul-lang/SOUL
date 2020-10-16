## SOUL Wasm

The soul command line tool supports generating a wasm file from soulpatches. You can use this tool to make wasm which can then be instantiated and used via javascript to perform the patches DSP for either offline processing, or for realtime use via WebAudio.

### Generating Wasm

Using the command line tool, the Wasm generation can be called with:

`soul generate --wasm <path to soulpatch file> --output <output filename>`

When specifying the path, be sure to include the full name to the soulpatch file (e.g. Reberb/Reberb.soulpatch)

### The Wasm Interface

The generated Wasm has the following exported functions that you'll use in the hosting javascript:

| Function                                              | Description                                                                                                                  |
|-------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------|
| `int getDescriptionLength() `                         | Returns the heap offset of the description json                                                                              |
| `int getDescription() `                               | Returns the size of the patch description json                                                                               |
| `int getBufferSize()`                                 | Returns the maximum block size that can be rendered per process call, and hence the size of the input and output data blocks |
| `int getNumInputChannels()`                           | Returns the number of input channels supported by the patch                                                                  |
| `int getNumOutputChannels()`                          | Returns the number of output channels supported by the patch                                                                 |
| `int getOutData(int channel)`                         | Returns the heap offset of the output buffer for the given channel                                                           |
| `int getInData(int channel)`                          | Returns the heap offset of the input buffer for the given channel                                                            |
| `void prepareToPlay (int sampleRate, int sessionId)`  | Initialises the code with the given sample rate and sessionId                                                                |
| `void onParameterUpdate (int parameter, float value)` | Updates the indicated parameter to the given value                                                                           |
| `void setAudioInput (int enabled)`                    | Indicates whether input data is being provided by the host                                                                   |
| `int getMidiBufferLength()`                           | Returns the length of the midi buffer                                                                                        |
| `int getMidiBuffer()`                                 | Returns the heap offset of the midi buffer                                                                                   |
| `void onMidiMessage(int length)`                      | Adds a midi message of the given length                                                                                      |
| `bool processBlock (int blockSize)`                   | Processes the given blockSize of samples from the inputs to the outputs                                                      |

A typical use of the API will proceed as follows:

#### Buses and Parameter inspection

Having instantiated the Wasm, a call to the getDescription() and getDesctiptionLength() will yield the location of a UTF-8 string in the heap, containing a JSON description of the loaded patch. Using the Reverb patch as an example (https://soul.dev/lab/?id=Reverb) the exported JSON looks as follows:


```json
  "description": {
    "UID"           : "dev.soul.examples.reverb",
    "version"       : "1.0",
    "name"          : "Reverb",
    "description"   : "SOUL Reverb",
    "category"      : "fx",
    "manufacturer"  : "soul.dev",
    "URL"           : "",
    "isInstrument"  : "false"
  },
  "inputBuses": [
    {
      "index"       : 0,
      "name"        : "audioIn",
      "numChannels" : 1
    }
  ],
  "outputBuses": [
    {
      "index"       : 0,
      "name"        : "audioOut",
      "numChannels" : 2
    }
  ],
  "parameters": [
    {
      "index"        : 1,
      "id"           : "roomSize",
      "name"         : "Room Size",
      "unit"         : "",
      "group"        : "",
      "text"         : "tiny|small|medium|large|hall",
      "minValue"     : 0.0,
      "maxValue"     : 100.0,
      "step"         : 25.0,
      "initialValue" : 80.0,
      "properties"   : { "min": 0, "max": 100, "init": 80, "name": "Room Size", "text": "tiny|small|medium|large|hall" }
    },
    {
      "index"        : 2,
      "id"           : "damping",
      "name"         : "Damping Factor",
      "unit"         : "%",
      "group"        : "",
      "text"         : "",
      "minValue"     : 0.0,
      "maxValue"     : 100.0,
      "step"         : 1.0,
      "initialValue" : 50.0,
      "properties"   : { "min": 0, "max": 100, "init": 50, "name": "Damping Factor", "unit": "%", "step": 1 }
    },
    {
      "index"        : 3,
      "id"           : "wetLevel",
      "name"         : "Wet Level",
      "unit"         : "%",
      "group"        : "",
      "text"         : "",
      "minValue"     : 0.0,
      "maxValue"     : 100.0,
      "step"         : 1.0,
      "initialValue" : 33.0,
      "properties"   : { "min": 0, "max": 100, "init": 33, "name": "Wet Level", "unit": "%", "step": 1 }
    },
    {
      "index"        : 4,
      "id"           : "dryLevel",
      "name"         : "Dry Level",
      "unit"         : "%",
      "group"        : "",
      "text"         : "",
      "minValue"     : 0.0,
      "maxValue"     : 100.0,
      "step"         : 1.0,
      "initialValue" : 40.0,
      "properties"   : { "min": 0, "max": 100, "init": 40, "name": "Dry Level", "unit": "%", "step": 1 }
    },
    {
      "index"        : 5,
      "id"           : "width",
      "name"         : "Width",
      "unit"         : "%",
      "group"        : "",
      "text"         : "",
      "minValue"     : 0.0,
      "maxValue"     : 100.0,
      "step"         : 1.0,
      "initialValue" : 100.0,
      "properties"   : { "min": 0, "max": 100, "init": 100, "name": "Width", "unit": "%", "step": 1 }
    }
  ]
}
```

This JSON describes the patch, the inputs and outputs, and the parameters which are exposed. The input and output channels provided by the Wasm code use de-interleaved streams, so the input and output busses indicate an index for the first channel corresponding to their allocated buffers. For example, if a patch has two inputs, the first one float<2> and the second float<3> then you'd expect to see the inputs described as:

```json
  "inputBuses": [
    {
      "index"       : 0,
      "name"        : "audioIn1",
      "numChannels" : 2
    },
    {
      "index"       : 2,
      "name"        : "audioIn2",
      "numChannels" : 3
    }
  ]
```

From this description you can see that we have two busses, with 5 channels in total (so calling `getNumInputChannels()` will return 5). `audioIn1` has an index of 0, so it's two channels will occupy [0] and [1] in the input channel data. `audioIn2` has an index of 2, so it's three channels will occupy [2],[3] and [4] in the input array. Calls to `getInData()` with the corresponding indexes will return the offsets of these buffers in the heap.

Parameters are somewhat simpler - each parameter's index is included in the JSON, including min and max values. Let's say we want to set the `width` parameter to it's maximum value (100.0) then we'd call `onParameterUpdate (5, 100.0)` as it's index was given as 5. Attempting to set the parameter outside the valid range will cause unexpected results. Expect future releases to enforce the range, and apply parameter quantisation.

#### Initialisation

To initialise the runtime state, the following must be performed:

1) A call to `prepareToPlay()` must be made specifying the sample rate and sessionId. The sessionId can be read by the SOUL code and used as a random seed, so ideally it should be a random integer.
2) Initial parameters need to be set - you are required to call `onParameterUpdate()` to set each parameter to the specified initial values in the exported JSON.

#### Producing output

Per block, the logic to be followed is as follows:

1) Apply any parameter updates - calls to `onParameterUpdate()` will apply the given parameter change for this block. If the parameter is a sparse stream interpolation will be automatically applied to smooth the parameter change across samples.
2) Submit any midi data - `getMidiBuffer()` and `getMidiBufferLength()` can be used to find the offset of the midi buffer, and midi bytes are written to this buffer. At present only simple messages can be written (up to 3 bytes). Having populated the buffer, calling `onMidiMessage()` with the populated message length will cause the message to be processed by the patch. Multiple messages can be submitted by repeating this process, updating the buffer and calling `onMidiMessage()` for each message.
3) Fill the input buffers - having obtained the offsets of the data buffers, these can be populated with input data.
4) Call `processBlock()` with the block size. This will cause the DSP to be run, and the output buffers will be filled with the given number of samples of output data.
5) Read the output data - using the offsets of the output data buffers, the output data can be read.

Each call to `processBlock()` can be up to the `getBufferSize()` block size. If larger blocks are required, or sub-block updates to parameters or midi messages applied, then multiple calls with smaller block sizes should be made to the above API in order to move time to the right moment. Parameter and midi updates are applied at immediately, with no ability to defer them to a later sample, so dividing blocks is used to achieve this.