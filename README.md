# Chops - Connective Handcrafted Openwork Software Medley

The Chops Medley is a collection of C++ libraries oriented towards networking and distributed processing. It is written using modern C++ design idioms and the latest (2017) C++ standard. A significant number of C++ 11 features are used, while only basic C++ 14 and 17 features are used (e.g. 'std::optional', 'std::byte', simple usages of structured bindings, generic lambdas).

# Contents

## Chops Net

Chops Net is an asynchronous general purpose networking library layered on top of the C++ Networking Technical Standard (TS). It has three main goals:

- Encapsulate and simplify 
- lkdsjf
- sdlkfj

## Chops Wait Queue

Chops Wait Queue is a multi-reader, multi-writer queue with wait and no-wait pop semantics, as well as simple "close" and "open" capabilities (to allow graceful shutdown or restart of thread or process communication). This lsdjf

# Dependencies

The libraries and API's have minimal library dependencies. Currently the non-test code depends on the standard C++ library and Chris Kohlhoff's Asio library. Asio is available at https://think-async.com/ as well as @github/chriskohlhoff https://github.com/chriskohlhoff/. Asio forms the basis for the C++ Networking Technical Standard (TS), which will (almost surely) be standardized in C++ 20. Currently the Chops Net library uses the 'networking-ts-impl' repository from Chris' Github account.

The test suites have additional dependencies, including Phil Nash's Catch 2.0 for the unit test framework. The Catch library is available at https://github.com/catchorg/Catch2.

For users that don't want to use the latest C++ compilers or compile with C++ 17 flags, Martin Moene has written a nice set of small header-only libraries that implement many useful C++ library features, both C++ 17 as well as future C++ standards. These include 'std::optional', 'std::variant', 'std::any', and 'std::byte' (from C++ 17) as well as 'std::ring_span' (C++ 20, most likely). He also has multiple other repositories such as an implementation of the C++ Guideline Support Library (GSL).


