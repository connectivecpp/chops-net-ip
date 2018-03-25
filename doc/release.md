# Chops Release Status

## Release 0.2

Release 0.2 is now (Feb 25, 2018) merged to the master branch:

### Chops Net IP

- Significant changes have been made to the `start` method function parameters of the `basic_net_entity` class. There are two function object parameters for callbacks, the first corresponding to an IO state change, and the second to errors. This makes better conceptual sense and cleans up logical inconsistencies in the callback interface. Specifically:
  - IO state change callbacks correspond to a TCP connection being created or destroyed, or a UDP socket being opened or closed. There is one state change callback invocation for creation or open and one state change callback invocation for destruction or close. In both cases the `basic_io_interface` is tied to a valid IO handler. This allows for simpler state management and consistent associative container usage.
  - Error callbacks correspond to errors or shutdowns. These can happen in either the IO handling or in the higher level net entity handling. There may or may not be a valid IO handler referred to in the `basic_io_interface` object.
- The indirect memory leaks reported by address sanitizer have been fixed.
- A more consistent approach to exceptions and error returns is now in place for the `basic_io_interface` and `basic_net_entity` methods.

- UDP multicast support is the top priority for the next feature to be implemented.
- Strand design and support is under consideration.
- Multiple compiler support is under way, VC++ first.
- CMake file improvement is under way.

All tests run, although they are still limited (see next steps and constraints).


## Next Steps, ToDo's, Problems, and Constraints:

- This is a good point to ask for project help and collaboration, which will be greatly appreciated (for many reasons).
- The code is well doxygenated, and there is a good start on the high level descriptions, but tutorials and other high-level documentation is needed. A "doxygen to markdown" procedure is needed (or an equivalent step to generate the documentation from the embedded doxygen).
- The code only compiles on one compiler, but VC++ and Clang support (with the latest C++ standard flags) is expected soon. Compiling and building on Windows 10 is also expected to be supported at that time. Once multiple compilers and desktop environments are tested, development will expand to smaller and more esoteric environments (e.g. Raspberry Pi).
- Attention will be devoted to performance bottlenecks as the project matures.
- The makefiles and build infrastructure components are not yet present. A working CMakeLists.txt is needed as well as Github continuous integration procedures (e.g. Jenkins and Travis).
- Code coverage tools have not been used on the codebase.

### Chops Net IP

- There are likely to be Chops Net IP bugs, specially relating to error and shutdown scenarios.
- Most of the testing has been "loopback" testing on one system. This will soon expand to distributed testing among multiple systems, specially as additional operating system build and testing is performed.
- Example code needs to be written and tested (there is a lot of code under the Catch framework, but that is not the same as stand-alone examples).
- The "to do's" that are relatively small and short-term (and mentioned in code comments):
  - Implement multicast support
  - Investigate specific error logic on TCP connect errors - since timer support is part of a TCP connector, determine which errors are "whoah, something bad happened, bail out", and which errors are "hey, set timer, let's try again a little bit later"
  - UDP sockets are opened in the "start" method with a ipv4 flag when there is not an endpoint available (i.e. "send only" UDP entities) - this needs to be re-thought, possibly leaving the socket closed and opening it when the first send is called (interrogate the first endpoint to see if it is v4 or v6)

## Older Releases

### Release 0.1

Release 0.1 is now (Feb 13, 2018) merged to the master branch:

- All initial Chops Net IP planned functionality is implemented except TCP multicast (there may be API changes in limited areas, see next steps below).
- All of the code builds under G++ 7.2 with "-std=c+1z" and is tested on Ubuntu 17.10.
- All tests are built with and run under the Catch2 testing framework.
- All code has been sanitized with "-fsanitize=address".

Known problems in release 0.1:

- Address sanitizer (Asan) is reporting indirect memory leaks, which appear to be going through the `std::vector` `resize` method on certain paths (where `start_io` is called from a different thread where most of the processing is occurring). This is being actively worked.
- The primary author (Cliff) is not happy with the function object callback interfaces through the `basic_net_entity.start` method (state change, error reporting callbacks). There are multiple possibilities, all of which have pros and cons. The message frame and message handler function object callback API is good and solid and is not likely to change.
