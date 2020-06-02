## CHOC: _"Clean Header Only Classes"_

A random assortment of simple, header-only C++ classes with as few dependencies as possible.

The C++ standard library lacks a lot of commonly-needed functionality, and some simple operations are much more of a faff than they really should be, so I've watched myself re-implement the same kinds of small helper classes and functions many times for different projects.

This repository is an attempt at avoiding wheel-reinvention, by collecting together some of the real basics that I need, and making it as frictionless as possible to add the code to any target project.

So with that goal in mind, the plan is for everything in here to be:
 - Permissively-licensed.
 - Strictly header-only. Just `#include` it and you're done, with zero project/build steps needed.
 - Minimally inter-dependent. If you only need a couple of these classes, you should be able to cherry-pick those files into your project without dragging the whole repo along.
 - Clean, consistent, concise, modern C++. Not too simple, or too over-generic and fancy. Easy to skim and navigate. Self-documenting where possible, with decent comments.

The choice of content is driven only by what I (and other contributors) need for our own projects. So there are some text and container helpers, some audio bits and pieces, some classes that are helpful for compilers and parsers, and quite a bit of miscellany. This may change over time.

Hopefully other people will find useful things in here too! But if you use it, please note that choc is not trying to be a "product" or even a public resource: requests for help, advice, features, PRs, bikeshedding, etc are likely to be respectfully ignored :)


-- Jules