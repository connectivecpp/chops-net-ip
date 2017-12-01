# Chops Net IP Detailed Overview

## Motivation

Chops Net IP is motivated by the need for a networking library that:
- is easy to use
- scales well
- integrates relatively quickly into an application or library
- is easy to use correctly and hard to use incorrectly
- abstracts common TCP design usages into application supplied callback functions
- performs well in many environments

## General Usage and Design Model

Chops Net IP is (in general terms) a "create network entity, provide function objects and let them do the work" API for incoming data, and a "send and forget" API for outgoing data. 

For incoming data, an application provides callable function objects to the appropriate Chops Net object for message framing (if needed), message processing, and connection state transitions.

For outgoing data, an application passes message data (as byte buffers) to the appropriate Chops Net object for queueing and transmission. A callable function object can be provided by the application to monitor the size of an outgoing queue.

Various Chops Net objects are provided to the application (typically in application provided function objects) as connection or endpoint states transition. For example, the Chops Net object for data sending is only created when a connection becomes active (TCP acceptor connection is created, or a TCP connector connection succeeds). This guarantees that an application cannot start sending data before a connection is active.

Chops Net has the following design goals:

- Encapsulate and simplify the (sometimes complex) details of asynchronous network programming. Managing buffer lifetimes can be tricky, and this library makes sure it is done correctly. Chaining asynchronous events together is not always easy in application code, and this library simplifies that logic. Error handling is simpler for the application.
- Abstract and separate the message framing and message processing for message streams. Sometimes the same "wire protocol" (i.e. message header and message body definition) is used on multiple connections, but different message processing is required depending on the connection address (or connection type). Chops Net provides specific customization points to ease code sharing and code isolation. In particular, a message framing function object might be defined for a TCP stream (and not needed for a UDP entity), but the same message processing code used for both TCP and UDP entities.
- Make it easy to write correct network code, and hard to write incorrect network code. An example is that message sending cannot be started before a TCP connection is active. A second example is that correctly collecting all of the bytes in a TCP message header is easier with this library (this is a common source of errors in TCP network programming). A third example is that a read is always posted for TCP connections, even if the connection is used only for sending data. This allows timely notification when the connection closes or an error occurs (a common mistake is to forget to post a read, which sometimes results in very slow notification when a connection closes or has an error).
- Provide customization points so that the application can be notified of interesting events.
- Make it easy to develop peer-to-peer applications that primarily care about data transfer versus which side is connecting versus accepting connection requests.

## Chops Net States and Transitions

Chops Net states and transitions match existing standard network protocol behavior. For example, when a TCP connector is created, an actual TCP data connection does not exist until the connect succeeds. When this happens (connect succeeds), the abstract state transitions from unconnected to connected. In Chops Net, when a TCP connector connects, a data connection object is created and an application state transition function object callback is invoked containing the connection object.

Even though an implicit state transition table exists within the Chops Net library (matching network protocol behavior), there are not any explicit state flags or methods to query the state through the API. Instead, state transitions are handled through application supplied function object callbacks, which notify the application that something interesting has happened and containing objects for further interaction and processing. In other words, there is not an "is_connected" method with the Chops Net library. Instead, an application can layer its own state on top of Chops Net (if desired), using the function object callbacks to manage the state.

Pro tip - Chops Net follows the implicit state model of the Networking TS and Asio (and similar) libraries where state transitions are implemented through chaining function object callbacks on asynchronous operations. Developers familiar with implicit or explicit state transition models will be familiar with the application model defined for Chops Net. Chops Net insulates the application from the intricacies of the Networking TS API and simplifies the state transition details.

(State transition diagram to be inserted here.)

## Constraints

Chops Net works well with the following communication patterns:

- Receive data, process it quickly (which may involve passing data along to another thread), ready for more incoming data.
- Receive data, process it quickly (as above), send data back through the same connection or endpoint, ready for more incoming data.
- Send data, go back to other work.

Chops Net requires more work with the following communication pattern:

- Send data, wait for reply (request-reply pattern). Everything in Chops Net is no-wait from the application perspective, so request-reply must be emulated through application logic (e.g. store some form of message transaction id in an outgoing table and correlate incoming data using the message transaction id, or track outstanding requests by connection address).

Chops Net works extremely well in environments where there might be a lot of network connections (e.g. thousands), each with a moderate amount of traffic, and each with different kinds of data or data processing. In environments where each connection is very busy, or a lot of processing is required for each incoming message (and it cannot be passed along to another thread), then more traditional communication patterns or designs might be appropriate (e.g. blocking or synchronous I/O, or "thread per connection" models.


Applications that do only one thing and must do it as fast as possible and want the least amount of overhead might not want the abstraction penalties and slight overhead of Chops Net. For example, a high performance server application where buffer lifetimes for incoming data are easily managed might not want the queuing and "shared buffer" overhead of Chops Net.


Applications that need to perform time consuming operations on incoming data and cannot pass that data off to another thread may encounter throughput issues. Multiple threads or thread pools or strands interacting with the event loop method (executor) may be a solution in those environments.


## Future Directions

- Older compiler (along with older C++ standard) support is likely to be implemented sooner than later (as discussed in the Language Requirements and Alternatives section below), depending on availability and collaboration support.
- SSL support may be added, depending on collaborators with expertise being available.
- Additional protocols may be added (or parallel libraries added) to the TCP, UDP, and UDP multicast support, including serial I/O and Bluetooth. If a reliable multicast protocol is popular enough, support may be added.
- Publish and Subscribe communications models may be added, but would likely be a separate library with a completely different API (using Chops Net internally, possibly).

