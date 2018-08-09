# Chops Net IP - Connective Handcrafted Openwork Software for Asynchronous IP Networking

Chops Net IP is a C++ library that makes asynchronous networking programming fun. Or at least if not fun, it makes network programming easier and safer without significantly sacrificing performance. It is built on top of the C++ Networking Technical Standard (TS) and handles Internet Protocol (IP) communications (TCP, UDP, UDP multicast). It is written using modern C++ design idioms and the latest (2017) C++ standard.

This project is distributed under the [Boost Software License](LICENSE.txt).

## Chops Net IP Release Status

Release 1.0 is under development as of August 2018, awaiting `std::expected` (proposed for C++ 20) error handling improvement, CMake config file completion, and additional testing under multiple compilers and platforms.

Release notes and upcoming development plans are [available here](doc/release.md).

# Overview

For many developers, asynchronous network programming in C++ is not easy. It is complex, has many pitfalls, and requires designing C++ code in a way that is not natural for many developers, even those with years of experience. Chops Net IP ("C"onnective "H"andcrafted "Op"enwork "S"oftware, Networking over Internet Protocol) simplifies asynchronous network programming and provides useful (and tasty!) abstractions for many types of communication patterns.

Chops Net IP is layered on top of the C++ Networking Technical Standard (TS). This allows it to be portable across many compilers and platforms and when the C++ Networking TS is standardized (C++ 20? We all hope!) Chops Net IP will directly use the networking facilities of the C++ standard library.

Chops Net IP simplifies application code that processes data on multiple simultaneous TCP connections or UDP endpoints. All Chops Net IP operations (from the application viewpoint) are no-wait (i.e. there are no blocking methods) and all network processing operations are performed asynchronously.

## Tasty Bites

Chops Net IP functionality:

- simplifies the creation of various IP (Internet Protocol) networking entities including TCP acceptors and connectors, UDP senders and receivers, and UDP multicast senders and receivers.
- simplifies the resolution of network names to IP addresses (i.e. domain name system lookups).
- abstracts message concepts in TCP (Transmission Control Protocol) and provides customization points in two areas:
  1. message framing, which is the code and logic that determines the begin and end of a message within the TCP byte stream.
  2. message processing, which is the code and logic that processes a message when the framing determines a complete message has arrived.
- provides buffer lifetime management for outgoing data.
- provides customization points for state changes in the networking entities, including:
  - a TCP connection has become active and is ready for input and output.
  - a UDP endpoint has been created and is ready for input and output.
  - a TCP connection has been destroyed or a UDP socket has closed.
- implements the "plumbing" for asynchronous processing on multiple simultaneous connections.
- abstracts many differences between network protocols (TCP, UDP, UDP multicast), allowing easier application transitioning between protocol types.
- allows the application to control threading (no threads are created or managed inside Chops Net IP).
- is agnostic with respect to data marshalling or serialization or "wire protocols" (application code provides any and all data marshalling and endian logic).
- does not impose any structure on network message content.

Chops Net IP is designed to make it easy and efficient for an application to create hundreds (or thousands) of network connections and handle them simultaneously. In particular, there are no threads or thread pools within Chops Net IP, and it works well with only one application thread invoking the event loop (an executor, in current C++ terminology).

## Tasty Uses

Example environments where Chops Net IP is a good fit:

- Applications that are event driven or highly asynchronous in nature.
- Applications where data is generated and handled in a non-symmetric manner. For example, data may be generated on the TCP acceptor side, or may be generated on a TCP connector side, or on both sides depending on the use case. Similarly, applications where the data flow is bi-directional and sends or receives are data-driven versus pattern-driven work well with this library.
- Applications interacting with multiple (many) connections (e.g. handling multiple sensors or inputs or outputs), each with low to moderate throughput needs (i.e. IoT environments, chat networks, gaming networks).
- Small footprint or embedded environments, where all network processing is run inside a single thread. In particular, environments where a JVM (or similar run-time support) is too costly in terms of system resource, but have a relatively rich operating environment (e.g. Linux running on a small chip) are a very good fit. (Currently the main constraint is small system support in the Networking TS implementation.)
- Applications with relatively simple network processing that need an easy-to-use and quick-for-development networking library.
- Applications with configuration driven networks that may need to switch (for example) between TCP connect versus TCP accept for a given connection, or between TCP and UDP for a given communication path.
- Peer-to-peer applications where the application doesn't care which side connects or accepts.
- Frameworks or groups of applications where abstracting wire-protocol logic from message processing logic makes sense.

## A Short Example

(fill in code here)

## Want More?

A detailed overview, a C++ socket library comparison, and a FAQ is [available here](doc/net_ip/overview.md).

# C++ Language Requirements and Alternatives

C++ 17 is the primary baseline for this repository.

A significant number of C++ 11 features are in the implementation and API. There are also limited C++ 14 and 17 features in use, although they tend to be relatively simple features of those standards (e.g. `std::byte`, structured bindings). For users that don't want to use the latest C++ compilers or compile with C++ 17 flags, Martin Moene provides an excellent set of header-only libraries that implement many useful C++ library features, both C++ 17 as well as future C++ standards (see [References](doc/references.md)).

While the main production branch will always be developed and tested with C++ 17 features (and relatively current compilers), alternative branches and forks for older compiler versions are expected. In particular, a branch using Martin's libraries and general C++ 11 (or C++ 14) conformance is expected for the future, and collaboration (through forking, change requests, etc) is very welcome. A branch supporting a pre-C++ 11 compiler or language conformance is not likely to be directly supported through this repository (since it would require so many changes that it would result in a defacto different codebase).

# External Dependencies

The libraries and API's have minimal (as possible) library dependencies (there are heavy dependencies on the C++ standard library in all of the code). There are more dependencies in the test code than in the production code.

Production external dependencies:

- Version 1.11 (or later) of Chris Kohlhoff's `networking-ts-impl` (Networking TS) repository is required for some components. Note that the version number is from the Asio version and may not exactly match the Networking TS version.
- Version 0.9 (or later) of `utility-rack`, which is a repository in the same account as Chops Net IP (`shared_buffer.hpp` is required).
- Version 0.1 (or later) of Martin Moene's `expected-lite` library.

Test external dependencies:

- Version 2.1.0 (or later) of Phil Nash's Catch 2 is required for all test scenarios.
- Version 1.65.1 (or later) of the Boost library is required in some test scenarios (`boost.endian` at minimum).

See [Reference Section](#references) for additional details on the above libraries.

# Supported Compilers and Platforms

Chops Net IP has been compiled and tests run on:

- g++ 7.2, Linux (Ubuntu 17.10, Linux kernel 4.13)
- (TBD, will include at least clang on linux and vc++ on Windows)

# Installation

Chops Net IP is header-only, so installation consists of downloading or cloning and setting compiler include paths appropriately. No compile time configuration macros are defined.

# References

See [References](doc/references.md) for details on dependencies and inspirations for Chops Net IP.

# About

The primary author of Chops is Cliff Green, softwarelibre at codewrangler dot net.

Co-authors include ...

Contributors include ...

Additional information including author comments is [available here](doc/about.md).

