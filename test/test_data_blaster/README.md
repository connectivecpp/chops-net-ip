# Test Data Blaster

The Test Data Blaster (T-DB) is a set of binaries that performs both TCP and UDP distributed testing (currently the UDP portion is not implemented). There are multiple binaries in the suite:
- Data sender / receiver (DSR), a TCP and (future) UDP version
- Statistics and logging monitor, a C++ and (future) Python version

Multiple data sender / receiver (DSR) instances are running at the same time (at least two must be running for sending and receiving data). Each TCP DSR instance can be configured for multiple TCP connectors and multiple TCP acceptors at the same time. Each (future) UDP DSR can be configured for multiple senders and receivers at the same time.

A single instance of the monitor shows statistics from the DSR instances. The monitor is also responsible for shutting down the DSRs. The same monitor instance can be running for both TCP and UDP (the messages sent to the monitor from the DSRs specify whether the test data is being sent over TCP or UDP).

While the DSRs are written in C++, the monitor app can be written in any language (and both a C++ and Python version is expected, with the Python version having a simple GUI display). The monitor message definition is text only, so binary endianness is not a factor.

## Usage

(Fill in command line usage here - note that typically the monitor is started first.)

(Fill in details about expected output.)

(Fill in details about how to trigger shutdown through the monitor.)

## Building Test Data Blaster

All of the C++ components can be built using CMake. 

(Fill in details, including single build instructions in Linux, and future Python build instructions.)

## Internals and Design

Having separate code bases (and executables) for the TCP and UDP DSRs simplifies the command line parameters as well as the internal logic. There is no specific technical reason why the DSR couldn't handle both TCP and UDP, but the DSR command line is already complicated enough.

There is (obviously) common code between all of the T-DB C++ components, some from the `shared_test` directory (i.e. code that is shared with the Chops Net IP unit tests), and some factored out in this (`test_data_blaster`) directory.

### Monitor Process

The T-DB Monitor is run as a TCP acceptor (server) only. It accepts TCP connections on one port, each connection corresponding to one instance of either a TCP DSR or a UDP DSR.

There are no "begin" or "end" data messages that the monitor has to process, a new incoming TCP connection is the indication that a new DSR instance has started, and the end of the TCP connection is the indication that the DSR instance has ended.

The DSR will not send a monitor message for every test message sent between DSR instances. A "modulo" command line parameter specifies how often a monitor message will be sent (for each test data set). (A modulo of 1 would generate one monitor message per test data set message.)

The monitor process will notify all DSR processes to shutdown by sending a shutdown message through each monitor connection. The monitor process will have a user initiated mechanism to send the shutdown messages and end the monitor process.

#### Monitor Message Definition

Each message from the DSRs to the monitor has the following fields:
- DSR name
- DSR protocol, either "tcp" or "udp"
- Remote endpoint (host, port)
- Data direction, either "incoming" or "outgoing"
- Current message number
- Total messages expected to be sent (or 0 if incoming messages -- with the previous field this makes "100th message out of 10,000 to be sent" displays possible)
- Current message size
- Current message beginning (first 15 characters)
- Outgoing queue size (will always be 0 for incoming data, and 0 for outgoing data if the receiving end is keeping up)

Each of the fields is a text string, and the full message is delimited by (fill in details).

TODO - define a format that makes Python deserialization easy. Specifically, are each of the text fields nul character terminated, or is there a length prefix? Do we want name / value pairs instead of value only fields? Is the end of the message delimited by a LF (line feed) character or CR / LF (carriage return / line feed)?

The shutdown message is the only message sent from the monitor to the DSRs and has the following fields:

(Fill in details.)

### TCP Data Sender Receiver

Each DSR instance can start multiple TCP connectors and multiple TCP acceptors as provided via the command line arguments.

If the DSR instance is configured to send messages (send count > 0), messages are sent when a connection is made. For TCP connectors this is when the connect succeeds. For acceptors it is when each incoming connection is made. Each connection will send out the full set of test messages with a delay between each message (the delay can be 0 for full message blasting).

If the "reply" command line parameter is set, an incoming message is reflected back to the sender.

Degenerate configurations are possible, with infinite message loops or where connection and shutdown timing is not clear. No special coding is in the T-DB to protect for bad configurations.

The DSR internal data messages uses the binary marshalled message format from the `shared_test` facilities (see `msg_handling.hpp` for specific code). The monitor messages are text based messages with all fields in a string format easy to unmarshall for Python applications.

The general logic flow:
- Process command line arguments
- Start connector to monitor
- Start acceptors and connectors
- If non-empty outgoing message set, for each connection made (whether connector or acceptor) start thread to send the messages, with a delay time between each message (delay may be 0)
- Shutdown when the future pops by an incoming shutdown message from the monitor process

