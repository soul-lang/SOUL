
- #### What is SOUL?

    You might want to read the [project overview](./SOUL_Overview.md).

- #### Who created SOUL?

    SOUL was first dreamt-up by Julian Storer at ROLI/JUCE in 2016, and announced publicly at [ADC2018](https://youtu.be/-GhleKNaPdk?t=910). Its initial beta release was in November 2019.

- #### What is the project status?

    SOUL is a long-term multi-year project that consists of a number of sub-projects.

    - __Language spec__ - the SOUL language is stable, and at a V1.0 status. Although we'll add more features in future versions, it'll mainly be syntactic sugar and we don't expect any major changes to the basic language.
    - __Compiler__ - the SOUL command-line tool is at a V1.0 status. As well as transpiling SOUL to other languages, it can run patches in its JIT engine, run tests, generate documentation and perform various other tasks.
    - __Core library__ - a range of basic DSP building blocks are available, and we'll be continually adding and updating the library content.
    - __SOUL Patch Format__ - the SOUL patch format is at a V1.0 status. It may be updated in future to add new features.
    - __soul.dev website__ - the SOUL website has been running as a simple playground since 2018. We plan to add extensive new features to the site to turn it into a very powerful development portal.
    - __Network Venue Support__ - A goal for SOUL is to allow it to be run remotely across any kind of network. This is work-in-progress, with our network protocol expected to be public around mid-2021.
    - __Target Devices__ - it's still early days for our support of processors other than Intel/ARM, but this is on the roadmap.

- #### How can I write some SOUL code?

    The easiest way is to try the web playground at [soul.dev](https://soul.dev/lab)
    You can also load and live-code SOUL patches using the latest versions of [Tracktion Waveform](https://tracktion.com/products/waveform)
    You can also compile and play patches using the [command-line tools](https://github.com/soul-lang/SOUL/releases/latest)

- #### Where can I find more technical details?

    The project website is at [soul.dev](https://soul.dev).
    This repository has various other guides in the [docs](../docs/) folder.
    You can look at some of the [source code](../source) in this repository

- #### Where can I give feedback, get help, or discuss the project?

    Until we set up a dedicated forum, the best public forum for discussing SOUL is probably the [JUCE forum](https://forum.juce.com/), where the team are available to answer questions.

- #### What is the licensing/business model?

    Our intention is to make SOUL entirely free and unencumbered for developers to use.
    All our public source code is [permissively (ISC) licensed](../LICENSE.md). We're currently keeping some of our secret sauce closed-source, but the [EULA](../SOUL-EULA.md) allows use of it freely to encourage its adoption in 3rd party hardware and software.
    Ultimately, we plan to commercialise SOUL by licensing back-end drivers and other IP for use by vendors who are building SOUL-compatible hardware products.
