# SOUL Overview

<!-- @import "[TOC]" {cmd="toc" depthFrom=2 depthTo=6 orderedList=false} -->

<!-- code_chunk_output -->

- [What is SOUL?](#what-is-soul)
- [What problems does it solve?](#what-problems-does-it-solve)
- [Why does the world need another programming language?](#why-does-the-world-need-another-programming-language)
- [Who is the target audience for the SOUL language?](#who-is-the-target-audience-for-the-soul-language)
- [Who is the target audience for the SOUL platform architecture?](#who-is-the-target-audience-for-the-soul-platform-architecture)
- [What does the code look like?](#what-does-the-code-look-like)
- [APIs](#apis)
  - [SOUL Patches](#soul-patches)
  - [SOUL Low-level API](#soul-low-level-api)
  - [Code-generation tools](#code-generation-tools)
- [Tools for developing SOUL code](#tools-for-developing-soul-code)

<!-- /code_chunk_output -->

### What is SOUL?

SOUL is short for __SOU__*nd* __L__*anguage*.

It's a platform for writing and running audio code, consisting of two elements: a **language** and a **runtime**.
- The SOUL _language_ is a small, carefully crafted DSL for writing real-time DSP code. Its design goals are to be fast, secure, safe, and simple for even beginners to learn.
- The SOUL _runtime_ is a collection of tools and technologies for running SOUL code with very high efficiency, on heterogeneous CPUs and DSPs, both locally and remotely.

### What problems does it solve?

Audio coding has a steep learning curve for beginners. Even for experts, it's laborious and error-prone. And we run most of our audio code on general-purpose systems that aren't really designed for the demands of an audio work-load.

- **CPUs are a bad place to run audio code** - To get good latency, a processor needs to wake up hundreds of times per second to render small chunks of audio data. These wake-ups require extremely precise timing, and mustn't be interrupted by other tasks. Non-realtime operating systems aren't designed for this kind of scheduling, and modern power-throttling and asymmetrical-core CPU architectures make it tough to achieve the kind of long-term stability needed.

- **Audio processing is lagging behind other domains** - Most other power-hungry tasks such as graphics, machine learning, network packet handling, etc are no longer done on general-purpose CPUs, but have migrated to dedicated processors which can run those workloads faster or more efficiently. Ironically, most of our devices already contain audio DSP processors, but the lack of a standard platform for safely running user code on them means that developers have no way of taking advantage of this.

- **Current tools for audio coding are complex and non-portable** - The standard way to write high-performance audio code involves C/C++, which is a big barrier to the vast majority of developers. As well as being difficult, it's also a dangerous language with many pitfalls for beginners, requiring years of effort to gain the skills needed to write it safely and securely. Most audio developers are more interested in DSP mathematics and algorithms than in honing their C++ skills, so it's no surprise that the audio industry is awash with buggy software.

### Why does the world need another programming language?

SOUL isn't trying to replace any existing languages. It's a *domain-specific* embedded language for writing just the realtime code in an audio app or plugin.

The goals of the project require a language because:

- Code written in a language that is secure by design can be safely executed on remote devices, inside drivers, or inside bare-metal or realtime kernels, even without a sandbox. This is key to reducing overhead and achieving low latency performance.

- By structuring the code in a graph-like form, it can be dynamically redeployed onto target processors with different parallelisation characteristics.

- SOUL has been designed to eliminate whole categories of common coding blunders, and its API hides a lot boilerplate work that audio apps generally had to provide.

- Because SOUL is an embedded language, even apps written in non-native languages like Javascript, Java, Python, etc can achieve C++-level (or better) performance, without the developer needing to learn C++ or use a compiler toolchain.

Think of it as an audio equivalent of *OpenGLSL* or *OpenCL*. Like those languages, you'll typically write a host app (in some other language) which passes a chunk of SOUL code to the SOUL API; the API compiles and sends it on to the best available driver/CPU/DSP or external device.

That might mean that it just runs on your normal CPU using a JIT-compiler. Or if an audio DSP is available, it might run on that. Or it may send the code to an external audio I/O device which contains a SOUL-compatible processor. Running on a DSP/bare-metal system means that you can get much better latency and performance than running it on a CPU, but your app doesn't have to worry about that - you write once, and the SOUL API takes care of deploying it to all kinds of different architectures.

Because all the performance-critical work happens in SOUL, the host app (or plugin, or web-page) doesn't have to be fast, so all the glue code can be written in user-friendly languages like Javascript, Java, Python, C#, etc.

In the same way that graphics code runs best on a dedicated GPU attached to your display, we believe that audio code should ideally run on a dedicated DSP attached directly to your speakers.

Some of the design goals for the language are:

- **Frictionless to learn** - Having to learn a new language for a single task is a pain, so we've made it as easy as possible. SOUL's syntax should be second-nature to anyone who's done a bit of casual programming in any of the more common languages. We certainly think that learning SOUL is far easier than the huge set of unwritten rules you need to understand to confidently write safe real-time code in C++.

- **Graph-based architecture** - Not only is a graph of connected nodes the best conceptual model for most audio tasks, it also retains structural information that the SOUL runtime can use for some clever auto-parallelisation tricks. This is the secret of being able to run the same naively-written program optimally on a wide range of heterogeneous CPU/DSP architectures.

- **Hard real-time performance** - No heap allocation. No GC. No VM. No race conditions. No system calls. When being JIT-compiled to run on a CPU, SOUL code will match or exceed C++ levels of performance.

- **Runs securely without sandboxing or overhead** - No pointers. No recursion. No stack overflows. No runtime bounds-checking. We want the code to run deep inside kernel processes or on bare-metal hardware, but we don't want to add the overhead of sandboxing or a VM. So the language itself stops you writing code that could crash, corrupt memory, use excessive CPU time, or deadlock. Although this sounds restrictive, if you were writing the same thing in C++ you'd end up having to manually avoid all the same pitfalls, but the compiler wouldn't be able to spot your mistakes.

Sadly, no existing language fitted the bill, so we created one!

### Who is the target audience for the SOUL language?

We expect SOUL to be valuable to audio developers at all levels - from absolute beginners (for whom the dramatically simpler syntax, API and browser-based dev tools will be helpful) to veteran audio professionals (who will appreciate its promise of lower latency, faster development and deployment to emerging accelerated hardware).

### Who is the target audience for the SOUL platform architecture?

At the high-end, SOUL will allow musicians to connect external accelerator devices to work on larger, more power-hungry sessions with much lower latency, while freeing up their laptop CPUs to handle the UI for their tools.

At a more general consumer level, SOUL can improve power and battery life for mobile devices that support it with dedicated hardware. And by providing a flexible platform for implementing audio codecs, it could be a more future-proof replacement for silicon which is dedicated to performing decompression of specific common codec formats.

If wireless speakers and headphones run SOUL internally, it will reduce their latency, making games more responsive, and will reduce the network bandwidth needed to run them, which is good for connection stability. For VR, reduced latency means better realism. And for gamers in general, shifting the audio workload off the CPU leaves more capacity for game developers to devote to other aspects of gameplay.

### What does the code look like?

SOUL is a procedural, non-OO language with an actor model and a graph/node/stream architecture.

We intentionally wanted SOUL to be non-threatening and non-surprising for anyone who's dabbled in other common languages. If you've ever used a curly-brace-based language (e.g. C/C++/C#, Javascript, Java, Swift, Go, Rust, etc) then SOUL will be very familiar to read, and you should be happily writing it with just a quick introduction to a few basic concepts.

Boilerplate is compact and self-explanatory, and the actual guts of your algorithm where you do the mathematical work will look pretty much like it would if you wrote it in C/C++.

A SOUL program will usually contain some `processor` and `graph` declarations. Processors are written as single-threaded actors that do the number-crunching, and graphs contain a set of processors and their connections.

E.g. This minimal gain processor takes a mono input and makes it quieter:

```C
processor MinimalGainExample
{
    // declare our inputs and outputs:
    input  stream float audioIn;   // mono input stream called "audioIn"
    output stream float audioOut;  // mono output stream called "audioOut"

    // every processor must declare a run() function
    void run()
    {
        const float gain = 0.5f;  // a constant to use as our gain factor

        loop   // (this just loops forever)
        {
            audioOut << audioIn * gain;  // Read the next input sample, multiply by our
                                         // constant, and write it to our output

            advance();    // Moves all our streams forward by one frame
        }
    }
}
```

You write a `processor` as if it's a thread. It has a `run()` function which will usually contain an infinite loop, inside which you read from your inputs and write to your outputs, doing whatever operations are needed on the values involved. Each time round the loop you call `advance()` to move all the streams forward by a fixed time interval. State can be stored in local variables, or processor-scoped variables (think of these as class members). You can create helper functions and structs just like you'd expect to. All of these things can go into namespaces to make library code easy to manage.

A `graph` is equally straightforward - it also declares some inputs and outputs, a list of processors/sub-graphs, and a list of connections to show where the data goes.

```C
processor MySinewaveGenerator (float frequency)
{
    output stream float sineOut;

    // run function omitted for clarity
}

graph PlayTwoSineWaves
{
    output stream float audioOut; // declare a mono output called "audioOut"

    // declare the processor nodes in our graph...
    let sin1 = MySinewaveGenerator (330.0);
    let sin2 = MySinewaveGenerator (420.0);

    // declare the list of all connections...
    connection
    {
        sin1.sineOut -> audioOut;
        sin2.sineOut -> audioOut;
    }
}
```

Your host program passes all this SOUL code into the API, which compiles and links it (converting it into an internal low-level language called HEART), ready for running on a device. The API contains a range of features for finding an available device to use, and for controlling and streaming data to and from the running program.

To learn more about the language syntax, see the [language guide](./SOUL_Language.md).

### APIs

To write an app which can load and run SOUL code, there are two levels of API:

#### SOUL Patches

The SOUL patch format is described in detail [here](./SOUL_Patch_Format.md).

This is a high-level audio-plugin-like format for writing SOUL that can be loaded into a host like a DAW. It is designed to be fairly similar in structure to formats like VST/AU/AAX so that it could be supported alongside them in a typical DAW without it being too disruptive.

A SOUL patch can declare a set of audio and MIDI i/o channels, and parameter controls. The format also contains a convention for patches to follow if they want to provide GUI code for a host to show (not written in SOUL!)

#### SOUL Low-level API

Patches are a high-level abstraction with a limited interface, but we will also allow access to the lower-level API for compiling and running SOUL, which can be used in a much more flexible way.

The main concepts involved in this API are **Performers** and **Venues**.

- A *Performer* is an implementation of a JIT compiler which can be given a SOUL program and asked to synchronously render blocks of data.

- A *Venue* is an independent device (e.g. a machine accessed via a network, or a separate process or driver running on your local machine), which can be sent a SOUL program to run, and then controlled remotely and asynchronously.

A developer can build a SOUL host which either compiles and runs SOUL synchronously inside its own process using a *performer*, or which dispatches the code to run remotely (and at low latency) in a suitable *venue*. Either way, using these APIs means that the programs are not limited to simple audio/MIDI i/o like the SOUL Patch format, and can have input and output channels containing any kind of data stream.

#### Code-generation tools

The SOUL command-line tools will provide various conversion utilities for translating SOUL into C++ or WASM to be incorporated manually into a legacy codebases. For more information about the WASM generator, see [this document](./SOUL_WASM.md).

### Tools for developing SOUL code

For end-users, we expect to see several environments in which they can write and test SOUL:

- In a browser playground: at [soul.dev](https://soul.dev/lab), where the code is compiled and run in a browser as WASM/Web-audio.
- In a DAW which supports SOUL patches, such as [Tracktion Waveform](https://www.tracktion.com/products/waveform-free) (v10 or later)
- Running the [SOUL command-line tool](./SOUL_Command.md) to compile and play a particular file or patch

Most of these environments support live-reloading of the code when it is modified and re-saved in a text editor.

Of course in the future, graphical UIs for visually constructing SOUL graphs are likely to become an easy way to get started for people who aren't so comfortable writing code.