# Test Data Blaster

The Test Data Blaster (T-DB) applications are:
- Data sender / receiver (DSR)
- Statistics and logging monitor

There are two varieties of the DSR (data sender / receiver), one for TCP/IP and one for UDP/IP. Having separate code bases (and executables) simplifies the command line parameters as well as the internal logic. There is common code, some already factored out in the `shared_test` directory, with additional common code factored out in this (`test_data_blaster`) directory.

The monitor application is the same for both the TCP/IP DSR and the UDP/IP DSR, and the same monitor instance can be run to monitor TCP and UDP DSR testing at the same time (to handle this use case, a protocol flag is part of the monitor message definition).

While the data sender and receiver apps are written in C++, the monitor app can be written in any language (and both a C++ and Python version is expected, with the Python version having a simple GUI display). The monitor message definition is text only, so binary endianness is not a factor.

## Monitor Process

The T-DB Monitor is run as a server (only). It accepts TCP connections on one port, each connection corresponding to one instance of either a TCP T-DB sender / receiver or a UDP T-DB sender / receiver.

There are no "begin" or "end" messages, a new incoming TCP connection is the indication that a new DSR instance has started, and the end of the TCP connection is the indication that the DSR instance has ended.

The DSR will not send a monitor message for every test message sent between DSR instances. A "modulo" command line parameter specifies how often a monitor message will be sent (for each test data set). (A modulo of 1 would generate one monitor message per test data set message.)

### Monitor Message Definition

Each message to the Monitor has the following fields:
- DSR name
- DSR protocol, either "tcp" or "udp"
- Remote endpoint (host, port)
- Data direction, either "incoming" or "outgoing"
- Current message number
- Total messages expected or to be sent
-- with previous field this makes either "100th message out of 10,000 expected / sent" displays possible
- Current message size
- Current message prefix
- Current message body character

Each of the fields is a text string, and the full message is delimited by

TODO - define a format that makes Python deserialization easy. Specifically, are each of the text fields nul character terminated, or is there a length prefix? Do we want name / value pairs instead of value only fields? Is the end of the message delimited by a LF (line feed) character or CR / LF (carriage return / line feed)?


## TCP Data Sender Receiver

Each DSR instance can start multiple TCP connectors and multiple TCP acceptors as provided via the command line arguments.

If the DSR instance is configured to send messages (send count > 0), messages are sent when a connection is made. For TCP connectors this is when the connect succeeds. For acceptors it is when each incoming connection is made.

Degenerate configurations are possible, with infinite message loops or where connection and shutdown timing is not clear. The T-DB is meant to be run by knowledgeable users, not general developers.

The internal data messages are using the binary marshalled messages from the `shared_test` facilities (see `msg_handling.hpp` for specific code).



