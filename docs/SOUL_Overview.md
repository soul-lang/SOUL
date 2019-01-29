### What is SOUL?

SOUL is short for __SOU__*nd* __L__*anguage*.

Its mission is to give audio development a long-overdue kick in the pants, both in terms of how we write audio code, and where the code executes.

The SOUL platform is a language and an API. The language is a small, carefully crafted DSL for writing the real-time parts of an audio algorithm. The API is designed to deploy that SOUL code to heterogeneous CPUs and DSPs, both locally and remotely.

### What problems does it solve?

Audio coding has a steep learning curve for beginners, is laborious and error-prone for experts, and we run most of our audio code on hardware that's poorly-suited to the task.

- **CPUs are a bad place to run audio code** - To get good latency, a processor needs to wake up hundreds of times per second to render small chunks of audio data. These wake-ups require extremely precise timing, and mustn't be interrupted by other tasks. Non-realtime operating systems aren't designed for this kind of scheduling, and modern power-throttling and asymmetrical-core CPU architectures make it tough to achieve the kind of long-term stability needed.

- **Domain-specific Architectures (DSAs) are the future** - The Moore's-Law party is over and the semiconductor industry is turning to DSAs for future performance and power gains. This trend involves finding workloads that can be moved off CPUs and onto special-purpose processors - graphics, machine learning and other areas of computing have already been transformed in this way, and it's time for audio to follow the same path.

- **Current tools for audio coding are complex and non-portable** - The standard way to write high-performance audio code involves C/C++, which is a big barrier to the vast majority of developers. It's also a dangerous language for beginners to play with, and since most audio enthusiasts are more interested in writing DSP algorithms than spending time learning to write safe C++, it's no surprise that the audio industry is rife with buggy software.

### Surely the world doesn't need another programming language?

Don't panic! SOUL isn't trying to replace any existing languages. It's a *domain-specific* embedded language for writing just the realtime code in an audio app or plugin.

The goals of the project require a language because:

- Code written in a language that is secure by design can be safely executed on remote devices, inside drivers, or inside bare-metal or realtime kernels, even without a sandbox. This is key to reducing overhead and achieving low latency performance.

- By structuring the code in a graph-like form, it can be dynamically redeployed onto target processors with different parallelisation characteristics.

- SOUL has been designed to eliminate whole categories of common coding blunders, and its API hides most boilerplate work that audio apps generally had to provide.

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

### Who is the target audience?

We're aiming for SOUL to be valuable to audio developers at all levels - from absolute beginners (for whom the dramatically simpler syntax, API and browser-based dev tools will be helpful) to veteran audio professionals (who will appreciate its promise of lower latency, faster development and deployment to emerging accelerated hardware).

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

            advance();    // Moves all our streams forward by one 'tick'
        }
    }
}
```

You write a `processor` as if it's a thread. It has a `run()` function which will usually contain an infinite loop, inside which you read from your inputs and write to your outputs, doing whatever operations are needed on the values involved. Each time round the loop you call `advance()` to move all the streams forward by a fixed time interval. State can be stored in local or processor scoped variables (think of these as class members). You can create helper functions and structs just like you'd expect to. All of these things can go into namespaces to make library code easy to manage.

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

Your host program passes all this SOUL code into the API, which compiles and links it together, ready for running on a device. The API contains a range of features for finding an available device to use, and for controlling and streaming data to and from the running program.
