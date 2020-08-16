## CHOC: _"Clean Header Only Classes"_

A random assortment of simple, header-only C++ classes with as few dependencies as possible.

The C++ standard library lacks a lot of commonly-needed functionality, and some simple operations are much more of a faff than they really should be, so I've watched myself re-implement the same kinds of small helper classes and functions many times for different projects.

This repository is an attempt at avoiding wheel-reinvention, by collecting together some of the real basics that I need, and making it as frictionless as possible to add the code to any target project.

So with that goal in mind, the plan is for everything in here to be:
 - Permissively-licensed.
 - Strictly header-only. Just `#include` it and you're done, with zero project/build steps needed.
 - Minimally inter-dependent. If you only need a couple of these classes, you should be able to cherry-pick those files into your project without dragging the whole repo along.
 - Clean, consistent, concise, modern C++. Not too simple, or too over-generic and fancy. Easy to skim and navigate. Self-documenting where possible, with decent comments.

The choice of content is driven by what I (and any other contributors) need for our own projects. So there are some text and container helpers, some audio bits and pieces, some classes that are helpful for compilers and parsers, and quite a bit of miscellany. This may change over time.

### Highlights

Some of the trinkets that you'll find in here include:

- A fast, round-trip-accurate [float/double to string converter](./text/choc_FloatToString.h)
- Some [type and value](./containers/choc_Value.h) classes which can represent typed values, but also build them dynamically, serialise them to a compact binary format, and also as [JSON](./text/choc_JSON.h).
- Some [classes](./audio/choc_SampleBuffers.h) for managing buffers of multi-channel sample data, which can flexibly handle both owned buffers and non-owned views in either packed/interleaved or separate-channel formats
- Some utility classes for handling [MIDI messages](./audio/choc_MIDI.h)
- Some [UTF8](./text/choc_UTF8.h) validation and iteration classes that have been useful in compiler tokenisers
- A scrappy collection of maths, text and container helpers which will grow randomly over time..

Hopefully some people out there will find some of these things useful! If you do use any of it, please note that choc is not trying to be a "product" and is very much a background task for me. So requests for help, advice, features, PRs, bikeshedding, etc are likely to be respectfully ignored :)



-- Jules