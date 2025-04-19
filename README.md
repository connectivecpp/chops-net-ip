# Chops Net IP - Connective Handcrafted Openwork Software for Asynchronous IP Networking

Chops Net IP is a C++ library that makes asynchronous networking programming fun. Or at least if not fun, it makes network programming easier and safer, without significantly sacrificing performance. Chops Net IP handles Internet Protocol (IP) communications including TCP, UDP, and UDP multicast. It is written using modern C++ design idioms and a recent (2020) C++ standard.

Chops Net IP is not like any other high-level, general purpose C++ socket library.

Chops Net IP is layered on top of the Asio asynchronous networking library, taking advantage of the portability and functionality that Asio provides. However, it simplifies network programming compared to coding against the Asio API, while providing easy scalability through the asynchronous facilities.

Chops Net IP:

1. Asio gotchas
2. Simplifies many use cases
3. API between TCP and UDP is similar, allowing shared code
4. Provides callback points 

#### Unit Test and Documentation Generation Workflow Status

![GH Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/connectivecpp/chops-net-ip/build_run_unit_test_cmake.yml?branch=main&label=GH%20Actions%20build,%20unit%20tests%20on%20main)

![GH Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/connectivecpp/chops-net-ip/build_run_unit_test_cmake.yml?branch=develop&label=GH%20Actions%20build,%20unit%20tests%20on%20develop)

![GH Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/connectivecpp/chops-net-ip/gen_docs.yml?branch=main&label=GH%20Actions%20generate%20docs)

![GH Tag](https://img.shields.io/github/v/tag/connectivecpp/chops-net-ip?label=GH%20tag)

## Overview

(fill in)

## Generated Documentation

The generated Doxygen documentation for `chops_net_ip` is [here](https://connectivecpp.github.io/chops-net-ip/).

## Library Dependencies

The stand-alone Asio library is a dependency for Chops Net IP. Specific version (or branch) specs for the Asio dependency is in [`cmake/download_asio_cpm.cmake`](cmake/download_asio_cpm.cmake).

The [`shared_buffer`](https://github.com/connectivecpp/shared-buffer) library from Connective C++ is a dependency, providing reference counted `std::byte` buffers.

[`expected_lite`](https://github.com/martinmoene/expected-lite) from Martin Moene is a dependency, providing `std::expected` functionality for C++ 20 code baselines. Chops Net IP uses `nonstd::expected` in the codebase per Martin's library and will transition to `std::expected` when C++ 23 becomes the baseline.

## C++ Standard

`chops_net_ip` is built under C++ 20, using features such as `std::span`. In the future `concepts` / `requires` will be added.

## Supported Compilers

Continuous integration workflows build and unit test on g++ (through Ubuntu), MSVC (through Windows), and clang (through macOS).

## Unit Test Dependencies

The unit test code uses [Catch2](https://github.com/catchorg/Catch2). If the `CHOPS_NET_IP_BUILD_TESTS` flag is provided to Cmake (see commands below) the Cmake configure / generate will download the Catch2 library as appropriate using the [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) dependency manager. If Catch2 (v3 or greater) is already installed using a different package manager (such as Conan or vcpkg), the `CPM_USE_LOCAL_PACKAGES` variable can be set which results in `find_package` being attempted. Note that v3 (or later) of Catch2 is required.

Specific version (or branch) specs for the Catch2 dependency is in the [test/CMakeLists.txt](test/CMakeLists.txt) file, look for the `CPMAddPackage` command.

## Example Dependencies

(Fill in).

## Build and Run Unit Tests

To build and run the unit test programs:

First clone the `chops-net-ip` repository, then create a build directory in parallel to the `chops-net-ip` directory (this is called "out of source" builds, which is recommended), then `cd` (change directory) into the build directory. The CMake commands:

```
cmake -D CHOPS_NET_IP_BUILD_TESTS:BOOL=ON ../chops_net_ip

cmake --build .

ctest
```

For additional test output, run the unit test individually, for example:

```
test/net_ip/basic_io_output_test -s
```

The examples can be built by adding `-D CHOPS_NET_IP_BUILD_EXAMPLES:BOOL=ON` to the CMake configure / generate step.

