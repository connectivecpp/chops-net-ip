# Chops - Connective Handcrafted Openwork Software Medley

The Chops Medley is a collection of C++ libraries oriented towards networking and distributed processing. It is written using modern C++ design idioms and the latest (2017) C++ standard.

# Chops Libraries

## Chops Net

Chops Net is an asynchronous general purpose networking library layered on top of the C++ Networking Technical Standard (TS). It has three main goals:

- Encapsulate and simplify the (sometimes complex) details of asynchronous network programming. Managing buffer lifetimes can be tricky, dkljflk, and lsdkfj, and this library makes sure it is done correctly.
- Abstracting 
- sdlkfj

## Chops Wait Queue

Chops Wait Queue is a multi-reader, multi-writer queue for transferring data between threads. It is templatized on the type of data passed through the queue as well as the queue container type. Data is passed with value semantics, either by copying or by moving (as opposed to a queue that transfers data by pointer or reference). The wait queue has both wait and no-wait pop semantics, as well as simple "close" and "open" capabilities (to allow graceful shutdown or restart of thread or process communication). It uses C++ standard library 

# C++ Language Requirements and Alternatives

A significant number of C++ 11 features are used, while only basic C++ 14 and 17 features are used (e.g. `std::optional`, `std::byte`, simple usages of structured bindings, generic lambdas). For users that don't want to use the latest C++ compilers or compile with C++ 17 flags, Martin Moene has written an excellent set of header-only libraries that implement many useful C++ library features, both C++ 17 as well as future C++ standards. These include `std::optional`, `std::variant`, `std::any`, and `std::byte` (from C++ 17) as well as `std::ring_span` (C++ 20, most likely). He also has multiple other repositories including an implementation of the C++ Guideline Support Library (GSL). Martin's repositories are available at https://github.com/martinmoene.

Using Boost libraries instead of `std::optional` (and similar C++ 17 features) is also an option, and would require minimal porting.

While the main production branch of Chops will always be developed and tested with C++ 17 features (and the latest compilers), alternative branches and forks for older compiler versions are expected. In particular, a branch using Martin's libraries and general C++ 11 (or C++ 14) conformance is expected for the future, and collaboration (through forking, change requests, etc) is 
very welcome. A branch supporting a pre-C++ 11 compiler or language conformance is not likely to be directly supported through this repository (since it would require so many changes as to be almost a different codebase).

# Dependencies

The libraries and API's have minimal library dependencies. Currently the non-test code depends on the standard C++ library and Chris Kohlhoff's Asio library. Asio is available at https://think-async.com/ as well as https://github.com/chriskohlhoff/. Asio forms the basis for the C++ Networking Technical Standard (TS), which will (almost surely) be standardized in C++ 20. Currently the Chops Net library uses the `networking-ts-impl` repository from Chris' Github account.

The test suites have additional dependencies, including Phil Nash's Catch 2.0 for the unit test framework. The Catch library is available at https://github.com/catchorg/Catch2.

# Supported Compilers

Chops is compiled and tested on g++ 7.2, clang xx, and VS xx.

## Author Note on Naming

>Yes, the name / acronym is a stretch. Quite a stretch. I like the word "chops", which is a jazz term for strong technique, so I decided on that for a name. For example, "Check out Tal Wilkenfeld, she's got mad chops."

>I considered many names, at least two or three dozen, but my favorite ideas were all taken (or too close to existing names and could create confusion). It seems that a lot of software developers have similar creative ideas when it comes to names.

>The crucial word in the acronym is "Connective". Most of my software engineering career has involved networking or distributed software and this collection of libraries and utilities is centered on that theme. Making it easier to write high performing networked applications, specially in C++, is one of my goals as someone writing infrastructure and core utility libraries.

>The second word of the acronym is just a "feel good" word for me. Most software is handcrafted, so there's nothing distinctive about my use of the term. I'm using the word because I like it, plus it has good mental associations for me with things like "handcrafted beer" (yum), or "handcrafted furniture". Handcrafted also implies quality, and I believe this software excels in in its implementation and design.

>I originally was going to use the term "openhearted" as part of this projects name, but decided that "openwork" is a nice alternate form of "open source" and is a little more technically consistent. The dictionary says that "openwork" is work constructed so as to show openings through its substance (such as wrought-iron openwork or embroidery, and is typically ornamental), so my use of the term is not exactly correct. I don't care, I still like the way I use it.

>Even though I didn't use "openhearted" in the name, it's an aspect that I aspire to in every part of my life. I fall short, very often, but not because of lack of effort or desire.

>I picked "medley" because I think it is more representative than "suite", which (to me) implies a cohesive set of libraries, or possibly a framework.

>I mentioned Tal earlier in this note, so here are two representative YouTube videos of her playing with Jeff Beck, who I consider one of the all time best guitarists in the world. The first video is more rock and funk, the second is the two of them playing one of my favorite fusion ballads: https://www.youtube.com/watch?v=jVb-izZVCwQ, https://www.youtube.com/watch?v=blp7hPFaIfU

>And since I'm on a music digression, an artist I often listen to while writing code is Helios. His music is instrumental, space music'ey, ambient, contains a nice amount of harmonic and rhythmic complexity, and is very melodic. Most important it doesn't interfere with my concentration. A good album to start with is Eingya: https://www.youtube.com/watch?v=fud-Lz76MHg
