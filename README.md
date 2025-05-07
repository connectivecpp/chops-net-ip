# Chops Net IP - Connective Handcrafted Openwork Software for Asynchronous IP Networking

Chops Net IP is a C++ library that makes asynchronous networking programming fun. Or at least if not fun, it makes network programming easier and safer, without significantly sacrificing performance. Chops Net IP handles Internet Protocol (IP) communications including TCP, UDP, and UDP multicast. It is written using modern C++ design idioms and a recent (2020) C++ standard.

Chops Net IP is not like any other high-level, general purpose C++ socket library.

Chops Net IP is layered on top of the Asio asynchronous networking library, taking advantage of the portability and functionality that Asio provides. However, it simplifies network programming compared to coding against the Asio API, while providing easy scalability through the asynchronous facilities.

Chops Net IP:

1. Handles most of the Asio "gotchas", such as keeping buffers "alive" in asynchronous designs.
2. Simplifies many networking use cases, allowing simpler callbacks to be used.
3. Provides a similar API between TCP and UDP, allowing shared code and easy switching between protocols.
4. Provides callback interfaces for all of the "state" change points in TCP and UDP, allowing customized code (for example logging or statistics collection).

#### Unit Test and Documentation Generation Workflow Status

![GH Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/connectivecpp/chops-net-ip/build_run_unit_test_cmake.yml?branch=main&label=GH%20Actions%20build,%20unit%20tests%20on%20main)

![GH Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/connectivecpp/chops-net-ip/build_run_unit_test_cmake.yml?branch=develop&label=GH%20Actions%20build,%20unit%20tests%20on%20develop)

![GH Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/connectivecpp/chops-net-ip/gen_docs.yml?branch=main&label=GH%20Actions%20generate%20docs)

![GH Tag](https://img.shields.io/github/v/tag/connectivecpp/chops-net-ip?label=GH%20tag)

## Generated Documentation

The generated Doxygen documentation for `chops_net_ip` is [here](https://connectivecpp.github.io/chops-net-ip/).

## Overview

For many software developers, asynchronous network programming in C++ is not easy. It is complex, has many pitfalls, and requires designing C++ code in a way that is not natural, even for those with years of experience. Chops Net IP ("C"onnective "H"andcrafted "Op"enwork "S"oftware, Networking over Internet Protocol) simplifies asynchronous network programming and provides useful (and tasty!) abstractions for many types of communication patterns.

Chops Net IP is layered on top of Chris Kohlhoff's Asio library (see [References](https://connectivecpp.github.io/doc/references.html)) allowing it to be portable across many compilers and platforms. If networking is standardized in C++ Chops Net IP will directly use the networking facilities of the C++ standard library.

### Tasty Uses

Example environments where Chops Net IP is a good fit:

- Applications that are event driven or highly asynchronous in nature.
- Applications where data is generated and handled in a non-symmetric manner. For example, data may be generated on the TCP acceptor side, or may be generated on a TCP connector side, or on both sides depending on the use case. Similarly, applications where the data flow is bi-directional and sends or receives are data-driven versus pattern-driven work well with this library.
- Applications interacting with multiple (many) connections (e.g. handling multiple sensors or inputs or outputs), each with low to moderate throughput needs (i.e. IoT environments, chat networks, gaming networks).
- Small footprint or embedded environments, where all network processing is run inside a single thread. In particular, environments where a JVM (or similar run-time support) is too costly in terms of system resources, but have a relatively rich operating environment (e.g. Linux running on a small chip) are a very good fit. (Currently the main constraint is small system support in the Asio library implementation.)
- Applications with relatively simple network processing that need an easy-to-use and quick-for-development networking library.
- Applications with configuration driven networks that may need to switch (for example) between TCP connect versus TCP accept for a given connection, or between TCP and UDP for a given communication path.
- Peer-to-peer applications where the application doesn't care which side connects or accepts.
- Frameworks or groups of applications where abstracting wire-protocol logic from message processing logic makes sense.

### Examples

Example demo programs are in the [`/example`](./example) directory.

The `simple_chat_demo.cpp` program has a listing of the multiple steps to set up a working example.

### Want More?

A detailed overview, a C++ socket library comparison, and a FAQ is [available here](doc/overview.md).

## Library Dependencies

The stand-alone Asio library is a dependency for Chops Net IP. Specific version (or branch) specs for the Asio dependency is in [`cmake/download_asio_cpm.cmake`](cmake/download_asio_cpm.cmake).

The [`shared_buffer`](https://github.com/connectivecpp/shared-buffer) library from Connective C++ is a dependency, providing reference counted `std::byte` buffers.

[`expected_lite`](https://github.com/martinmoene/expected-lite) from Martin Moene is a dependency, providing `std::expected` functionality for C++ 20 code baselines. Chops Net IP uses `nonstd::expected` in the codebase per Martin's library and will transition to `std::expected` when C++ 23 becomes the baseline.

## C++ Standard

`chops_net_ip` is built under C++ 20, using features such as `std::span`. In the future `concepts` / `requires` will be added.

## Supported Compilers

Continuous integration workflows build and unit test on g++ (through Ubuntu) and MSVC (through Windows). Clang (through macOS) will be supported for continuous integration when C++ 20 thread feature `std::stop_source` (and related functionality) is fully supported in the GitHub macOS runners. The C++ 20 thread features are only needed for unit testing (through `wait_queue`). Conditional unit test builds (on macOS) could be implemented, but macOS clang updating to a standard that is now almost six years old is a better solution.

## Unit Test Dependencies

The unit test code uses [Catch2](https://github.com/catchorg/Catch2). If the `CHOPS_NET_IP_BUILD_TESTS` flag is provided to Cmake (see commands below) the Cmake configure / generate will download the Catch2 library as appropriate using the [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) dependency manager. If Catch2 (v3 or greater) is already installed using a different package manager (such as Conan or vcpkg), the `CPM_USE_LOCAL_PACKAGES` variable can be set which results in `find_package` being attempted. Note that v3 (or later) of Catch2 is required.

Specific version (or branch) specs for the Catch2 dependency is in the [test/CMakeLists.txt](test/CMakeLists.txt) file, look for the `CPMAddPackage` command.

The unit tests use the following libraries from Connective C++:
- [wait-queue](https://github.com/connectivecpp/wait-queue)
- [binary-serialize](https://github.com/connectivecpp/binary-serialize)
- [utility-rack](https://github.com/connectivecpp/utility-rack)

## Example Dependencies

The example apps do not (currently) have external dependencies other than the required dependencies for `chops_net_ip`.

## Build and Run Unit Tests

To build and run the unit test programs:

First clone the `chops-net-ip` repository, then create a build directory in parallel to the `chops-net-ip` directory (this is called "out of source" builds, which is recommended), then `cd` (change directory) into the build directory. The CMake commands:

```
cmake -D CHOPS_NET_IP_BUILD_TESTS:BOOL=ON ../chops-net-ip

cmake --build .

ctest
```

For additional test output, run the unit test individually, for example:

```
test/net_ip/basic_io_output_test -s
```

The examples can be built by adding `-D CHOPS_NET_IP_BUILD_EXAMPLES:BOOL=ON` to the CMake configure / generate step.

