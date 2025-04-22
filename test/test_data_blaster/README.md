# Test Data Blaster

The Test Data Blaster (T-DB) is a set of binaries that performs both TCP and UDP distributed testing (currently the UDP portion is not implemented). There are multiple binaries in the suite:
- Data sender / receiver (DSR), a TCP and (future) UDP version (C++ only)
- Monitor, a statistics and logging display (both C++ and Python versions)

Multiple data sender / receiver (DSR) instances are running at the same time (at least two must be running for sending and receiving data). Each TCP DSR instance can be configured for multiple TCP connectors and multiple TCP acceptors at the same time. Each (future) UDP DSR can be configured for multiple senders and receivers at the same time.

A single instance of the monitor (either the C++ or Python version) shows statistics from the DSR instances. The monitor is also responsible for shutting down the DSRs. The same monitor instance can be running for both TCP and UDP DSRs (the log messages sent to the monitor from the DSRs specify whether the test data is being sent over TCP or UDP).

The message definitions between the DSRs and the monitor are text only, so binary endianness is not a factor, and will run on any platform. The data flowing between DSRs is serialized and deserialized as needed by the DSRs.

## Usage

The monitor process is typically started first, otherwise monitor log messages may be lost.

(Fill in command line usage here.)

(Fill in details about expected output.)

If the connection to the monitor process from a DSR is broken, (fill in details about display behavior, it will be different between the C++ and Python versions).

(Fill in details about how to trigger shutdown through the monitor.)


## Building Test Data Blaster

All of the C++ components can be built using CMake. 

The Python monitor is built by ...

(Fill in details, including single build instructions in Linux, and Python build instructions.)

## Internals and Design

Having separate code bases (and executables) for the TCP and UDP DSRs simplifies the command line parameters as well as the internal logic. There is no specific technical reason why the DSR couldn't handle both TCP and UDP, but the TCP DSR command line has quite a bit of complexity without adding UDP options.

There is (obviously) common code between all of the T-DB C++ components, some from the `shared_test` directory (i.e. code that is shared with the Chops Net IP unit tests), and some factored out in this (`test_data_blaster`) directory.

### Monitor Process

The T-DB Monitor is run as a TCP acceptor (server) only. It accepts TCP connections on one port, each connection corresponding to one instance of either a TCP DSR or a (future) UDP DSR.

There are no "begin" or "end" data messages that the monitor has to process, a new incoming TCP connection is the indication that a new DSR instance has started, and the end of the TCP connection is the indication that the DSR instance has ended.

The DSR will not send a monitor log message for every test message sent between DSR instances. A "modulo" command line parameter specifies how often a monitor log message will be sent (for each test data set). (A modulo of 1 would generate one monitor log message per test data set message.)

The monitor process will notify all DSR processes to shutdown by sending a shutdown message through each monitor connection. The monitor process will have a user initiated mechanism to send the shutdown messages and end the monitor process.

When shutdown is initiated the monitor process waits for (fill in details) seconds to ensure that the shutdown message is sent to all DSR connections.

(Note - Roxanne has suggested adding a two-way shutdown message sequence. A "shutdown requested" message would be sent to all DSRs, and a "shutdown response" message would be sent back to the monitor be each DSR. This allows quicker shutdown by the monitor process, since it will know exactly when the last DSR has shut down.)

#### Monitor Log Message Contents

Each message from the DSRs to the monitor has the following fields:
- DSR name
- DSR protocol, either "tcp" or "udp"
- Remote endpoint (host, port)
- Data direction, either "incoming" or "outgoing"
- Current message number
- Current message size
- Current message beginning (first 15 characters)
- Total messages expected to be sent (or 0 if incoming messages -- with the previous field this makes "100th message out of 10,000 to be sent" displays possible)
- Outgoing queue size (will always be 0 for incoming data, and 0 for outgoing data if the receiving end is keeping up)

#### Monitor Log Message Wire Protocol

The monitor message will be sent as a text message, with a newline ("\n") as the end character.

Each of the fields is a text string (array of Ascii characters, UTF-8 without any extended characters) with a null character ("\0") at the end of the string.

#### Shutdown Message Contents and Wire Protocol

The shutdown message is the only message sent from the monitor to the DSRs and consists of three characters: "s\0\n"

These three characters are a lower case "s" (short for shutdown), a null character and a newline character.

(Proposed message, from Roxanne: - the DSR sends back a shutdown response message consisting of three characters: "r\0\n".)

### TCP Data Sender Receiver

Each DSR instance can start multiple TCP connectors and multiple TCP acceptors as provided via the command line arguments.

If the DSR instance is configured to send messages (send count > 0), messages are sent when a connection is made. For TCP connectors this is when the connect succeeds. For acceptors it is when each incoming connection is made. Each connection will send out the full set of test messages with a delay between each message (the delay can be 0 for full message blasting).

If the "reply" command line parameter is set, an incoming message is reflected back to the sender.

Degenerate configurations are possible, with infinite message loops or where connection and shutdown timing is not clear. No special coding is in the T-DB to protect for bad configurations.

The DSR internal data messages uses the binary serialized message format from the `shared_test` facilities (see `msg_handling.hpp` for specific code). The monitor messages are text based messages with all fields in a string format easy to deserialize for Python applications.

The general logic flow:
- Process command line arguments
- Start connector to monitor
- Start acceptors and connectors
- If non-empty outgoing message set, for each connection made (whether connector or acceptor) start thread to send the messages, with a delay time between each message (delay may be 0)
- Shutdown when the future pops by an incoming shutdown message from the monitor process

