# Contents of Test Data Blaster

The Test Data Blaster (acronym TDB) applications consist of two executables:
- Data sender / receiver (acronym DSR)
- Statistics and logging monitor

There are two varieties of the DSR (data sender / receiver), one for TCP/IP and one for UDP/IP. Having separate code bases (and executables) simplifies the command line as well as the internal logic. Some common code is already factored out in the `shared_test` directory, with additional common code factored out in this directory.

The monitor application is the same for both the TCP/IP DSR and the UDP/IP DSR, and the same monitor instance can be run to monitor TCP and UDP DSR testing at the same time (to handle this use case, a protocol flag is part of the monitor message definition).

While the data sender and receiver apps are written in C++, the monitor app can be written in any language (and both a C++ and Python version is expected, with the Python version having a simple GUI display). The monitor message definition is text only, so binary endianness is not a factor.

## TDB Monitor

The TDB Monitor is run as a server (only). It accepts TCP connections on one port, each connection corresponding to one instance of either a TCP TDB sender / receiver or a UDP TDB sender / receiver.

## TDB Monitor Message Definition

Each incoming message has the following fields:
- DSR name
- DSR protocol, either "tcp" or "udp"
- 

There are no "begin" or "end" messages, a new incoming connection is the indication that a new DSR instance has started, and the end of the connection is the indication that the DSR instance has ended.




## TCP TDB - TCP/IP Test Data Blaster


