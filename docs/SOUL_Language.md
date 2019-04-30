## SOUL Language Guide

SOUL uses a familiar syntax which shares a lot of syntax and style with many mainstream languages (e.g. C/C++, Java, Javascript, C#, etc). This document covers the basic structure and syntax of the code that is passed into the SOUL compiler.

The intended audience for this guide is people who've at least dabbled with some simple programming using a procedural language.

If nothing in this document seems particularly surprising or unusual, then we've succeeded in our goals for making the language accessible.

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
- `wrap<size>` - a range-constrained `int32` type. The `size` value must be a compile-time constant, and operations on this type are guaranteed to result in values where 0 <= x < size. If incrementing or decrementing it, out-of-range values will wrap around.
- `clamp<size>` - similar to `wrap<size>` but out-of-range values are clamped rather than wrapped
- `string` - a character literal. These can be passed around but no operations which could incur runtime allocation are possible (i.e. concatenation, etc), and slicing/indexing is not supported - the main purpose of string literals and this type is for use in debug logging functions.

All types can be prefixed with `const` when declaring a constant variable.

#### Numeric literals

SOUL largely follows the familiar C/C++/Java/Javascript convention for numeric values:

- ##### Integers
    e.g. `1234` (decimal), `0xabcd` (hexadecimal)
- ##### Floating-point
    e.g. `123.0f` (32-bit float), `123.0` (64-bit float), `1.0e-10` (64-bit float)
- ##### Boolean
    `true` and `false` are built-in keywords

#### String literals

SOUL supports very basic string literal functionality, allowing literals to be passed around as instances of the `string` type. String literals are written as double-quoted strings using the standard JSON rules for formatting escape sequences within them.

e.g. `string s = "Hello World\n";`

#### Array types

Arrays are declared with this syntax:
```C++
float[10] x;   // an array of 10 floats, implicitly initialised to all-zeros

let x = int[4] (1, 2, 3, 4);   // initialises a const array of 4 elements.
let x = int[] (1, 2, 3, 4);    // (the size can be omitted if giving an initialiser list)

int[] x = (1, 2, 3, 4);       // initialises a mutable array of 4 elements
```

Arrays have a special built-in member-style variable `size` which is a compile-time constant returning their number of elements.

Access into an array using the square bracket operator requires the parameter type to be a compile time constant, or `wrap` or `clamp` to guarantee that its value is never out-of-bounds. e.g.

```C
int[4] myArray;

let x = myArray[4]; // error
let x = myArray[3]; // OK

let y = myArray.size; // y has the type int, and value 4

wrap<4> wrappedIndex;
let x = myArray[wrappedIndex]; // OK

wrap<2> wrappedIndex;
let x = myArray[wrappedIndex]; // OK - the wrap size can be less than the array size

clamp<5> clampedIndex;
let x = myArray[clampedIndex]; // error - would need to be clamp<4> or less to work
```

To access an index with an integer, you can either cast it to a `wrap` or `clamp`, or use the `at()` method:

```C++
let x = myArray.at (someInteger); // this will check the incoming value and modulo
                                  // it with the array size if it's out-of-bounds

let x = myArray[clamp<myArray.size>(someInteger)];  // use a cast to clamp the value
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

#### External data arrays

Sometimes a processor needs to randomly-access large blocks of read-only data (e.g. for audio samples, etc). To allow the app to get this data across to the SOUL program, the `external` qualifier is added to an array with an undefined size (`[]`).

`external float[] someData;   // an array of floats`

Elements can be read from the object with the `[]` operator like a normal array, but it does not provide all the other operations that an array does. An external variable can also be passed by value without the data itself being copied, so you can pass it around like a handle for use in functions.

At runtime, the app must use an API to bind a data provider (either a lump of raw data or a callback function) for each external that the program declares. These data providers determine the size of the array when the program uses it.

```C++
processor ExampleSampleLooper
{
    // at link-time, the host program uses the API
    // to set the data that these variables will contain
    external float<2>[] myAudioSample1, myAudioSample2;

    int sampleIndex;

    void run()
    {
        // plays a continuous loop of one of our audio samples
        loop
        {
            audioOut << getNextFrame (myAudioSample1);
            advance();
        }
    }

    float<2> getNextFrame (external float<2>[] sample)
    {
        let result = sample.at (sampleIndex);

        if (++sampleIndex >= sample.size)
            sampleIndex = 0;

        return result;
    }
}
```

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

        MyStructure x;   // declare an instance of a structure

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

`advance()` is a special built-in function which causes all the processor's streams to move forward in time by the same amount (the exact amount will depend on the rate at which the processor has been instantiated by its parent graph)

A well-formed `run()` function must contain at least one call to `advance()`.

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

#### Input/Output declarations

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

#### Endpoint Properties

Properties can be associated with any input or output declaration. Properties have unique names, and are either string or numeric. Multiple properties can be associated with an endpoint. Properties are declared between `[[` and `]]` markers after the name, and appear as a json style list using `:` seperators:

```C++
graph Reverb
{
    input  stream float<2> audioIn;
    output stream float<2> audioOut;

    input event
    {
        float roomSize [[ min:0, max:1, init:0.8 ]];
        float damping  [[ min:0, max:1, init:0.5 ]];
        float wetLevel [[ min:0, max:1, init:0.33 ]];
        float dryLevel [[ min:0, max:1, init:0.4 ]];
        float width    [[ min:0, max:1, init:1.0 ]];
    }
}
```

The properties have no effect on the generated SOUL code, but are visible to the SOUL runtime, and are used to communicate information about inputs and outputs. A typical use would be as the example above, to communicate ranges and initial valuess for parameters.

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

###### Processor instance declarations

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

###### Connection declarations

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

###### Arrays of Processors

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
        voices[0].audioOut -> delay[0].audioIn;   // 1-to-1 first element of voices array feeds first delay
        voices[1].audioOut -> delay.audioIn;      // 1-to-many second voice feeds all delays
        delays.audioOut    -> myOutput;           // many-to-1 sum of all delay output feeds myOutput
        voices.audioOut    -> voiceOut;           // many-to-many each voice output feeds exactly one voiceOut stream

        voices.audioOut    -> delays.audioIn;     // ERROR - voices has 8 elements, delays has 4 elements
    }
}
```



#### Processor/Graph specialisation parameters

A processor or graph can be declared with some constant values and type definitions that must be supplied when it is instantiated. These arguments can then be used to specialise the behaviour of the processor. e.g:

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

#### Event endpoints

Each event input in a processor has a special event handler function, which is called whenever an event is received. The handler is introduced by the event keyword, and has a name that matches the corresponding input event. The value passed must match the event definition, and can be passed by value, or const reference. e.g.

```C++
processor EventToStream
{
    input event float eventIn;
    output stream float eventOut;

    event eventIn (float f)
    {
        lastReceivedValue = f;
    }

    float lastReceivedValue;

    void run()
    {
        loop
        {
            eventOut << lastReceivedValue;
            advance();
        }
    }
}
```

Event handlers can generate events themselves and write these to an output event endpoint, and can write multiple events. These will be seen as multiple events to the called processor event handler. e.g

```C++
processor EventToStream
{
    input event float eventIn;
    output stream float eventOut;

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

  - **sinc** interpolation - high quality sample rate conversion with minimal aliasing
  - **linear** interpolation - simple linear ramp between sample values
  - **latch** interpolation - repeating the last received value between sample values

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
        // Use sinc interpolation for the input to the waveshaper, and linear for it's output
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

SOUL provides the usual trio of `while`, `do` and `for` loops:

```C++
int i = 0;
while (i < 10) { foo(i); ++i; }

int i = 0;
do { foo(i); ++i; } while (i < 10);

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

### Reading and writing to endpoints

To read the current value of an input, simply use the input's name as an expression.

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


### Built-in intrinsics and constants

The language provides a set of built-in intrinsics:

- Arithmetic: `abs()` `sqrt()` `pow()` `exp()` `log()` `log10()` `floor()` `ceil()`
- Range clamping: `min(v1, v2)` `max(v1, v2)` `clamp(v, low, high)` `wrap(v, max)` `fmod(numer, denom)` `remainder(numer, denom)`
- Trigonometry: `sin()` `cos()` `tan()` `acos()` `asin()` `atan()` `atan2()` `sinh()` `cosh()` `tanh()` `asinh()` `acosh()` `atanh()`

All of the intrinsics are defined for `float32` and `float64` data types. The following functions also support `int32` and `int64` data types:

`min()` `max()` `clamp()` `wrap()` `abs()`

### Built-in library Functions

A set of utilities are available in the `soul::` namespace. These include:

```C++
namespace soul
{
    float dBtoGain (float decibels);
    float gainTodB (float gain);

    float addModulo2Pi (float value, float increment);

    float noteNumberToFrequency (float noteNumber);
    float frequencyToNoteNumber (float frequency);
}
```

Within a processor or graph, the special keyword `processor` provides member variables to get information about constants:

- `processor.period` returns the duration in seconds of one sample (as a float64)
- `processor.frequency` returns the number of samples per second (as a float64)

## Linking and resolving modules

Multiple blocks of SOUL code containing processor and graph declarations may be parsed separately, and later linked into a single program.

Basic syntax errors will be picked up when compiling the individual modules, but unresolved symbol errors only occur at link time, once all the available modules are known.
