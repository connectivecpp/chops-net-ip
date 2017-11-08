# Chops - Connective Handcrafted Openwork Software Medley

The Chops Medley is a collection of C++ libraries oriented towards networking and distributed processing. It is written using modern C++ design idioms and the latest (2017) C++ standard.

# Chops Libraries

## Chops Net

Chops Net is an asynchronous general purpose networking library layered on top of the C++ Networking Technical Standard (TS). It has three main goals:

- Encapsulate and simplify the (sometimes complex) details of asynchronous network programming. Managing buffer lifetimes can be tricky, dkljflk, and lsdkfj, and this library makes sure it is done correctly.
- Abstracting 
- sdlkfj

## Chops Wait Queue

Chops Wait Queue is a multi-reader, multi-writer FIFO queue for transferring data between threads. It is templatized on the type of data passed through the queue as well as the queue container type. Data is passed with value semantics, either by copying or by moving (as opposed to a queue that transfers data by pointer or reference). The wait queue has both wait and no-wait pop semantics, as well as simple "close" and "open" capabilities (to allow graceful shutdown or restart of thread or process communication).

Multiple writer and reader threads can access a Wait Queue simultaneously, although when a value is pushed on the queue, only one reader thread will be notified to consume the value.

Close sementics are simple, and consist of setting an internal flag and notifying all waiting reader threads. Subsequent pushes are disallowed (an error is returned on the push). On close, if the queue is non-empty data is not flushed (elements in the queue will be destructed when the Wait Queue object is destructed, as typical in C++).

Wait Queue uses C++ standard library concurrency facilities (mutex, condition variables) in its implementation. It is not a lock-free queue, but it has been designed to be used in memory constrained or performance critical environments. In particular, Wait Queue:

- Has been tested with Martin Moene's `ring_span` library for the internal container (see the Language Requirements and Alternatives section for more details). This implies that the Wait Queue can be used in environments where dynamic memory management (heap) is not allowed or is problematic. In particular, no heap memory is directly allocated within the Wait Queue.

- Does not throw or catch exceptions anywhere in its code base. 

The implementation is adapted from the book Concurrency in Action, Practical Multithreading, by Anthony Williams. Anthony is a recognized expert in concurrency including Boost Thread and C++ standards efforts. His web site is http://www.justsoftwaresolutions.co.uk. It is highly recommended to buy his book, whether in paper or electronic form, and Anthony is busy at work on a second edition (covering C++ 14 and C++ 17 concurrency facilities) that is available in pre-release form.

The core logic in this library is the same as provided by Anthony in his book, but the API has changed and additional features added. The name of the utility class template in Anthony's book is `threadsafe_queue`.

# Chops C++ Language Requirements and Alternatives

A significant number of C++ 11 features are used, while only basic C++ 14 and 17 features are used (e.g. `std::optional`, `std::byte`, simple usages of structured bindings, generic lambdas). For users that don't want to use the latest C++ compilers or compile with C++ 17 flags, Martin Moene has written an excellent set of header-only libraries that implement many useful C++ library features, both C++ 17 as well as future C++ standards. These include `std::optional`, `std::variant`, `std::any`, and `std::byte` (from C++ 17) as well as `std::ring_span` (C++ 20, most likely). He also has multiple other useful repositories including an implementation of the C++ Guideline Support Library (GSL). Martin's repositories are available at https://github.com/martinmoene.

Using Boost libraries instead of `std::optional` (and similar C++ 17 features) is also an option, and should require minimal porting.

While the main production branch of Chops will always be developed and tested with C++ 17 features (and the latest compilers), alternative branches and forks for older compiler versions are expected. In particular, a branch using Martin's libraries and general C++ 11 (or C++ 14) conformance is expected for the future, and collaboration (through forking, change requests, etc) is 
very welcome. A branch supporting a pre-C++ 11 compiler or language conformance is not likely to be directly supported through this repository (since it would require so many changes that it would result in a defacto different codebase).

# Dependencies

The libraries and API's have minimal library dependencies. Currently the non-test code depends on the standard C++ library and Chris Kohlhoff's Asio library. Asio is available at https://think-async.com/ as well as https://github.com/chriskohlhoff/. Asio forms the basis for the C++ Networking Technical Standard (TS), which will (almost surely) be standardized in C++ 20. Currently the Chops Net library uses the `networking-ts-impl` repository from Chris' Github account.

The test suites have additional dependencies, including Phil Nash's Catch 2.0 for the unit test framework. The Catch library is available at https://github.com/catchorg/Catch2. The Chops Wait Queue tests use Martin Moene's `ring_span` library for fixed buffer queue semantics.

# Supported Compilers

Chops is compiled and tested on g++ 7.2, clang xx, and VS xx.

## Author Note on Naming

>Yes, the name / acronym is a stretch. Quite a stretch. I like the word "chops", which is a jazz term for strong technique, so I decided on that for a name. For example, "Check out Tal Wilkenfeld, she's got mad chops."

>I considered many names, at least two or three dozen, but my favorite ideas were all taken (or too close to existing names and could create confusion). It seems that a lot of software developers have similar creative ideas when it comes to names.

>The crucial word in the acronym is "Connective". Most of my software engineering career has involved networking or distributed software and this collection of libraries and utilities is centered on that theme. Making it easier to write high performing networked applications, specially in C++, is one of my goals as someone writing infrastructure and core utility libraries.

>The second word of the acronym is just a "feel good" word for me. Most software is handcrafted, so there's nothing distinctive about my use of the term. I'm using the word because I like it, plus it has good mental associations for me with things like "handcrafted beer" (yum), or "handcrafted furniture" (good stuff). Handcrafted also implies quality, and I believe this software excels in its implementation and design.

>I originally was going to use the term "openhearted" as part of the project name, but decided that "openwork" is a nice alternate form of "open source" and is a little more technically consistent. The dictionary says that "openwork" is work constructed so as to show openings through its substance (such as wrought-iron openwork or embroidery, and is typically ornamental), so my use of the term is not exactly correct. I don't care, I still like the way I use it.

>Even though I didn't use "openhearted" in the name, it's an aspect that I aspire to in every part of my life. I fall short, very often, but not because of lack of effort or desire.

>I picked "medley" because I think it is more representative than "suite", which (to me) implies a cohesive set of libraries, or possibly a framework.

>I mentioned Tal earlier in this note, so here are two representative YouTube videos of her playing with Jeff Beck, who I consider one of the all time best guitarists. The first video is a blend of jazz, rock, and funk, the second is the two of them playing one of my favorite rock / jazz fusion ballads: https://www.youtube.com/watch?v=jVb-izZVCwQ, https://www.youtube.com/watch?v=blp7hPFaIfU

>And since I'm on a music digression, an artist I often listen to while writing code is Helios. His music is instrumental, space music'ey, ambient, contains a nice amount of harmonic and rhythmic complexity, and is very melodic. Most important it doesn't interfere with my concentration. A good album to start with is Eingya: https://www.youtube.com/watch?v=fud-Lz76MHg
