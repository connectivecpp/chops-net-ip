# Chops Net IP Release Status

## Release 1.0

Release 1.0 is under development, expected in the summer of 2019. Additional platform and compiler testing is under way, in-depth tutorials are under development, and the marshalling library needs to be completed.

## Release 0.3

Significant API and internal changes have been made. 

- `basic_io_interface` is now split into `basic_io_interface` and `basic_io_output`. The `send` and `get_output_queue_stats` methods have moved from `basic_io_interface` to `basic_io_output`. There is now a `make_io_output` method in `basic_io_interface`. (Motivation: separation of responsibilities, allow optimized data sending path.)

- There is no longer a `basic_net_entity` class. It is now `net_entity` and is not a class template. (Motivation: simplify usage of `net_entity` class.)

- State change callback now returns a `bool` (instead of `void`). (Motivation: provide another path for shutting down a network entity, e.g. TCP connector can be shut down by returning `false`.)

- The message handler callback interface now provides a `basic_io_output` (either a `tcp_io_output` or a `udp_io_output`) instead of a `basic_io_interface`. (Motivation: constrain possibilities within a message callback reply.)

- `get_socket` is now `visit_socket` with a different signature (now takes a function object instead of returning a socket reference). (Motivation: simplifies template method design.)

- `net_entity` now has a `visit_io_output` method to allow passing data into all available `basic_io_output` objects associated with that `net_entity`. (Motivation: consistency with `visit_socket` and simplifies template method design.)

- `io_interface.hpp` is now named `io_type_decls.hpp`, which contains `using` declarations to instantiate `basic_io_interface` and `basic_io_output` with `tcp_io` and `udp_io`. (Motivation: `using` declarations are for two different class templates, both related to IO.)

- All exception throwing within Chops Net IP has been removed, and error returns in many places have been replaced with a `std::expected` return (currently using the `nonstd` namespace instead of `std` through Martin Moene's `expected-lite` library). (Motivation: remove need for `try`, `catch` blocks, provide useful error return information.)

Specifics on each change (including non-API changes):

- `basic_net_entity` has gone away, replaced by a non-template class supporting all three entity types (TCP connector, TCP acceptor, UDP). Internally it is now using a `std::variant`.

- The state change callback now returns `bool`, allowing a `net_entity` to be stopped (when `false` is returned). This allows slightly simpler code for stopping a TCP connector (or certain use cases of stopping a TCP acceptor). It is also similar to the return type of the message handler callback, where returning `false` shuts down the TCP connection (or UDP socket).

- The TCP connector now will attempt re-connects after a TCP connection error (or shutdown), unless `false` is returned from the state change callback. This simplifies the uses cases where re-connects are the desired behavior (and if not, it's now relatively simple to stop the connector by returning `false` from the state change callback).

- A new `start_io` method is available for the `basic_io_interface` class, supporting simple variable length message framing (a common use case).

- `basic_io_interface` is now split into `basic_io_interface` and `basic_io_output`. This simplifies the responsibility of `basic_io_interface`, as it now is (primarily) responsible only for starting and stopping IO. `basic_io_output` is only responsible for sending data (and providing output queue statistics). This allows some optimization of the send path, since a `weak_ptr` no longer has to be locked into a `shared_ptr` for every `send`. (This fits into the overall performance goal of making the sending and receiving of data to be as fast as possible, while other operations such as starting and stopping connections to have more overhead).

- The message handler callback now provides a `basic_io_output` object instead of a `basic_io_interface` object. This constrains the message handler callback, which is appropriate (since `start_io` and `stop_io` should never be called from within the message handler).

- The `net_entity` class now allows data to be passed into a `visit_io_output` method (by function object), allowing that data (in the function object) to be sent to all associated and active IO handlers. This allows certain use cases to be simplified.

- The `send_to_all` and `simple_variable_len_message_frame` component headers have been removed (now supported in the core library as described in the previous changes).

- Internal shutdown logic has been improved.

- The `net_ip/component` directory has been moved to `net_ip_component` to make it more visible and to show that it is parallel to (and not part of) the core `net_ip` library.

## Release 0.2

Release 0.2 is now (Feb 25, 2018) merged to the master branch.

### Notes

- Significant changes have been made to the `start` method function parameters of the `basic_net_entity` class. There are two function object parameters for callbacks, the first is invoked on an IO state change, and the second is invoked on errors or closes. This makes better conceptual sense and cleans up former inconsistencies in the callback interface. Specifically:
  - IO state change callbacks correspond to a TCP connection being created or destroyed, or a UDP socket being opened or closed. There is one state change callback invocation for creation or open and one state change callback invocation for destruction or close. In both cases the `basic_io_interface` is associated to a valid IO handler. This allows for simpler state management and consistent associative container usage.
  - Error callbacks correspond to errors or shutdowns. These can happen in either the IO handling or in the higher level net entity handling. There may or may not be a valid IO handler association in the `basic_io_interface` object.
- The indirect memory leaks reported by address sanitizer have been fixed.
- A more consistent approach to exceptions and error returns is now in place for the `basic_io_interface` and `basic_net_entity` methods.

- UDP multicast support is the top priority for the next feature to be implemented.
- Strand design and support is under consideration.
- Multiple compiler support is under way, VC++ first.
- CMake file improvement is under way.

All tests run, although they are still limited (see next steps and constraints).


### Next Steps, ToDo's, Problems, and Constraints:

- The code is well doxygenated, and there is a good start on the high level descriptions, but tutorials and other high-level documentation is needed. A "doxygen to markdown" procedure is needed (or an equivalent step to generate the documentation from the embedded doxygen).
- The code only compiles on one compiler, but VC++ and Clang support (with the latest C++ standard flags) is expected soon. Compiling and building on Windows 10 is also expected to be supported at that time. Once multiple compilers and desktop environments are tested, development will expand to smaller and more esoteric environments (e.g. Raspberry Pi).
- Attention will be devoted to performance bottlenecks as the project matures.
- The makefiles and build infrastructure components are not yet present. A working CMakeLists.txt is needed as well as GitHub continuous integration procedures (e.g. Jenkins and Travis).
- Code coverage tools have not been used on the codebase.

### Overall Comments

- There are likely to be Chops Net IP bugs, specially relating to error and shutdown scenarios.
- Most of the testing has been "loopback" testing on one system. This will soon expand to distributed testing among multiple systems, specially as additional operating system build and testing is performed.
- Example code needs to be written and tested (there is a lot of code under the Catch framework, but that is not the same as stand-alone examples).
- The "to do's" that are relatively small and short-term (and mentioned in code comments):
  - Implement multicast support
  - Investigate specific error logic on TCP connect errors - since timer support is part of a TCP connector, determine which errors are "whoah, something bad happened, bail out", and which errors are "hey, set timer, let's try again a little bit later"
  - UDP sockets are opened in the "start" method with a ipv4 flag when there is not an endpoint available (i.e. "send only" UDP entities) - this needs to be re-thought, possibly leaving the socket closed and opening it when the first send is called (interrogate the first endpoint to see if it is v4 or v6)

## Release 0.1

### Notes

Release 0.1 is now (Feb 13, 2018) merged to the master branch:

- All initial Chops Net IP planned functionality is implemented except TCP multicast (there may be API changes in limited areas, see next steps below).
- All of the code builds under G++ 7.2 with "-std=c+1z" and is tested on Ubuntu 17.10.
- All tests are built with and run under the Catch2 testing framework.
- All code has been sanitized with "-fsanitize=address".

Known problems in release 0.1:

- Address sanitizer (Asan) is reporting indirect memory leaks, which appear to be going through the `std::vector` `resize` method on certain paths (where `start_io` is called from a different thread where most of the processing is occurring). This is being actively worked.
- The primary author (Cliff) is not happy with the function object callback interfaces through the `basic_net_entity.start` method (state change, error reporting callbacks). There are multiple possibilities, all of which have pros and cons. The message frame and message handler function object callback API is good and solid and is not likely to change.
