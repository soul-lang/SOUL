# SOUL Language Guide

SOUL uses a familiar syntax which shares a lot of syntax and style with many mainstream languages (e.g. C/C++, Java, Javascript, C#, etc). This document covers the basic structure and syntax of the code that is passed into the SOUL compiler.

The intended audience for this guide is people who've at least dabbled with some simple programming using a procedural language.

If nothing in this document seems particularly surprising or unusual, then we've succeeded in our goals for making the language accessible.

<!-- @import "[TOC]" {cmd="toc" depthFrom=2 depthTo=6 orderedList=false} -->

<!-- code_chunk_output -->

- [Code format](#code-format)
    - [Whitespace](#whitespace)
    - [Comments](#comments)
    - [Identifiers](#identifiers)
    - [Semicolons](#semicolons)
    - [Reserved keywords](#reserved-keywords)
- [Types](#types)
    - [Primitive types](#primitive-types)
    - [Numeric literals](#numeric-literals)
    - [String literals](#string-literals)
    - [Structures](#structures)
    - [Vector types](#vector-types)
    - [Array types](#array-types)
    - [Array slicing](#array-slicing)
    - [Dynamic array slices](#dynamic-array-slices)
    - ['External' variables](#external-variables)
      - [Special support for audio data in externals](#special-support-for-audio-data-in-externals)
      - [External variable annotations](#external-variable-annotations)
    - [Type aliases](#type-aliases)
    - [Variables](#variables)
- [Program structure](#program-structure)
    - [Namespace declarations](#namespace-declarations)
    - [Function declarations](#function-declarations)
    - [Processor declarations](#processor-declarations)
      - [The `run()` function](#the-run-function)
      - [The `advance()` function](#the-advance-function)
      - [The `init()` function](#the-init-function)
      - [Processor state variables](#processor-state-variables)
    - [Input/Output Endpoint declarations](#inputoutput-endpoint-declarations)
      - [Exposing child endpoints](#exposing-child-endpoints)
    - [Endpoint Annotations](#endpoint-annotations)
    - [Graph declarations](#graph-declarations)
      - [Processor instance declarations](#processor-instance-declarations)
      - [Connection declarations](#connection-declarations)
        - [Default endpoints and connection chaining](#default-endpoints-and-connection-chaining)
        - [Fan-in/out](#fan-inout)
      - [Delay Lines](#delay-lines)
      - [Processor delay compensation (a.k.a. PDC)](#processor-delay-compensation-aka-pdc)
      - [Feedback loops](#feedback-loops)
      - [Arrays of Processors](#arrays-of-processors)
    - [Processor/Graph specialisation parameters](#processorgraph-specialisation-parameters)
    - [Event endpoints](#event-endpoints)
    - [Processor oversampling and undersampling](#processor-oversampling-and-undersampling)
      - [Choosing an oversampling and undersampling strategy](#choosing-an-oversampling-and-undersampling-strategy)
  - [Functions](#functions)
    - [Universal function call syntax](#universal-function-call-syntax)
  - [Generic Functions](#generic-functions)
  - [Metafunctions for type manipulation](#metafunctions-for-type-manipulation)
  - [static_assert](#static_assert)
  - [Compile-time `if`](#compile-time-if)
  - [References](#references)
  - [Casts and Numeric Conversions](#casts-and-numeric-conversions)
  - [Arithmetic operators](#arithmetic-operators)
  - [Conditionals](#conditionals)
  - [Loops](#loops)
  - [Reading and writing to endpoints](#reading-and-writing-to-endpoints)
  - [Annotations](#annotations)
- [Linking and resolving modules](#linking-and-resolving-modules)
    - [Specifying the 'main' processor](#specifying-the-main-processor)
  - [Built-in intrinsic functions](#built-in-intrinsic-functions)
      - [Arithmetic](#arithmetic)
      - [Comparison and ranges](#comparison-and-ranges)
      - [Trigonometry](#trigonometry)
    - [Vector intrinsics](#vector-intrinsics)
  - [Built-in constants](#built-in-constants)
      - [Global constants](#global-constants)
      - [Processor constants](#processor-constants)
  - [Built-in library Functions](#built-in-library-functions)
  - [HEART](#heart)
- [Creating unit-tests with the `.soultest` file format](#creating-unit-tests-with-the-soultest-file-format)
    - [`## compile`](#compile)
    - [`## function`](#function)
    - [`## error <error message>`](#error-error-message)
    - [`## processor`](#processor)
    - [`## global`](#global)
    - [`## console`](#console)
    - [`## disabled`](#disabled)

<!-- /code_chunk_output -->

## Code format

The SOUL parser expects programs to be encoded as UTF-8 text.

#### Whitespace

All whitespace (tabs, spaces, linefeeds, carriage-returns) is treated equally. Other than being used to delimit tokens, the size and layout of whitespace blocks is ignored.

It's your prerogative to choose how to indent and format your code, but the official SOUL style for our library and example code is to indent using multiples of 4 spaces (not tabs!), and to use the Allman style of brace layout.

#### Comments
SOUL uses the familiar C++/Java/Javascript/C# style, i.e.:
```C++
// Single-line Comment

/* Multi-line
   comment */
```

#### Identifiers

An identifier name (e.g. the name of a type, variable, structure, processor, or graph) may only contain the following ASCII characters: `A-Z` `a-z` `0-9` `_`.

The first character must be a letter. Leading underscores are reserved for use in system code, and trailing underscores are discouraged on the grounds that they're ugly :)

It's up to you how you choose to name your objects, but the official SOUL style guide encourages:

- `UpperCamelCase` for naming *types*, e.g. graphs, processors and structures.
- `lowerCamelCase` for naming *variables*, both local and global ones.

We recommend choosing meaningful, self-explanatory names rather than adding annotations like prefixes (e.g. 'g' for globals) or Hungarian notation.

#### Semicolons

SOUL statements are terminated with a semicolon. In future versions of the compiler, we may decide to allow the semicolon to be omitted at the end of lines, but are keeping things conservative initially by requiring it in all cases.

#### Reserved keywords

Some identifiers are reserved for use as keywords, and can't be used as identifiers (e.g variable, namespace, processor or type names).

Currently these are: `if, else, do, while, for, loop, break, continue, const, return, let, var, void, int, int32, int64,
float, float32, float64, bool, clamp, wrap, true, false, struct, string, using, external, graph, processor, namespace, input, output, connection, event`

In addition, there are some reserved words which have been set aside for possible use with future language features. These are: `fixed, import, try, catch, throw`

## Types

#### Primitive types

The primitive types currently supported are:
- `int` - fastest integer type on the platform, either `int32` or `int64`
- `int32` - 32-bit int type
- `int64` - 64-bit int type
- `bool` - boolean type (either true or false)
- `float32` - 32-bit float type
- `float64` - 64-bit float type
- `float` - fastest floating-point type that provides at least 32-bit precision
- `complex32` - 32-bit complex type
- `complex64` - 64-bit complex type
- `complex` - fastest complex type that provides at least 32-bit precision
- `wrap<size>` - a range-constrained `int32` type. The `size` value must be a compile-time constant, and operations on this type are guaranteed to result in values where 0 <= x < size. If incrementing or decrementing it, out-of-range values will wrap around.
- `clamp<size>` - similar to `wrap<size>` but out-of-range values are clamped rather than wrapped
- `string` - a character literal. These can be passed around but no operations which could incur runtime allocation are possible (i.e. concatenation, etc), and slicing/indexing is not supported - the main purpose of string literals and this type is for use in debug logging functions.

All types can be prefixed with `const` when declaring a constant variable.

#### Numeric literals

SOUL largely follows familiar C/Java/Javascript conventions for numeric values:

- ##### Integers
    Numbers without suffixes are 32-bit signed, e.g. `1234` (32-bit decimal), `0xabcd` (32-bit hexadecimal), `0b101010` (binary).
    For 64-bit values, SOUL supports both C/Java-style `L` or Rust-style `i64` suffixes. E.g. `123L` is the same as `123i64`.
    (Lower-case `l` is not allowed, as it's too easy to confuse with `1`)
    To explicitly mark a number as a 32-bit integer, you can use the suffix `i32`.
    For readability, you can also add an underscore before any of these suffixes, e.g. `123_i64`
- ##### Floating-point
    For a 32-bit float, you can use either `f` or `f32` as the suffix, e.g. `123.0f` or `123.0f32`.
    For 64-bit floats, either write it without a suffix, or use `f64`, `123.0` or `123.0f64`
    For readability, you can also add an underscore before any of these suffixes, e.g. `123.0_f64`
- ##### Complex numbers
    Complex numbers are made from a real and imaginary component. The real component uses the same syntax as the assocated floating point formats (so `f` or `f32` to indicate a 32 bit floating point value) whilst imaginary numbers are specified using the `fi` or `f32i` suffix for 32 bit imaginary components, or `i` or `f64i` for 64 bit imaginary components. Complex constants with both real and imaginary components can be declared by adding these components together, for example `1.2f + 3.4fi`
- ##### Boolean
    `true` and `false` are built-in keywords

#### String literals

SOUL supports very basic string functionality, allowing string literals to be passed around as instances of the `string` type. Literals are written as double-quoted strings using the standard JSON rules for escape sequences.

e.g. `string s = "Hello World \uD83D\uDE42\n";  // Hello World 🙂`

#### Structures

Structures are declared in traditional C style (but without the semicolon after the closing brace):

```C++
struct ExampleStruct
{
    int            member1;
    float          member2;
    bool[8]        member3;
    MyOtherStruct  member4;
}
```

Member access unsurprisingly uses the dot operator:

```C++
let x = myStruct.member3;
```

#### Vector types

Vectors of primitive types can be declared with angle-bracket syntax, e.g.

```C++
float<4> myVector;  // a vector of 4 floats

let v1 = int<4> (1, 2, 3, 4);
let v2 = int<4> (4, 3, 2, 1);

let v3 = v1 + v2; // v3 == (5, 5, 5, 5)

let element1 = v3[1]; // extract an element
```

The vector size between angle brackets must be a compile-time constant.

#### Array types

Arrays are declared with this syntax:
```C++
float[10] x;   // an array of 10 floats, implicitly initialised to all-zeros

let x = int[4] (1, 2, 3, 4);   // initialises a const array of 4 elements.
let x = int[] (1, 2, 3, 4);    // (the size can be omitted if giving an initialiser list)

int[] x = (1, 2, 3, 4);       // initialises a mutable array of 4 elements
```

Unlike vectors, the element type is not restricted to primitive types.

The array size between square brackets must be either a compile-time constant or omitted.

Arrays have a special built-in member-style variable `size` which is returns their number of elements.

Access into an array using the square bracket operator is ideally done by passing a `wrap` or `clamp` type, because these are guaranteed to never be out-of-bounds, so optimal code can be generated.

If an array index is passed as a normal integer, then the compiler will generate a wrap operation around it so that any out-of-range values are brought within safe bounds. This adds run-time overhead so a warning is emitted when this happens. To avoid the warning, you can use the `.at()` method or cast the integer to a suitable `wrap<>` or '`clamp<>` type explicitly.

```C
int[4] myArray;

let y = myArray.size; // y has the type int, and value 4

let x = myArray[3]; // OK
let x = myArray[4]; // Compile-time error because the compiler knows this is out of bounds

wrap<4> wrappedIndex;
let x = myArray[wrappedIndex]; // This is ideal because the compiler knows the index is always in-range

wrap<2> wrappedIndex;
let x = myArray[wrappedIndex]; // Also good - the wrap size can be smaller than the array size

int intIndex;
let x = myArray[intIndex]; // Emits a performance warning because the compiler must add a runtime wrap operation
let x = myArray.at (intIndex); // Compiles cleanly, and performs a run-time wrap on the index value
let x = myArray[wrap<4> (intIndex)]; // Compiles without warnings and the cast performs the wrap operation
```

Negative indexes allow you to take an element at the back of the array, e.g.

```C
int[10] myArray;

let x = myArray[-1];  // returns the last element (same as myArray[9] for a size of 10)
let y = myArray[-2];  // returns the last-but-one element in the array
let z = myArray[-10];  // error: this is out-of-bounds
```

#### Array slicing

Subsections of arrays can be assigned using a colon-based syntax, e.g.

```C++
int[10] x;
x = 123; // set all elements to 123
x[:] = 123; // set all elements to 123
x[2:] = 123; // set all elements from index 2 onwards
x[:3] = 123; // set all elements up to (but not including) index 3
x[1:4] = 123; // set elements 1, 2 and 3

int[8] y;
y[1:5] = x[2:6]; // copying sub-sections of arrays
let z = x[2:-3]; // copies all elements apart from the first 2 and the last 3
```

#### Dynamic array slices

An array declared without a size, e.g. `int[] x` is treated as a dynamically-sized array, so for example a function which takes a parameter like this can be passed arrays which don't have a fixed size, e.g.

```
float getAnArrayElement (float[] a, int index)      { return a.at (index); }
```

To avoid confusion with fixed-size array indexes needing to use the `wrap` or `clamp` types, the `array[index]` syntax is not allowed on dynamic slices. Instead, you can use the following accessor methods:

`array.at (index)`  - this casts the index argument to an integer (rounding down if it's a floating point value), wraps it if it's out-of-range, and returns the element at that index.
`array.read (index)` - (essentially a synonym for `at`)
`array.readLinearInterpolated (index)` - returns the linearly-interpolated value at the fractional position (after wrapping it if it's out-of-bounds). This us only available for arrays of floating-point types.

Other interpolators such as LaGrange, Catmull-Rom, etc, may be added in future releases.

Like for fixed-size arrays, you can use the `.size` property to find out the number of elements in the array, but this is a run-time operation, not a compile-time constant as it is for fixed-size arrays.

Dynamic slices are handled internally as a "fat pointer", i.e. a pointer + size, and because SOUL has no heap or garbage-collection, there are restrictions on how they can be used. In the current release, those limitations include:

- A dynamic slice can only be used for read-only data
- A dynamic slice can only refer to data whose lifetime is permanent for the run of the program - currently that means only state variable and externals can be converted to a dynamic slice. Local fixed-size arrays cannot be referenced by a slice, although in the future there'll be some circumstances where this rule can be relaxed if smarter compilers are able to prove that the data's lifetime will outlive the slice object.

Slices may be left uninitialised, in which case they have a size of zero and any access will return a value of 0 (in whatever element type they're using).

#### Complex components

As complex numbers are primitives, vectors and arrays of complex numbers can be constructed. The real and imaginary components of the complex number can be retrieved using the `.real` and `.imag` accessors respectively. For a complex vector, these accessors will return a vector of real or imaginary components.

```C++
let c = 1.0f + 2.0fi;       // c is a complex32 with real: 1.0f, imag: 2.0f
let d = c * c;              // d is a complex32 with real: -3.0f, imag: 4.0f
let e = d.real;             // e is a float32 with value -3.0f
let f = d.imag;             // f is a float32 with value 4.0f

complex64<4> g = 1.0;       // vector of 4 complex64 with real: 1.0, imag: 0.0
let h = g.real;             // vector of 4 float64 all with value 1.0
```

#### 'External' variables

Sometimes a processor needs to randomly-access large blocks of read-only data (e.g. for audio samples, etc), or to allow the runtime API to provide other constants which aren't part of the source code. To allow the app to get this data across to the SOUL program, the `external` qualifier can be added to a variable.

```
external float[] someData;  // an array of floats (its size is determined at runtime from the data provided)
external int someValue;     // an integer that will be supplied at runtime
external MyStruct thing;    // a structure whose fields should be filled-in at runtime
```

At link-time, the host app must supply a data provider which can give it the value of any externals that the program contains.

```C++
processor DrumLoopPlayer
{
    output stream float<2> audioOut;

    struct Thing
    {
        int thing1, thing2;
    };

    // at link-time, the host program will provide values for these external variables:
    external float<2>[] drumLoop;
    external Thing[] otherThings;

    void run()
    {
        int frameNumber;

        loop
        {
            audioOut << drumLoop.at (frameNumber);
            frameNumber = wrap (frameNumber + 1, drumLoop.size);
            advance();
        }
    }
}
```

The host app will have to supply the data for these variables - in a SOUL patch for example that could be done in the manifest like this:

```json
{
  "soulPatchV1": {
    "ID": "dev.soul.examples.drumloop",

    ...etc...

    "source": "DrumLoopPlayer.soul",
    "externals": {
      "DrumLoopPlayer::drumLoop": "DrumLoop.wav",
      "DrumLoopPlayer::otherThings": [
          { "thing1": 123, "thing2": 456 },
          { "thing1": 234, "thing2": 789 }
        ]
    }
  }
}
```

If asked to convert a JSON string into an array of floats, the patch API will attempt to load it as an audio filename. If possible, it'll do some basic adjustments to match the number of channels to the size of the elements in the target array - so for example it'll mono-ise a multichannel file if the data type is `float[]` or convert a mono file to stereo if the destination type is `float<2>[]`.

##### Special support for audio data in externals

Because an audio file consists of more than just an array, the runtime will use some heuristics to look for structures which can be used to represent audio data. So if it sees a structure which contains an array of floats and also a member variable called "rate" or "sampleRate", it will do the right thing, e.g.

```C++
struct MyStereoAudioFile
{
    float<2>[] channelData;
    float64 sampleRate;
}

external MyStereoAudioFile myAudioSample;
```

If you're using the SOUL Patch API and you tell it to bind a .wav file to this value, it'll do the right thing and fill-in the sample rate from the file.

Currently it just looks for names like `sampleRate`/`rate`/`frequency` but we will continue to extend this system to allow more complex metadata to be included, such as loop points etc.

##### External variable annotations

There are also some annotations which can be added to an external declaration to help the API provide the data.

A default value can be provided, so that if the host doesn't supply a value, the default can be used rather than triggering an error, e.g.

```C++
external int sampleRateToUse [[ default: 44100 ]];
external float someKindOfValue [[ default: 1234.0f ]];
```

If the type is an array of floats, you can ask the runtime to generate some simple waves to populate it, e.g.

```C++
external float[] twoSecondA440TriangleWave [[ triangle, rate: 48000, frequency: 440, numFrames: 96000 ]];
external float[] oneSecondA440Sinewave     [[ sinewave, rate: 44100, frequency: 440, numFrames: 44100 ]];
```

Wave types supported are `sinewave`, `triangle`, `squarewave`, `sawtooth`.

Where the runtime is providing an audio sample, you can add the annotation:
```C++
external float[] audioFileData [[ resample: 48000 ]];
```

...which will tell the runtime to pre-process the audio file (or buffer) provided so that it is resampled to the given rate. The runtime will use a high-quality interpolator and there'll be no run-time overhead once the code is running.

You can also use the annotation 'sourceChannel' to pull out a specific channel from the file:

```C++
external float[] audioFileChannel [[ sourceChannel: 3 ]];  // extracts channel 3 from the source file and returns that
```

#### Type aliases

A namespace or processor can contain `using` declarations to allow aliases to be used for types, e.g.

```C++
using MySampleType = float<2>;
```

#### Variables

There are two scopes in which variables can be declared:

- **Local** variables are self-explanatory - they work like any other stack/scope-based language. You can declare them inside functions and they're visible to any code that follows them in the same block.
- **State** variables are declared inside a processor block. See [Processor state variables](#processor-state-variables) for details

Variables can be declared in several styles:
- `let [name] = [initial value];` - infers the type from the initialiser, and the resulting variable is *const*
- `var [name] = [initial value];` - infers the type from the initialiser, and the resulting variable is *non-const*
- `[typename] [name];` - creates a mutable variable of the given type, which will be implicitly zero-initialised
- `const [typename] [name] = [initial value];` - creates a const variable of the given type

```C++
processor ExampleProcessor
{
    // these are state variables, declared within the body of a processor,
    // and these are visible to all functions within the same processor

    int x, y, z = 123;      // x and y are initialised to 0
    let abc = 1.0f;         // declares a constant
    const int abc2 = 0xff;  // constants declared in this scope can be used for
                            // compile-time constants like array bounds, etc

    void myFunction()
    {
        // these are local variables, declared within the body of a function:

        let b = 1.0f;    // read-only value
        var c = 1.0f;    // mutable value

        float a = 1.0f;
        int d = 3.0;     // error! the initialiser must have a matching type

        MyStructure foo; // declare an instance of a structure

        int[4] myValue;  // array of 4 ints, initialised to zero
        let f = int[3] (3, 4, 5); // declare an array with some initial values
    }
}
```

## Program structure

When the SOUL compiler is given a block of code to parse, it expects it to contain a series of top-level declarations.
There are 3 types of top-level item:

- [Namespaces](#namespace-declarations)
- [Processors](#processor-declarations)
- [Graphs](#graph-declarations)

The order in which a set of declarations occurs is not important, they can refer to each other recursively regardless of which one is declared first.

#### Namespace declarations

A namespace is simply a nested container that contains functions, namespaces, processors and graphs.

Declare a namespace like this:

```C++
namespace ExampleNamespace
{
    void myFunction (int x) ...etc...
    float myOtherFunction (bool b) ...etc...

    graph MyGraph
    {
        // ...etc...
    }

    namespace ExampleNestedNamespace
    {
        // ...etc...
    }
}
```

The name must conform to the rules for valid identifier names.

When referring to a member of a namespace, use the double-colon syntax, e.g.

```C++
void someFunction()
{
    let x = ExampleNamespace::myFunction (1234);
}
```

You can also declare nested namespaces using double-colon syntax, e.g.

```C++
namespace OuterNamespaceName::InnerNamespaceName
{
    ...
}
```

The namespace `soul` is reserved for system use.

#### Function declarations

Functions may be declared inside a namespace or a processor, but not inside a graph.

The syntax follows the familiar C/C++/Java/Javascript style of
```
<returntype> functionName (type param1, type param2... etc)
```

#### Processor declarations
A processor declaration looks like this:
```C++
processor ExampleProcessor
{
    // ...endpoint declarations...

    // ...state variable declarations...

    // ...function declarations...

    void run()
    {
        // ...your processor implementation...
    }
}
```

Processors are the lowest-level of node in a SOUL graph. A processor definition contains its inputs and outputs, its per-instance state variables, whatever functions it needs, and a run() function which does the work.

Many instances of a processor may be instantiated, but the only way a processor can communicate with anything else is via streams connected to its endpoints. There is no shared global state.

##### The `run()` function

Every processor must contain a function with the signature `void run()`.

This function is where the guts of your algorithm is implemented. Typically, you'll write an infinite loop where you:

- Read from your inputs (as needed)
- Write to your outputs (as needed)
- call `advance()`
- repeat (probably forever)

e.g.
```C++
void run()
{
    loop  // (infinite loop)
    {
        let inputValue = myInput;
        myOutput << inputValue * 0.1234f;
        advance();  // moves time forward
    }
}
```

If the `run()` function returns at any point, this essentially puts it into an inert state where it continues to emit silence on all its outputs.

##### The `advance()` function

`advance()` is a special built-in function which causes all the processor's streams to move forward in time by the same amount called the timeslice (the exact amount will depend on the rate at which the processor has been instantiated by its parent graph)

A well-formed `run()` function must contain at least one call to `advance()`.

##### The `init()` function

A processor can optionally declare a function with the prototype `void init()`, and if this exists, the runtime will call it when the processor is created, and before it starts running, in order to do any lengthy set-up and initialisation that might be needed, e.g.

```C++
processor Foo
{
    output stream float out;

    float[1024] myLookupTable;

    void init()
    {
        myLookupTable = generateComplicatedLookupTable();
    }

    void run()
    {
        loop { out << doSomethingUsingLookupTable (myLookupTable); advance(); }
    }
}
```

##### Processor state variables

Processors may contain variables which are "global" in the sense that any function within the processor may refer to them. However they're not global in the sense that a C variable can be global: multiple instances of a processor will each have its own state, containing individual copies of all these variables. (Another analogy is to think of them like class member variables).

e.g.
```C++
processor ExampleProcessor
{
    int stateVariable1, stateVariable2;
    float[10] arrayOfStuff;

    int myFunction (int x)
    {
        ++stateVariable1;
        return x + stateVariable2;
    }
}
```

State variables are inaccessible from outside the processor instance which they belong to, and all calls within that processor instance are guaranteed to be non-interleaved so there's no danger of race conditions.

#### Input/Output Endpoint declarations

Processors and graphs share a syntax for declaring their inputs and outputs, collectively referred to as endpoints. Each item in the list is either marked as an `input` or `output` endpoint, along with its type and unique name, e.g.

```C++
processor ExampleProcessor
{
    input  stream float     in1;          // input which takes a stream of floats
    input  value  int       in2;          // input value of type int
    output stream float64   out1, out2;   // two output streams of float64s
    output stream float<2>  out3;         // output producing a stream of float<2>s
    output value  float64   out4;         // output value of type float64
    output event  MyType    out5;         // output event of a user specified type
}
```

Multiple declarations can also be bundled into a braced set of items, e.g. the code above could be written like this:

```C++
processor ExampleProcessor
{
    input
    {
        stream float in1;
        value int in2;
    }

    output stream
    {
         float64 out1, out2;
         float<2> out3;
    }

    output
    {
        value float64 out4;
    }

    output event MyType out5;
}
```

In addition, arrays of inputs and outputs can be declared. This comes in useful with parameterised processors where the number of inputs or outputs can be set when the processor is instantiated in a graph. e.g

```C++
processor ExampleMixer
{
    input  stream float in[4];  // Declares 4 input streams (in[0] through in[3])
    output stream float out;
}
```

The 'input' or 'output' keyword is followed by the name of its type, which may be:

- `stream` a sequence of values (one per time interval for the processor) of the type which follows. The type must be a primitive or vector.
- `value` an instantaneous value which can be changed at any moment, with no guarantee of temporal accuracy. (An example use-case could be a master volume control).
- `event` a sequence of sample-accurate event callbacks which hold a user-specified type.

A processor must have *at least one output*. Because processors can only interact with the outside world via their endpoints, a processor with no outputs would be unable to do anything useful.

The endpoint declarations must be the first items that appear in the processor or graph declaration. They are then followed by all the other items such as functions, state variables, connections, etc. (These other items can be mixed up in any order).

##### Exposing child endpoints

Graphs can also declare endpoints which map directly only those of a child processor, e.g.

```C++
graph TopLevel
{
    input  child.input1;     // exposes an input stream from the 'child' processor
    input  child.proc1.proc2.input1;  // you can expose child endpoints from sub-graph at any depth
    output child.output1;
    output child.output2  output3; // this exposes 'output2' and gives it the new name 'output3'

    let child = SomeProcessor;

    ...
}
```

Annotations from the original child endpoints will be carried forward, and any new annotations added to the top-level declarations will be merged over them.

#### Endpoint Annotations

A set of [annotations](#annotations) can be attached to any input or output declaration. An annotation list has no effect on the generated code - it's simply a set of key-value pairs for use by the application that is loading the SOUL code - so the exact set of properties and their meanings will depend on the application that is loading and running the code. For an endpoint, a typical set of annotations might define its name, range, default value, etc. E.g.

```C++
graph Reverb
{
    input  stream float<2> audioIn;
    output stream float<2> audioOut;

    input event
    {
        float roomSize  [[ min: 0,  max: 1.0f,  init: 0.8f  ]];
        float damping   [[ min: 0,  max: 1.0f,  init: 0.5f  ]];
        float wetLevel  [[ min: 0,  max: 1.0f,  init: 0.33f ]];
        float dryLevel  [[ min: 0,  max: 1.0f,  init: 0.4f  ]];
        float width     [[ min: 0,  max: 1.0f,  init: 1.0f  ]];
    }
}
```

The properties have no effect on the generated SOUL code, but are visible to the SOUL runtime, and are used to communicate information about inputs and outputs. A typical use would be as the example above, to communicate ranges and initial values for parameters.

#### Graph declarations

A graph is a list of processor instances, and a declaration of which endpoints are connected together. Graphs may be nested inside other graphs, and may be treated as a sub-class of processor.

A graph declaration follows this pattern:
```C++
graph ExampleGraph [optional specialisation parameters]
{
    // [...endpoint declarations...]

    // [...processor instance declarations...]

    // [...connection declarations...]
}
```

##### Processor instance declarations

After declaring a graph's endpoints, you should declare the set of processor nodes that it contains. The syntax for this is a series of `let` statements to declare local names for each instance of a processor or graph, e.g.

```C++
processor AllpassFilter (int frequency)
{
    // ...etc...
}

graph ExampleGraph
{
    output stream float xyz;

    let
    {
        allpass1 = AllpassFilter(225);
        allpass2 = AllpassFilter(341);
        allpass3 = AllpassFilter(441);
        allpass4 = AllpassFilter(556);
    }

    // [...connection declarations...]
}
```

An alternative syntax is to use `let` statements for each item - the code above is equivalent to:

```C++
graph ExampleGraph
{
    output stream float xyz;

    let allpass1 = AllpassFilter(225);
    let allpass2 = AllpassFilter(341);
    let allpass3 = AllpassFilter(441);
    let allpass4 = AllpassFilter(556);

    [...connection declarations...]
}
```

##### Connection declarations

A graph then needs a set of connections to show which node endpoints should be connected together. An input and output can only be connected if they share the same type.

```C++
graph ExampleGraph
{
    output stream float myOutput;

    let
    {
        allpass1 = AllpassFilter(225);
        allpass2 = AllpassFilter(341);
        allpass3 = AllpassFilter(441);
        allpass4 = AllpassFilter(556);
    }

    connection
    {
        allpass1.audioOut -> allpass2.audioIn;
        allpass2.audioOut -> allpass3.audioIn;
        allpass3.audioOut -> allpass4.audioIn;
        allpass4.audioOut -> myOutput;
    }
}
```

If you only want a single instance of a particular type of processor in the graph, you may omit the `let` declaration and just refer to the processor type in the connection section, e.g.

```C++
processor Reverb
{
    // ...etc...
}

graph ExampleGraph
{
    input  stream float myInput;
    output stream float myOutput;

    connection
    {
        myInput -> Reverb.audioIn;
        Reverb.audioOut -> myOutput;
    }
}
```

###### Default endpoints and connection chaining

When you have a processor with only a single input or output endpoint, you can omit the name of the endpoint, e.g.

```C++
connection  audioIn -> Reverb; // this is OK as long as the Reverb processor only has a single input
```

Connections may also be written as a chain, as long as any processors in the middle of the chain don't have more than one input or output, e.g.

```C++
connection  audioIn -> Delay -> Chorus -> Reverb -> audioOut;
```

###### Fan-in/out

If you want to connect multiple sources to a single destination, or send a single source to multiple destinations, a more concise way to write all those connections is to use a comma-separated list, e.g.

```C++
connection   filter1, filter2, filter3 -> audioOut; // all 3 processors are summed into the audioOut
connection   audioIn -> filter1, filter2, filter3; // each value from audioIn is copied to 3 destinations
```

##### Delay Lines

A connection may be used to introduce a delay into the signal chain. To add a delay to a connection, just add a bracketed value to indicate the number of samples to introduce:

```C++
graph SignalWithEcho
{
    source.out -> [5000] -> dest.in;  // pass samples through with 5000 samples delay
    source.out -> dest.in;            // pass samples through with no delay
}
```

##### Processor delay compensation (a.k.a. PDC)

Some types of DSP algorithm cannot be implemented without using an internal delay, which means that their output is out-of-sync with their input by a number of samples.

This means that when mixing a source signal with the delayed output of such a processor, it's desirable to compensate for the delay by adding artificial delays to other paths of the graph. This keeps everything in-sync and avoids phasing problems. In the world of DAWs this is known as PDC (Plugin Delay Compensation).

The SOUL compiler will automatically insert delay lines on appropriate connections of a graph if some of the processors in the graph have internal latency. To declare that a processor has a latency, use the syntax:

```C++
processor MyProcessorWithDelay
{
    input stream float in;
    output stream float out;

    processor.latency = 100; // this means that there's a 100 sample delay

```

This processor property must be set in the main body of the processor, not inside a function, and is a compile-time constant so can't be modified. It can be read if you need to use the value somewhere in the code.

A graph has its latency calculated automatically. For example, a graph containing a sequential chain of 4 processors which each has a 20 sample latency will have 80 sample latency overall.

If you use the ` -> [x] -> ` syntax to add a delay to a connection, this is NOT treated as a delay which should be balanced-out, as it is assumed that you've added such a delay deliberately because you want one, and therefore don't want it to be compensated for.

##### Feedback loops

Feedback loops within a graph are only permitted if one of the connections in the loop contains a delay.

```C++
graph SignalWithEcho
{
    foo.out -> foo.in;         // Error! Can't send output from a processor back into itself!
    foo.out -> [1] -> foo.in;  // OK! Cycles are allowed if the loop is broken by a delay of at least 1 sample
}
```

##### Arrays of Processors

It is possible to declare an array of processors. Like arrays of inputs and outputs, this allows you to parameterise graphs where the exact number of processors can be chosen at runtime. Typical uses for this would be to vary the number of voices in a synthesiser. Arrays of processors are declared using a variation of the let statement, e.g:

```C++
graph ExamplePolyphonicSynthesiser
{
    output stream float myOutput;

    let voices = SynthesiserVoice[8];    // Declare 8 voices

    connection
    {
        voices.audioOut -> myOutput;    // Combine the output of all voices
    }
}
```

In the example above, you can see that an array of voices is connected to a single output. The rules for this allow for 1-to-many and many-to-1 connections. If the arrays have the same size, many-to-many connections are also allowed. In addition, this syntax works for arrays of inputs and outputs, so you can connect arrays of endpoints to arrays of processors with a single connection. e.g

```C++
graph ExampleGraph
{
    output stream float audioSum;
    output stream float voiceOut[8];

    let
    {
        voices = SynthesiserVoice[8];   // 8 voices
        delays = Delay[4];              // 4 delays
    }

    connection
    {
        voices[0].audioOut -> delays[0].audioIn;   // 1-to-1 first element of voices array feeds first delay
        voices[1].audioOut -> delays.audioIn;      // 1-to-many second voice feeds all delays
        delays.audioOut    -> audioSum;            // many-to-1 sum of all delay output feeds myOutput
        voices.audioOut    -> voiceOut;            // many-to-many each voice output feeds exactly one voiceOut stream

        voices.audioOut    -> delays.audioIn;      // ERROR - voices has 8 elements, delays has 4 elements
    }
}
```

#### Module specialisation parameters

All module types (processors, namespaces and graphs) can be declared with some constant values, types, namespace and processor definitions that must be supplied when it is instantiated. These arguments can then be used to specialise the behaviour of the processor. e.g:

```C++
// This is a processor which requires a type definition 'SampleType'
// and a constant int 'delayLength'
processor Delay (using SampleType, int delayLength)
{
    SampleType[delayLength + 1] buffer;

    void doUpdate()
    {
        delayLength = 10;     // ERROR - delayLength is a compile-time constant
    }

    // ...etc...
}

processor Mixer (int voices)
{
    input  stream float audioIn[voices];
    output stream float audioOut;

    // ...etc...
}

// Graphs may also use 'processor' parameters to allow the caller
// to pass in specific types of processor for them to use
graph ExampleGraph (processor DelayType, int length)
{
    let delay1 = DelayType(float, length * 2);
    let delay2 = DelayType(float, length * 3);
    let delay3 = DelayType(float<2>, 10000);
}

```

In addition, defaults for parameters can be specified using an assignment in the declaration. This below example also demonstrates how to declare a namespace alias, which can be used to declare an instance of a namespace with the given parameters:

```C++
// This namespace takes a type definition 'DataType' which defaults to int32
namespace calc (using DataType = int32)
{
    // Returns the sum of two values
    DataType sum (DataType v1, DataType v2)
    {
        return v1 + v2;
    }
}

bool testInt()
{
    // The default DataType is int32
    return calc::sum (2, 3) == 5;
}

bool testFloat32()
{
    // Specify float32
    return calc (float32)::sum (2.0f, 3.0f) == 5.0f;
}

// Create namespace alias for calc on float64
namespace float64Calc = calc (float64);

bool testFloat64()
{
    return float64Calc::sum (2.0, 3.0) == 5.0;
}
```

#### Event endpoints

Each event input in a processor has a special event handler function, which is called whenever an event is received. The handler is introduced by the event keyword, and has a name that matches the corresponding input event. The value passed must match the event definition, and can be passed by value, or const reference. e.g.

```C++
processor EventToStream
{
    input event float eventIn;
    output stream float streamOut;

    event eventIn (float f)
    {
        lastReceivedValue = f;
    }

    float lastReceivedValue;

    void run()
    {
        loop
        {
            streamOut << lastReceivedValue;
            advance();
        }
    }
}
```

Event handlers can generate events themselves and write these to an output event endpoint, and can write multiple events. These will be seen as multiple events to the called processor event handler. e.g

```C++
processor EventGenerator
{
    input event float eventIn;
    output event float eventOut;

    event eventIn (float f)
    {
        // Output 2* and 5* the received event value
        eventOut << f * 2.0f;
        eventOut << f * 5.0f;
    }
}
```

#### Processor oversampling and undersampling

Processors declared within a graph can be marked as running at a faster or slower sampling rate than the graph that contains them. e.g

```C++
graph ExampleGraph
{
    let
    {
        waveshaper = Waveshaper * 4;    // Declare a waveshaper as oversampled 4 times
        visualiser = Visualiser / 100;   // Declare a visualiser undersampled by 100 times
    }
}
```

The oversampling factor is specified as a compile time integer constant. It  can be based on a graph parameter, e.g

```C++
graph ExampleGraph(int controlRate)
{
    let
    {
        visualiser = Visualiser / (controlRate * 4);   // Undersampling rate dependent on the graph parameter
    }
}
```

Oversampling (running at a higher sampling rate) is typically used for audio rate processors where the algorithm produces aliasing or other artifacts, and by oversampling, these products can be reduced.

Undersampling is typically used for control rate processors where changes do not need to be updated as often, such as applying modulation matrix calculations, or outputs updating.

Undersampling and Oversampling can be applied to sub-graphs as well as processors. If these contain other graphs or processors which are themselves oversampled or undersampled the effect is compounded.

##### Choosing an oversampling and undersampling strategy

Any streams in or out of a processor which is running at a different sample rate must be sample rate converted to the new processor sample rate. There are different strategies provided, with different performance and runtime tradeoffs.

Three strategies are provided:

  - `sinc` interpolation - high quality sample rate conversion with minimal aliasing
  - `linear` interpolation - simple linear ramp between sample values
  - `latch` interpolation - repeating the last received value between sample values

By default, oversampled streams are sinc interpolated in both directions. For undersampled processors, input streams are latch interpolated, and the output streams are linear interpolated.

To specify a different interpolation strategy, a modifier is specified in the processor connections. e.g

```C++
graph ExampleGraph
{
    let
    {
        waveshaper = Waveshaper * 4;
    }

    connection
    {
        // Use sinc interpolation for the input to the waveshaper, and linear for its output
        [sinc]   audioIn -> waveshaper.audioIn;
        [linear] waveshaper.audioOut -> audioOut;
    }
}
```

### Functions

Functions are declared with C/C++/C#/Java style:

```C++
int addTwoFloatsAndCastToAnInt (float param1, float param2)
{
    return int (param1 + param2);
}

void modifyAMemberVariable()
{
    ++someMemberVariable;
}
```

Function parameters are always passed using by-value semantics, unless declared as a reference (see the section about references).

The `return` statement works just the way you'd expect it to.

#### Universal function call syntax

SOUL supports "universal function call syntax", which allows any suitable function to be called using the dot syntax as used for method calls in most object-oriented languages.

When parsing a dot operator, the compiler will attempt to find a function whose first parameter matches the left-hand side of the dot, and uses that function if found, e.g.

```
struct MyStruct
{
    int blah;
}

int getBlah (MyStruct& self)      { return self.blah; }

void f()
{
    MyStruct s;

    let x = getBlah (s);   // these two calls do exactly the same thing
    let y = s.getBlah();   // but the second version is more OO-friendly!
}
```

### Generic Functions

Simple generic ("templated") functions are possible by appending a list of named pattern-matching placeholders to the function name:

*return type* function_name<*pattern1*, *pattern2*, ...> ([parameter list])

Each pattern name must be a single identifier, and the return type and parameter declarations may use the pattern names as placeholders for types, e.g.

```
T negateSomeValue<T> (T x)    { return -x; }
T addTwoValues<T> (T x, T y)  { return x + y; }

elementType<ArrayType> get4thElement<ArrayType> (ArrayType array)     { return array[4]; }

void foo()
{
    let sum1 = addTwoValues (1, 2);         // OK: sum1 is an int
    let sum2 = addTwoValues (1.0f, 2.0f);   // OK: sum2 is a float32
    let sum3 = addTwoValues (true, false);  // error! can't add two bools!
}
```

Each call to the function will attempt to pattern-match the argument types and generate a specialised instance of the function. This of course means that the body of the function will be left mostly un-parsed until a call attempts to instantiate it, and then each call may trigger difference compile errors in the function body during instantiation, depending on the types that are resolved.

The language doesn't currently support

### Metafunctions for type manipulation

A set of built-in functions are provided which take *types* as parameters rather than values, and which perform compile-time operations on the types. These are:

| Function              | Operation                                                                                  |
|-----------------------|--------------------------------------------------------------------------------------------|
| `type(T)`             | given an endpoint or value, this evaluates to its type                                     |
| `size(T)`             | returns the number of elements in a vector or array                                        |
| `elementType(T)`      | returns the type of a vector or array's elements, e.g. `elementType(int[4]) == int`        |
| `primitiveType(T)`    | returns the scalar type of a primitive or vector, e.g. `primitiveType(int<4>) -> int`      |
| `isStruct(T)`         | returns `true` if the type is a structure                                                  |
| `isArray(T)`          | returns `true` if the type is an array (either fixed-size or dynamic)                      |
| `isDynamicArray(T)`   | returns `true` if the type is an unsized array                                             |
| `isFixedSizeArray(T)` | returns `true` if the type is a fixed-size array                                           |
| `isVector(T)`         | returns `true` if the type is a vector                                                     |
| `isPrimitive(T)`      | returns `true` if the type is a primitive                                                  |
| `isFloat(T)`          | returns `true` if the type is a float32 or float64                                         |
| `isFloat32(T)`        | returns `true` if the type is a float32                                                    |
| `isFloat64(T)`        | returns `true` if the type is a float64                                                    |
| `isInt(T)`            | returns `true` if the type is an int32 or int64                                            |
| `isInt32(T)`          | returns `true` if the type is an int32                                                     |
| `isInt64(T)`          | returns `true` if the type is an int64                                                     |
| `isScalar(T)`         | returns `true` if the type is a scalar type (i.e. a vector or primitive of a numeric type) |
| `isString(T)`         | returns `true` if the type is a string                                                     |
| `isBool(T)`           | returns `true` if the type is a bool                                                       |
| `isReference(T)`      | returns `true` if the type is a reference                                                  |
| `isConst(T)`          | returns `true` if the type is a constant                                                   |

Note that these functions can be applied to a type name or a value, and can be called in the standard syntax, or applied with the dot operator, e.g.

```
{
    int i = 1;

    int.isScalar        // these expressions all
    isScalar(int)       // do exactly the same thing
    i.isScalar
    isScalar(i)
}
```

### static_assert

The `static_assert` statement emits a compile error if a compile-time boolean expression is false.

In generic functions, it can be handy to use `static_assert` along with metafunctions to provide helpful error messages to callers of the function, e.g.

```C++
T addTwoNumbers<T> (T a, T b)
{
    static_assert (T.isScalar, "Sorry, addTwoNumbers() can only take arguments that are scalar types!");
    return a + b;
}
```

### Compile-time `if`

The syntax `if const (condition)` allows you to check a compile-time constant and use its status to determine whether or not the parser should attempt to compile the true or false branches. (This feature is similar to C++'s `if constexpr`).

e.g.

```
bool isVectorSize4<T> (T something)
{
    if const (T.isVector) // this condition must be a compile-time constant
        if const (T.size == 4)
            return true;

    return false;
}
```

Only the successful branch of the if statement will be checked for errors, and any errors in the other branch will be ignored. That makes it handy for use in a generic function where it can be used to guard code which will only work with a particular type of argument.

### References

The only place where a reference to a variable may be used is in a function parameter, to allow the function to modify the caller's instance of that object rather than copying it.

e.g.
```C++
void addTwo (int& valueToModify)
{
    valueToModify += 2;
}

void foo()
{
    int x = 1;
    addTwo (x);
    // x is now 3
}
```

You may also mark a parameter as being a const reference, which avoids the overhead of copying when passing large read-only structs or arrays into a function.

```C++
void doSomeWork (const int[10000]& values) { ... }
void useALargeStruct (const ExampleLargeStruct& x) { ... }
```

### Casts and Numeric Conversions

Numeric values can be cast using a function-call syntax, e.g.

```C++
int x = int(myFloat);
float x = float(myInteger);
```

The compiler is fairly strict about when implicit casts are allowed, but in cases where no accuracy is lost, the cast can be omitted.

### Arithmetic operators

SOUL provides the standard set of operators:

- Arithmetic: `+` `-` `*` `/` `%`
- Post- and pre-increment and decrement operators: `++` `--`
- Logical: `&&` `||` `!`
- Bitwise: `&` `|` `^` `~` `<<` `>>`
- Comparison: `<` `<=` `>` `>=` `==` `!=`
- Ternary: `? :`

### Conditionals

The `if` statement and ternary operators are pretty unremarkable:

```C++
if (x == 0) foo();

callMyFunction (x > 0 ? 1 : 2);
```

### Loops

SOUL provides the familiar `while` and `for` loops:

```C++
int i = 0;
while (i < 10) { foo(i); ++i; }

for (int i = 0; i < 10; ++i)
    foo(i);
```

There's also a `loop` keyword, which can be used for either infinite or counted loops. Please use this rather than the awful `while (true)` idiom!

```C++
loop  // infinite loop
{
    foo();
}

loop (10)  // runs 10 times
{
    foo();
}
```

You can use the standard `break` keyword to jump out of any of these loops, and `continue` to jump to the start of the next iteration.

The C-style `do..while` syntax isn't supported, because the same effect is easily achieved using `loop` and `break`. This style also gives more flexibility and clarity over the control flow, and access to variables inside the loop, e.g.

```C++
int i = 0;

loop
{
    doSomething();

    if (++x > 10)
        break;
}
```

Range-based `for` loops are allowed if the for statement contains just a variable with a `wrap` type, e.g.

```C++
for (wrap<5> i)
    console << i << " ";     // prints 0 1 2 3 4

for (wrap<6> i = 2)
    console << i << " ";     // prints 2 3 4 5

float[4] a;

for (wrap<a.size> i) // iterates over all the indexes in the variable 'a'
    a[i] = i * 3.0f;
```

### Reading and writing to endpoints

To read the current value of an input, simply use the input's name as an expression.
The read operation is side-effect free, thus an input name can be used multiple times within a timeslice and yields the same value at each use.
Similarly, if an input name is not used within a timeslice, the value for the current timeslice is discarded.

To write a value to an output, use the `<<` operator:

```C++
void run()
{
    loop
    {
        myOutput << sin(phase);

        // ...etc...
    }
}
```

The `<<` operator was chosen for writing to output endpoints as the operation is distinct from an assignment. For different endpoint types, the `<<` operator performs differently:

- `stream endpoints` The sum of the written values between advance calls is written for the current timeslice. If no values are written, the output is 0
- `event endpoints` Each time `<<` is called a distinct event is emitted by the endpoint
- `value endpoints` The written value overwrites the previous value and is persisted until a new value is written

### Annotations

An annotation is a set of key-value pairs which are not used by the SOUL compiler, but instead are passed along for use by the host application that is loading the code, to allow the programmer to add hints about how to use certain elements of the program.

An annotation set is enclosed in double-square-brackets, and contains a (fairly JSON-style) comma-separated list of key-value pairs, e.g.

```C++
input event float myInput [[ name: "Room Size", min: 0, max: 100.0f, init: 80.5f, unit: "%",  step: 1 ]];
```

The names must be legal identifiers (or SOUL keywords, which are allowed), and can only be used once within the list. The value can be any primitive SOUL numeric literal, or a compile-time constant expression. If no colon or value is provided, the value is considered to be a boolean `true`.

Note that annotations can't be used in arbitrary places in the code, there is only a certain set of limited locations where you can add them, such as after a processor or endpoint declaration (other locations will probably be added in the future).

## Linking and resolving modules

Multiple blocks of SOUL code containing processor and graph declarations may be parsed separately, and later linked into a single program.

Basic syntax errors will be picked up when compiling the individual modules, but unresolved symbol errors only occur at link time, once all the available modules are known.

#### Specifying the 'main' processor

When providing a block of SOUL code which contains multiple `processor` or `graph` declarations, you can add an [annotation](#annotations) to indicate which one you intend to be the master. To do this, just append the annotation to the processor name, e.g.

```C++
processor MyMainProcessor  [[ main ]]
{
    ...etc
```

When choosing which processor to instantiate, the runtime will attempt to use the first one with this annotation. If none have this annotation, it'll choose the last declaration that could be a candidate.

# Appendices

### Built-in intrinsic functions

The language provides a set of built-in intrinsics. Most of them will accept parameter types such as float32, float64, and integers where appropriate.

##### Arithmetic
`abs()` `sqrt()` `pow()` `exp()` `log()` `log10()` `floor()` `ceil()` `fmod(numer, denom)` `remainder(numer, denom)`

##### Comparison and ranges
`min(v1, v2)` `max(v1, v2)` `clamp(value, low, high)` `wrap(v, max)`

##### Trigonometry
`sin()` `cos()` `tan()` `acos()` `asin()` `atan()` `atan2()` `sinh()` `cosh()` `tanh()` `asinh()` `acosh()` `atanh()`

#### Vector intrinsics

Most of the arithmetic and trigonometry intrinsics which take a single argument can also be applied to vectors, and the result will be a vector where that operation has been applied to each element in parallel.

There are also some vector-reduce operations: `sum` and `product` which take a vector of any size and reduce it to a single value, e.g.

```
float<5> myVector = (1, 2, 3, 4, 5);
float total = sum (myVector);
```

Note that you can use the universal function call syntax to write any of these intrinsics using the dot operator, e.g.

```
float<5> myVector = (1, 2, 3, 4, 5);
float x = myVector.sum();
float<5> sines = myVector.sin();
```

### Built-in constants

##### Global constants

A few handy floating-point constants are defined (these have 64-bit precision but can be cast to a float32 if you need to):

| Name    | Value                    |
|---------|--------------------------|
| `pi`    | Pi                       |
| `twoPi` | 2 * Pi                   |
| `nan`   | float NaN (Not A Number) |
| `inf`   | float Inf (Infinity)     |

##### Processor constants

Within a processor or graph, the `processor` keyword provides some compile-time constants which describe properties of the current processor.

| Name                  | Value                                                                                                                                             |
|-----------------------|---------------------------------------------------------------------------------------------------------------------------------------------------|
| `processor.period`    | the duration in seconds of one frame (as a float64).                                                                                              |
| `processor.frequency` | the number of frames per second (as a float64).                                                                                                   |
| `processor.id`        | provides a unique `int32` for each instance of a processor. (Useful for things like seeding RNGs differently for each of an array of processors). |
| `processor.session`   | a unique int32 which is different each time the program runs.                                                                                     |

### Built-in library Functions

Various utility functions are provided as part of the SOUL standard library.

These namespaces include:

- [soul::intrinsics](../source/soul_library/soul_library_intrinsics.soul)
- [soul::mixers](../source/soul_library/soul_library_mixing.soul)
- [soul::gain](../source/soul_library/soul_library_mixing.soul)
- [soul::envelope](../source/soul_library/soul_library_mixing.soul)
- [soul::random](../source/soul_library/soul_library_noise.soul)
- [soul::noise](../source/soul_library/soul_library_noise.soul)
- [soul::oscillator](../source/soul_library/soul_library_oscillators.soul)
- [soul::DFT](../source/soul_library/soul_library_frequency.soul)
- [soul::note_events](../source/soul_library/soul_library_notes.soul)
- [soul::voice_allocators](../source/soul_library/soul_library_notes.soul)
- [soul::midi](../source/soul_library/soul_library_midi.soul)
- [soul::audio_samples](../source/soul_library/soul_library_audio_utils.soul)
- [soul::pan_law](../source/soul_library/soul_library_audio_utils.soul)
- [soul::delay](../source/soul_library/soul_library_audio_utils.soul)

### HEART

"HEART" is the name of SOUL's internal low-level language, which is used as the format in which code is passed to a performer to be run. It's analogous to low level languages like LLVM-IR or WebAssembly.

The front-end SOUL compiler converts SOUL into a HEART program, and then it is passed to something like a JIT compiler or a C++ or WASM code-generator. Since HEART is a much simpler, this makes the process of optimising, manipulating and security-checking the code easier, and it makes it simpler to implement new back-end JIT engines.

The syntax of the HEART language can be seen indirectly in the `soul_core` compiler module, but we'll also release a full spec document describing it as soon as we can!

## Creating unit-tests with the `.soultest` file format

The command-line soul tool can be used to run unit-tests by providing it with a `.soultest` file, e.g.

`soul errors MyUnitTests.soultest`

(Power-user tip: If you set up VSCode or your favourite IDE to run this command on a source file it makes it very easy to jump straight to any failed tests, as they are printed in the usual compile error format).

The content of a test file is a sequence of chunks of code, each one beginning with a delineater line to declare the type and parameters for that test.

A delimiter line begins with a double-hash `##` followed by a command. The available commands are:

#### `## compile`

The simplest kind of test, this just makes sure that the block of code which follows it compiles without errors, e.g.

```C++
## compile
int foo() { return 123; }
```

#### `## function`

This attempts to compile the subsequent code-chunk and to evaluate any functions which take no parameters and return a bool. If any of these functions return false, this is considered a failure.

e.g.
```C++
## function
bool testAddition() { return 1 + 1 == 2; }
bool testSubtraction() { return 2 - 1 == 1; }
```

#### `## error <error message>`

This section declares that the code which follows it must make the compiler emit the given error message. The main purpose of this is to test the compiler, so it may not be particularly useful to users, but if you're writing generic functions and want to make sure they reject certain types then it may be handy.

If you provide a delimiter which is just `## error` without a message, then running the test will re-save your `.soultest` file after adding the error message to this line. That lets you easily create a test and use this to fill-in the error message. Subsequent runs of the test will fail if the new error doesn't match the one in the file.

e.g.

```C++
## error 2:41: error: Divide-by zero is undefined behaviour

bool test()  { let i = 5; ++i; return i / 0 == 0; }
```

#### `## processor`

This test expects to find a processor or graph declaration called `test`, which has an output stream of `int`s. It will then instantiate that processor and look at the numbers which come out of the stream.

If a 1 is written, it is ignored
If a 0 is written, the test fails
The processor continues to run until a -1 is written to the stream.

e.g.
```C++
## processor

processor test // the processor must be called "test" (but it could be a complex graph of other processors)
{
    output event int results; // the stream must be of type int, but the name doesn't matter

    void run()
    {
        results << 1; // sending a 1 = success. If this is changed to 0, the test will fail.

        loop { advance(); results << -1; } // a -1 must be emitted to make the test terminate
    }
}
```

#### `## global`

A global chunk should go at the start of the file, and just provides a chunk of shared code which all the other tests will have added. This is handy if youf file contains many tests with repeatitive common functionality.

#### `## console`

This test tries to run a processor called `test` (same rules apply as for the `## processor` test) and checks the string that it writes to the console, e.g.

```C++
## console 321xxx

processor test
{
    output stream int out;

    void run()
    {
        console << 321 << "xxx";
        loop { out << -1; advance(); } // the -1 is needed to make the test terminate
    }
}
```

#### `## disabled`

This just marks the block as disabled, and no test is run, but the number of disabled tests in the file is reported in the results.