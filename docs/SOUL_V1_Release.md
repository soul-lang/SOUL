### SOUL, a new language now in V1.0, makes audio coding vastly more accessible

##### January 20, 2021

London — Today’s release of SOUL, the new universal language for audio applications, will dramatically lower barriers and expand access for developers creating any apps that depend on audio, as well as providing a more efficient architecture for audio processing.

SOUL has reached version 1.0 status with its language, compiler, and “SOUL patch” format all stable, tested, and ready to use in a wide variety of audio-related projects.. The team, led by JUCE and Tracktion creator Julian Storer, is already at work on an array of other SOUL tools including a visual editor and a developer portal.

“SOUL will revolutionize audio app development, eliminating challenges that have impeded developers for too long,” said Julian Storer, Head of Software Architecture at ROLI. “The need for a radical rethink of how audio apps are made has only become more urgent since I started the SOUL project in 2016. I’m tremendously excited about the V1.0 released today and the additional tools to come.”

Both a programming language and a platform for creating audio plugins and apps, SOUL will solve four fundamental problems facing audio developers today:

- Easy to learn and use: C++, the standard language for audio app coding, is extremely difficult to learn. The SOUL language is much simpler and more intuitive. Browser-based tools and fast live testing make it even easier to use and master.

- More secure: Sandboxing untrusted third-party code adds performance overheads that are a particular problem for audio coding. SOUL uses an intermediate assembly language called HEART that is safe to run without sandboxing, making SOUL code far more secure.

- Optimized for power and performance: CPU-intensive power consumption is a constant problem for developers. The SOUL language is far more efficient. It is specifically designed for performance, with its JIT engine matching an equivalent program written in native C++.

- Achieves lowest latency: Latency in audio apps is another deep-rooted issue that to date is solved by running audio on dedicated audio hardware. SOUL unlocks ultra-low latency on devices without the need for embedded CPUs and DSPs.


Since debuting SOUL at ADC 2018, the team has continuously welcomed feedback through the open-source repository at https://soul.dev. This feedback has helped SOUL reach its V1.0 state today.

The SOUL team encourages developers to explore the language on soul.dev, read the documentation on the SOUL repository on Github, and give more feedback as the SOUL toolset continues to grow this year.

---

#### Further reading:

- [Project Overview](https://soul-lang.github.io/SOUL/docs/SOUL_Overview.html)
- [FAQ](https://soul-lang.github.io/SOUL/docs/SOUL_FAQ.html)
- [Language Guide](https://soul-lang.github.io/SOUL/docs/SOUL_Language.html)
- [Library Reference](https://soul-lang.github.io/SOUL/docs/soul_library.html)
- [Code Examples](https://soul.dev/examples)
