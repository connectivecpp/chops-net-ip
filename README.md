# Chops - Connective Handcrafted Openwork Software Medley

The Chops Medley is a collection of C++ libraries for networking and distributed processing. It is written using modern C++ design idioms and the latest (2017) C++ standard.

# Chops Libraries

## Chops Net

### Overview

Chops Net is an asynchronous general purpose networking library layered on top of the C++ Networking Technical Standard (TS). It is designed to simplify application code for processing data on multiple simultaneous connections or endpoints in an asynchronous, efficient manner. Every application interaction with Chops Net operations is no-wait (i.e. there are no blocking methods) and all network processing operations are performed asynchronously.

Example environments where Chops Net is a good fit:

- Applications interacting with multiple connections (e.g. handling multiple sensors or inputs or outputs), each with low to moderate throughput needs (i.e. IoT environments, chat networks, gaming networks).
- Small footprint or embedded environments, where all network processing is run inside a single thread.
- Applications with relatively simple network processing that need an easy-to-use and quick-for-development network library.
- Applications with configuration driven networks that may need to switch (for example) between connect versus accept for a given connection, or between TCP and UDP for a given communication path.

Chops Net:

- simplifies the creation of various IP (Internet Protocol) networking entities including TCP acceptors and connectors, UDP senders and receivers, and UDP multicast senders and receivers.
- simplifies the resolution of network names to IP addresses (i.e. domain name system lookups).
- abstracts the concept of message handling in TCP (Transmission Control Protocol) and provides customization points in two areas:
  1. message framing, which is the code and logic that determines the begin and end of a message within the TCP byte stream.
  2. message processing, which is the code and logic that processes a message when the framing determines a complete message has arrived.
- provides buffer lifetime management for outgoing data.
- provides customization points for state changes in the networking entities, including:
  - a connection has become active or inactive (TCP acceptors and connectors).
  - an endpoint has been created or destroyed (UDP).
  - an error has occurred.
- implements the "plumbing" for asynchronous processing on multiple simultaneous connections.
- abstracts many differences between network protocols (TCP, UDP, UDP multicast), allowing easier application transitioning between protocol types.
- allows the application to control threading (no threads are created or managed inside Chops Net).
- is agnostic with respect to data marshalling or serialization or "wire protocols" (application code provides any and all data marshalling and endian logic).
- does not impose any structure on network message content.

Chops Net is designed to make it easy and efficient for an application to create hundreds or thousands of network connections and handle them simultaneously. In particular, there are no threads or thread pools within Chops Net, and it works well with only one application thread invoking the event loop (an executor, in current C++ terminology).

A detailed overview is [available here](doc/chops_net.md).

## Timer

### Periodic Timer

The Periodic Timer class is an asynchronous periodic timer that adjusts for time jitter.

Writing code using asynchronous timers from the C++ Networking Technical Specification (TS) is relatively easy. However, there are no timers that are periodic. This class simplifies the task, using application supplied function object callbacks.

A detailed overview is [available here](doc/timer.md).

## Chops Utilities

### Repeat

Repeat is a function template to abstract and simplify loops that repeat N times, from Vittorio Romeo (see [References Section](#references)). The C++ range based `for` doesn't directly allow N repetitions of code. Vittorio's utility fills that gap.

### Wait Queue

Wait Queue is a multi-reader, multi-writer FIFO queue for transferring data between threads. It is templatized on the type of data passed through the queue as well as the queue container type. Data is passed with value semantics, either by copying or by moving (as opposed to a queue that transfers data by pointer or reference). The wait queue has both wait and no-wait pop semantics, as well as simple "close" and "open" capabilities (to allow graceful shutdown or restart of thread or process communication). A fixed size container (e.g. a `ring_span`) can be used, eliminating any and all dynamic memory management (useful in embedded or performance constrained environments).

A detailed overview of the utilities is [available here](doc/utility.md).

# C++ Language Requirements and Alternatives

A significant number of C++ 11 features are in the implementation and API. There are also limited C++ 14 and 17 features in use, although they tend to be relatively simple features of those standards (e.g. `std::optional`, `std::byte`, structured bindings). For users that don't want to use the latest C++ compilers or compile with C++ 17 flags, Martin Moene provides an excellent set of header-only libraries that implement many useful C++ library features, both C++ 17 as well as future C++ standards (see [References Section](#references)).

Using Boost libraries instead of `std::optional` (and similar C++ 17 features) is also an option, and should require minimal porting.

While the main production branch of Chops will always be developed and tested with C++ 17 features (and relatively current compilers), alternative branches and forks for older compiler versions are expected. In particular, a branch using Martin's libraries and general C++ 11 (or C++ 14) conformance is expected for the future, and collaboration (through forking, change requests, etc) is very welcome. A branch supporting a pre-C++ 11 compiler or language conformance is not likely to be directly supported through this repository (since it would require so many changes that it would result in a defacto different codebase).

# Dependencies

The libraries and API's have minimal library dependencies. Currently the non-test code depends on the standard C++ library and Chris Kohlhoff's `networking-ts-impl` library (see [References Section](#references)).

The test suites have additional dependencies, including Phil Nash's Catch 2.0 for the unit test framework (see [Reference Section](#references)). Various tests for templatized queue container types use Martin Moene's `ring_span` library for fixed buffer queue semantics.

# References

- Chris Kohlhoff is a networking and C++ expert, creator of the Asio library and initial author of the C++ Networking Technical Standard (TS). Asio is available at https://think-async.com/ and Chris' Github site is https://github.com/chriskohlhoff/. Asio forms the basis for the C++ Networking Technical Standard (TS), which will (almost surely) be standardized in C++ 20. Currently the Chops Net library uses the `networking-ts-impl` repository from Chris' Github account.

- Phil Nash is the author of the Catch C++ unit testing library. The Catch library is available at https://github.com/catchorg/Catch2.

- Anthony Williams is the author of Concurrency in Action, Practical Multithreading. His web site is http://www.justsoftwaresolutions.co.uk and his Github site is https://github.com/anthonywilliams. Anthony is a recognized expert in concurrency including Boost Thread and C++ standards efforts. It is highly recommended to buy his book, whether in paper or electronic form, and Anthony is busy at work on a second edition (covering C++ 14 and C++ 17 concurrency facilities) now available in pre-release form.

- Martin Moene is a C++ expert and member and former editor of accu-org, His Github site is https://github.com/martinmoene. Martin provides an excellent set of header-only libraries that implement many useful C++ library features, both C++ 17 as well as future C++ standards. These include `std::optional`, `std::variant`, `std::any`, and `std::byte` (from C++ 17) as well as `std::ring_span` (C++ 20, most likely). He also has multiple other useful repositories including an implementation of the C++ Guideline Support Library (GSL). 

- Kirk Shoop is a C++ expert, particularly in the area of asynchronous design, and has presented multiple times at CppCon. His Github site is https://github.com/kirkshoop.

- Vittorio Romeo is a blog author and C++ expert. His web site is https://vittorioromeo.info/ and his Github site is https://github.com/SuperV1234. Vittorio's blog is excellent and well worth reading.

# Supported Compilers and Platforms

Chops has been compiled on g++ 7.2, clang xx, VS xx and tested on Linux (4.13), Windows 10, and xx.

# Installation

All Chops libraries are header-only, so installation consists of downloading or cloning and setting compiler include paths appropriately. No compile time configuration macros are defined in Chops.

# About

The primary author of Chops is Cliff Green, softwarelibre at codewrangler dot net. Cliff is a software engineer and has worked for years with networked and distributed systems, wireless networks, location technology, and large scale embedded systems in the military aerospace industry. He has volunteered every year at CppCon and presented at BoostCon (before it was renamed to C++ Now).

Co-authors include ...

Contributors include ...

Additional information including various author notes is [available here](doc/about.md).

