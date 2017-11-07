# chops
The Chops Medley is a collection of C++ libraries oriented towards networking and distributed processing. It is written using modern C++ design idioms and the latest (as of 2017) C++ standard languages. A significant number of C++ 11 features are used, while only basic C++ 14 and 17 features are used (e.g. std::optional, std::byte, simple usages of structured bindings, generic lambdas).

The libraries and API's have minimal library dependencies, typically only on the standard C++ library. The test suites have additional dependencies, including Catch 2.0 for the unit test framework. The Catch library is available at https://github.com/catchorg/Catch2.

For users that don't want to use the latest C++ compilers or compile with C++ 17 flags, Martin Moene has written a nice set of small header-only libraries that implement many useful C++ library features, both C++ 17 as well as future C++ features. . These include std::optional, std::variant, Thes such as an implementation of the proposed std::ring_span
