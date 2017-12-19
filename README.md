# Chops - Connective Handcrafted Openwork Software Medley

The Chops Medley is a collection of C++ libraries for networking and distributed processing and simulation. It is written using modern C++ design idioms and the latest (2017) C++ standard.

This project is licensed under the terms of the MIT license.

# Chops Major Components

## Chops Net IP

### Overview

Chops Net IP is an asynchronous general purpose networking library layered on top of the C++ Networking Technical Standard (TS) handling Internet Protocol (IP) communications. It is designed to simplify application code for processing data on multiple simultaneous connections or endpoints in an asynchronous, efficient manner. Every application interaction with Chops Net IP operations is no-wait (i.e. there are no blocking methods) and all network processing operations are performed asynchronously.

Example environments where Chops Net IP is a good fit:

- Applications that are event driven or highly asynchronous in nature.
- Applications where data is generated and handled in a non-symmetric manner. Similarly, applications where the data flow is bi-directional and sends or receives are data-driven versus pattern-driven work well with this library.
- Applications interacting with multiple (many) connections (e.g. handling multiple sensors or inputs or outputs), each with low to moderate throughput needs (i.e. IoT environments, chat networks, gaming networks).
- Small footprint or embedded environments, where all network processing is run inside a single thread.
- Applications with relatively simple network processing that need an easy-to-use and quick-for-development networking library.
- Applications with configuration driven networks that may need to switch (for example) between connect versus accept for a given connection, or between TCP and UDP for a given communication path.
- Peer-to-peer applications where the application doesn't care which side connects or accepts, only that there's a data connection.
- Frameworks or groups of applications where abstracting wire-protocol code from message processing code makes sense.

Chops Net IP:

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
- allows the application to control threading (no threads are created or managed inside Chops Net IP).
- is agnostic with respect to data marshalling or serialization or "wire protocols" (application code provides any and all data marshalling and endian logic).
- does not impose any structure on network message content.

Chops Net IP is designed to make it easy and efficient for an application to create hundreds or thousands of network connections and handle them simultaneously. In particular, there are no threads or thread pools within Chops Net IP, and it works well with only one application thread invoking the event loop (an executor, in current C++ terminology).

A detailed overview is [available here](doc/net_ip.md).

## Future Components

### MQTT

(More info to be added.)

### Discrete Event Sim

(More info to be added.)

### Bluetooth

(More info to be added.)

### Serial

(More info to be added.)

# Chops Minor Components

## Timer

### Periodic Timer

The Periodic Timer class is an asynchronous periodic timer that wraps and simplifies C++ Networking Technical Standard (TS) timers when periodic callbacks are needed. The periodicity can be based on either a simple duration or on timepoints based on a duration.

Asynchronous timers from the C++ Networking Technical Specification (TS) are relatively easy to use. However, there are no timers that are periodic. This class simplifies the usage, using application supplied function object callbacks. When the timer is started, the application specifies whether each callback is invoked based on a duration (e.g. one second after the last callback), or on timepoints (e.g. a callback will be invoked each second according to the clock).

A detailed overview is [available here](doc/timer.md).

## Queue

### Wait Queue

Wait Queue is a multi-reader, multi-writer FIFO queue for transferring data between threads. It is templatized on the type of data passed through the queue as well as the queue container type. Data is passed with value semantics, either by copying or by moving (as opposed to a queue that transfers data by pointer or reference). The wait queue has both wait and no-wait pop semantics, as well as simple "close" and "open" capabilities (to allow graceful shutdown or restart of thread or process communication). A fixed size container (e.g. a `ring_span`) can be used, eliminating any and all dynamic memory management (useful in embedded or performance constrained environments). Similarly, a circular buffer that only allocates on construction can be used, which eliminates dynamic memory management when pushing values on to the queue.

Wait Queue is inspired by code from Anthony Williams' Concurrency in Action book (see [References Section](#references)), although heavily modified.

A detailed overview is [available here](doc/queue.md).

## Utilities

### Repeat

Repeat is a function template to abstract and simplify loops that repeat N times, from Vittorio Romeo (see [References Section](#references)). The C++ range based `for` doesn't directly allow N repetitions of code. Vittorio's utility fills that gap.

### Make Byte Array

Since `std::byte` pointers are used as a general buffer interface, a small utility function from Blitz Rakete as posted on Stackoverflow (see [References Section](#references)) is useful to simplify creation of byte buffers, specially for testing purposes.

### Shared Buffer

Reference counted byte buffer classes are used within Chops Net IP, but can be useful in other contexts. These classes are based on example code inside Chris Kohlhoff's Asio library (see [References Section](#references)).

A detailed overview of the utility classes is [available here](doc/utility.md).

# C++ Language Requirements and Alternatives

A significant number of C++ 11 features are in the implementation and API. There are also limited C++ 14 and 17 features in use, although they tend to be relatively simple features of those standards (e.g. `std::optional`, `std::byte`, structured bindings). For users that don't want to use the latest C++ compilers or compile with C++ 17 flags, Martin Moene provides an excellent set of header-only libraries that implement many useful C++ library features, both C++ 17 as well as future C++ standards (see [References Section](#references)).

Using Boost libraries instead of `std::optional` (and similar C++ 17 features) is also an option, and should require minimal porting.

While the main production branch of Chops will always be developed and tested with C++ 17 features (and relatively current compilers), alternative branches and forks for older compiler versions are expected. In particular, a branch using Martin's libraries and general C++ 11 (or C++ 14) conformance is expected for the future, and collaboration (through forking, change requests, etc) is very welcome. A branch supporting a pre-C++ 11 compiler or language conformance is not likely to be directly supported through this repository (since it would require so many changes that it would result in a defacto different codebase).

# External Dependencies

The libraries and API's have minimal (as possible) library dependencies (there are heavy dependencies on the C++ standard library in all of the code). There are more dependencies in the test code than in the production code. The external dependencies:

- Version 1.11 (or later) of Chris Kohlhoff's `networking-ts-impl` (Networking TS) repository is required for some components. Note that the version number is from the Asio version and may not exactly match the Networking TS version.
- Version 2.01 (or later) of Phil Nash's Catch 2.0 is required for all test scenarios.
- Version 0.00 (or later) of Martin's Ring Span Lite is required for some test scenarios.
- Version 1.65.1 or 1.66.0 of the Boost library (specific usages below).

See [Reference Section](#references)) for additional details on the above libraries.

Specific dependencies:

- All test scenarios: Catch 2.0
- Chops Net IP (production): `networking-ts-impl`
  - Boost.Endian (test)
- Wait Queue (production): none
  - Ring Span Lite (test)
  - Boost.Circular (test)
- Periodic Timer (production): `networking-ts-impl`
- Shared Buffer (production): none

# References

- Chris Kohlhoff is a networking expert (among other expertises, including C++), creator of the Asio library and initial author of the C++ Networking Technical Standard (TS). Asio is available at https://think-async.com/ and Chris' Github site is https://github.com/chriskohlhoff/. Asio forms the basis for the C++ Networking Technical Standard (TS), which will (almost surely) be standardized in C++ 20. Currently the Chops Net IP library uses the `networking-ts-impl` repository from Chris' Github account.

- Phil Nash is the author of the Catch C++ unit testing library. The Catch library is available at https://github.com/catchorg/Catch2.

- Anthony Williams is the author of Concurrency in Action, Practical Multithreading. His web site is http://www.justsoftwaresolutions.co.uk and his Github site is https://github.com/anthonywilliams. Anthony is a recognized expert in concurrency including Boost Thread and C++ standards efforts. It is highly recommended to buy his book, whether in paper or electronic form, and Anthony is busy at work on a second edition (covering C++ 14 and C++ 17 concurrency facilities) now available in pre-release form.

- The Boost libraries collection is a high quality set of C++ libraries, available at http://www.boost.org/.

- Martin Moene is a C++ expert and member and former editor of accu-org. His Github site is https://github.com/martinmoene. Martin provides an excellent set of header-only libraries that implement many useful C++ library features, both C++ 17 as well as future C++ standards. These include `std::optional`, `std::variant`, `std::any`, and `std::byte` (from C++ 17) as well as `ring_span` (C++ 20, most likely). He also has multiple other useful repositories including an implementation of the C++ Guideline Support Library (GSL). 

- Andrzej Krzemieński is a C++ expert and proficient blog author. His very well written blog is https://akrzemi1.wordpress.com/ and a significant portion of the Chops code is directly influenced by it.

- Bjørn Reese writes about many topics in C++ at the expert level and is active with the Boost organization. His blog is http://breese.github.io/blog/.

- Kirk Shoop is a C++ expert, particularly in the area of asynchronous design, and has presented multiple times at CppCon. His Github site is https://github.com/kirkshoop.

- Vittorio Romeo is a blog author and C++ expert. His web site is https://vittorioromeo.info/ and his Github site is https://github.com/SuperV1234. Vittorio's blog is excellent and well worth reading.

- Blitz Rakete is a student software developer who has the user id of Rakete1111 on many forums and sites (including Stackoverflow). His Github site is https://github.com/Rakete1111.

# Supported Compilers and Platforms

Chops has been compiled and tests run on:

- g++ 7.2, Linux (Ubuntu 17.10, Linux kernel 4.13)
- (TBD, will include at least clang on linux and vc++ on Windows)

# Installation

All Chops libraries are header-only, so installation consists of downloading or cloning and setting compiler include paths appropriately. No compile time configuration macros are defined in Chops.

# About

The primary author of Chops is Cliff Green, softwarelibre at codewrangler dot net. Cliff is a software engineer and has worked for years writing infrastructural libraries and applications for use in networked and distributed systems, typically where high reliability or uptime is required. The domains where he has worked include wireless networks, location technology, and large scale embedded and simulation systems in the military aerospace industry. He has volunteered every year at CppCon and presented at BoostCon (before it was renamed to C++ Now).

Cliff lives in the Seattle area and you may know him from other interests including volleyball, hiking, railroading (both the model variety and the big ones in real life), music, or even parent support activities (if you are having major difficulties with your teen check out the Changes Parent Support Network, http://cpsn.org).

Co-authors include ...

Contributors include ...

Additional information including various author notes is [available here](doc/about.md).

