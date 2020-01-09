Branch | Status
|---|---|
**Master** | [![Build Status](https://travis-ci.org/connectivecpp/chops-net-ip.svg?branch=master)](https://travis-ci.org/connectivecpp/chops-net-ip)
**Develop** | [![Build Status](https://travis-ci.org/connectivecpp/chops-net-ip.svg?branch=develop)](https://travis-ci.org/connectivecpp/chops-net-ip)


# Chops Net IP - Connective Handcrafted Openwork Software for Asynchronous IP Networking

Chops Net IP is a C++ library that makes asynchronous networking programming fun. Or at least if not fun, it makes network programming easier and safer, without significantly sacrificing performance. Chops Net IP handles Internet Protocol (IP) communications including TCP, UDP, and UDP multicast. It is written using modern C++ design idioms and the latest (2017) C++ standard.

Chops Net IP is not like any other high-level, general purpose C++ socket library.

Chops Net IP is layered on top of the Asio asynchronous networking library, taking advantage of the portability and functionality that Asio provides. However, it simplifies network programming compared to coding against the Asio API, while providing easy scalability through the asynchronous facilities.

# License

[![Licence](https://img.shields.io/badge/license-boost-4480cc.svg)](http://www.boost.org/LICENSE_1_0.txt)

This project is distributed under the [Boost Software License](LICENSE.txt).

## Chops Net IP Release Status

Release 1.0 is under development as of January 2020, awaiting CMake enhancements, a comprehensive tutorial, and additional testing under multiple compilers and platforms.

Release notes and upcoming development plans are [available here](doc/release.md).

# Overview

For many software developers, asynchronous network programming in C++ is not easy. It is complex, has many pitfalls, and requires designing C++ code in a way that is not natural, even for those with years of experience. Chops Net IP ("C"onnective "H"andcrafted "Op"enwork "S"oftware, Networking over Internet Protocol) simplifies asynchronous network programming and provides useful (and tasty!) abstractions for many types of communication patterns.

Chops Net IP is layered on top of Chris Kohlhoff's Asio library (see [References](doc/references.md)) allowing it to be portable across many compilers and platforms. When the C++ Networking TS is standardized (possibly C++ 23 but more likely C++ 26) Chops Net IP will directly use the networking facilities of the C++ standard library.

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
- Small footprint or embedded environments, where all network processing is run inside a single thread. In particular, environments where a JVM (or similar run-time support) is too costly in terms of system resources, but have a relatively rich operating environment (e.g. Linux running on a small chip) are a very good fit. (Currently the main constraint is small system support in the Asio library implementation.)
- Applications with relatively simple network processing that need an easy-to-use and quick-for-development networking library.
- Applications with configuration driven networks that may need to switch (for example) between TCP connect versus TCP accept for a given connection, or between TCP and UDP for a given communication path.
- Peer-to-peer applications where the application doesn't care which side connects or accepts.
- Frameworks or groups of applications where abstracting wire-protocol logic from message processing logic makes sense.

## Examples

Example demo programs are in the [`/example`](https://github.com/connectivecpp/chops-net-ip) directory. 

The `simple_chat_demo.cpp` program has a listing of the multiple steps to set up working example.

## Want More?

A detailed overview, a C++ socket library comparison, and a FAQ is [available here](doc/overview.md).

# C++ Language Requirements and Alternatives

C++ 17 is the primary standards baseline for this repository. There is also one future C++ facility in use (`std::expected`, allowing error handling without using exceptions).

A significant number of C++ 11 features are in the implementation and API. There are also C++ 14 and 17 features in use such as `std::byte`, `std::optional`, `std::variant` and `auto` parameters in lambda functions. For users that don't want to use the latest C++ compilers or compile with C++ 17 flags, Martin Moene provides an excellent set of header-only libraries that implement many useful C++ 17 library features (for older compilers or standards), as well as future C++ features (see [References](doc/references.md)).

While the main production branch will always be developed and tested with C++ 17 features (and relatively current compilers), alternative branches and forks for older compiler versions are welcome. In particular, a branch using Martin's libraries and general C++ 11 (or C++ 14) conformance would be useful. A branch supporting a pre-C++ 11 compiler or language conformance is not likely to be directly supported through this repository (since it would require so many changes that it would result in a defacto different codebase).

# External Dependencies

The libraries and API's have minimal (as possible) library dependencies (there are heavy dependencies on the C++ standard library in all of the code). There are more dependencies in the test code than in the production code.

All the dependencies listed below have links that will take you to the library or repository.

Production external dependencies:

- Version 1.13 (or later) of Chris Kohlhoff's [`asio`](https://github.com/chriskohlhoff/asio) library is required. Note that it is the stand-alone Asio library, not the Boost Asio version.
- The [`utility-rack`](https://github.com/connectivecpp/utility-rack) library, which is a repository in the same GitHub account as Chops Net IP.

Test external dependencies:

- Version 2.8.0 (or later) of Phil Nash's [`Catch2`](https://github.com/catchorg/Catch2) library is required for all test scenarios.

There are single file headers that have been copied into the `third_party` directory of the `utility-rack` repository from various GitHub repositories and do not require any external dependency management. These are:

- Martin Moene's [`expected-lite`](https://github.com/martinmoene/expected-lite) library.

See [References](doc/references.md) for additional details.

# Supported Compilers and Platforms

Chops Net IP has been compiled and tests run on:

- g++ 7.2, g++ 7.3, Linux (Ubuntu 17.10 - kernel 4.13, Ubuntu 18.04 - kernel 4.15)
- (TBD, will include at least clang on linux and vc++ on Windows)

Follow the CI links for additional build environments.

# Installation

Chops Net IP is header-only, so installation consists of downloading or cloning and setting compiler include paths appropriately. No compile time configuration macros are defined.

# References

See [References](doc/references.md) for details on dependencies and inspirations for Chops Net IP.

# About

Team member information is [available here](https://connectivecpp.github.io/), and a few random author comments are [available here](doc/about.md).

