# C++ Socket Library Comparisons

This page is a comparison of various portable C++ socket libraries against Chops Net IP, from the primary author (Cliff Green). This comparison offers evaluations that are both subjective and objective.

The main criteria includes:
- Portable C++, compilable on multiple compilers and operating system platforms
- General purpose, can interface with any protocol (does not impose its own wire protocol)
- Supports both binary and text protocols
- Has both TCP connector and TCP acceptor support
- Allows socket options to be queried and modified

The secondary criteria includes:
- UDP unicast and multicast support
- Provides an OS (or lower level) socket interface
- Highly scalable
- Bi-directional data flow

## Asio and C++ Networking TS

Both Asio and the C++ Networking TS provide the foundation and underlying asynchronous model for Chops Net IP. Chops Net IP is layered on top of the C++ Networking TS and could easily be ported to use Asio. Chops Net IP provides usage and abstraction advantages as outlined in the overview documentation.

## ACE

ACE is available at http://www.cs.wustl.edu/~schmidt/ACE.html.

ACE is the "granddaddy" of portable C++ networking libraries, in my personal experience. It is where I learned about reactor and proactor designs, and where the seminal ideas for the Chops Net IP abstractions were formed. Some of the strengths of ACE include:
- Support for many different compilers and platforms.
- Maturity and a very well tested implementation.
- Scalability and performance in many environments.
- Reactor and proactor implementions, allowing multiple models of application usage.

Some of the disadvantages:
- The C++ API and implementation are old (pre C++ 11) with many design constraints for old C++ and compiler dialects.
- The level of abstraction is similar to the Asio and C++ Networking TS compared to a higher level of abstraction in Chops Net IP.
- ACE has many classes and functions that are no longer needed in modern C++.
- Configuring and building ACE is non-trivial.





