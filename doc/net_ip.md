# Chops Net IP Detailed Overview

## Motivation

Chops Net IP is motivated by the need for a networking library that:
- is asynchronous and integrates relatively quickly into an application or library
- scales well
- is easy to use correctly and hard to use incorrectly
- abstracts common TCP design needs and usages into application supplied callback function objects
- performs well in many environments
- allows independent bi-directional data flow

## General Usage and Design Model

Chops Net IP is (in general terms) a "create network entity, provide function objects and let them do the work" API for incoming data, and a "send and forget" API for outgoing data. 

For incoming data, an application provides callable function objects to the appropriate Chops Net IP object for message framing (if needed), message processing, and connection state transitions.

For outgoing data, an application passes message data (as byte buffers) to the appropriate Chops Net IP object for queueing and transmission. The application can query information about the outgoing data queue.

Various Chops Net IP objects are provided to the application (typically in application provided function objects) as connection or endpoint states transition. For example, the Chops Net IP object for data sending is only created when a connection becomes active (TCP acceptor connection is created, or a TCP connector connection succeeds). This guarantees that an application cannot start sending data before a connection is active.

Chops Net IP has the following design goals:

- Encapsulate and simplify the (sometimes complex) details of asynchronous network programming. This includes:
  - Managing buffer lifetimes (which can be tricky) and this library makes sure it is done correctly.
  - Managing function object lifetimes (which can be tricky), making sure function objects are copied or moved between the application thread and executor thread (run loop thread) instead of allowing dangling function object references.
  - Chaining asynchronous events together, which is not always easy or obvious in application code. This library simplifies the chaining logic. 
  - Simplifying error and exception handling as much as possible.
- Abstract and separate the message framing and message processing for message streams. Sometimes the same "wire protocol" (i.e. message header and message body definition) is used on multiple connections, but different message processing is required depending on the connection address (or connection type). Chops Net IP provides specific customization points to ease code sharing and code isolation. In particular, a message framing function object might be defined for a TCP stream (and not needed for a UDP entity), but the same message processing code used for both TCP and UDP entities.
- Make it easy to write correct network code, and hard to write incorrect network code. An example is that message sending cannot be started before a TCP connection is active. A second example is that correctly collecting all of the bytes in a TCP message header is easier with this library (this is a common source of errors in TCP network programming). A third example is that a read is always posted for TCP connections, even if the connection is used only for sending data. This allows timely notification when the connection closes or an error occurs (a common mistake is to forget to post a read, which sometimes results in very slow notification when a connection closes or has an error).
- Provide customization points so that the application can be notified of interesting events (primarily errors and connection state changes).
- Make it easy to develop peer-to-peer applications that primarily care about data transfer versus caring about which application connects and which accepts connection requests.

## Component Directory

All of the essential Chops Net IP headers are in the `net_ip` and `detail` directories. There are additional useful (and tested) classes and functions in the `component` directory. One of the headers provides an executor and "run loop" convenience class, and other headers provide classes or functions for common use cases such as returning a `std::future` (or pair of futures) that provides an `io_interface` object for starting the read processing and enabling sends.

## FAQ

- Is Chops Net IP a framework?
  - No. It is a general purpose library. There are no specific network protocols required by the library and no wire protocols added by the library. It can communicate with any TCP or UDP application. Obviously the wire protocols and communication semantics need to be appropriately implemented by the application using the Chops Net IP library.
- Wny is a queue required for outgoing data?
  - Applications may send data faster than it can be consumed at the remote end (or passed through the local network stack). Queueing the outgoing data allows timely processing if this situation occurs. One design possibility for the library is to push the responsibility of sending the next chunk of data back to the application, but this requires a non-trivial API interaction between the library and the application. "Fire and forget" makes for an easy application API at the cost of the outgoing queue overhead.
- What if the outgoing data queue becomes large?
  - This is an indication that the remote end is not processing data fast enough (or that data is being produced too fast). The application can query outgoing queue stats to determine if this scenario is occurring.
- Why not provide a configuration API for a table-driven network application?
  - There are many different formats and types of configurations, including dynamically generated configurations. Configuration should be a separate concern from the Chops Net IP library. Configuration parsing for common formats (e.g. JSON) may be added to the `component` directory (non-dependent convenience classes and functions) in the future.
- Is Chops Net IP a complete wrapper over the C++ Networking TS?
  - No. There are access points that expose Networking TS internals. In particular, Chops Net IP provides an interface to access the underlying Networking TS socket and the application can directly set (or query) socket options (versus wrapping and exactly duplicating the functionality).
- What are some of the subtle design challenges in the implementation?
  - Passing application supplied function objects through the library layers is central to the design. Since these are passed across thread boundaries at certain points, knowing when to call `std::move` versus `std::forward<F>` is crucial (and has been the source of more than one bug).
  - The shutdown notification logic is tricky and hard to get correct, specially between the TCP connection object and the TCP acceptor and TCP connector objects.
  - `std::atomic` variables can be used for critical section locks, but care must be taken, and the Chops Net IP implementation uses the `compare_exchange_strong` method on a bool atomic to guard against multiple threads calling `start` or `stop` at the same time.


## States and Transitions

Chops Net IP states and transitions match existing standard network protocol behavior. For example, when a TCP connector is created, an actual TCP data connection does not exist until the connect succeeds. When this happens (connect succeeds), the abstract state transitions from unconnected to connected. In Chops Net IP, when a TCP connector connects, a data connection object is created and an application state transition function object callback is invoked containing the connection object.

Even though an implicit state transition table exists within the Chops Net IP library (matching network protocol behavior), there are not any explicit state flags or methods to query the state through the API. Instead, state transitions are handled through application supplied function object callbacks, which notify the application that something interesting has happened and containing objects for further interaction and processing. In other words, there is not an "is_connected" method with the Chops Net IP library. Instead, an application can layer its own state on top of Chops Net IP (if desired), using the function object callbacks to manage the state.

Pro tip - Chops Net IP follows the implicit state model of the Networking TS and Asio (and similar) libraries where state transitions are implemented through chaining function object callbacks on asynchronous operations. Developers familiar with implicit or explicit state transition models will be familiar with the application model defined for Chops Net IP. Chops Net IP insulates the application from the intricacies of the Networking TS API and simplifies the state transition details.

(State transition diagram to be inserted here.)

## Constraints

Chops Net IP works well with the following communication patterns:

- Receive data, process it quickly (which may involve passing data along to another thread), become ready for more incoming data.
- Receive data, process it quickly (as above), send data back through the same connection or endpoint, become ready for more incoming data.
- Send data, go back to other work.

Chops Net IP requires more work with the following communication pattern:

- Send data, wait for reply (request-reply pattern). Everything in Chops Net IP is no-wait from the application perspective, so request-reply must be emulated through application logic (e.g. store some form of message transaction id in an outgoing table and correlate incoming data using the message transaction id, or track outstanding requests by connection address).

Chops Net IP works extremely well in environments where there might be a lot of network connections (e.g. thousands), each with a moderate amount of traffic, and each with different kinds of data or data processing. In environments where each connection is very busy, or a lot of processing is required for each incoming message (and it cannot be passed along to another thread), then more traditional communication patterns or designs might be appropriate (e.g. blocking or synchronous I/O, or "thread per connection" models).

Applications that do only one thing and must do it as fast as possible with the least amount of overhead might not want the abstraction penalties and overhead of Chops Net IP. For example, a high performance server application where buffer lifetimes for incoming data are easily managed might not want the queuing and "shared buffer" overhead of Chops Net IP.

Applications that need to perform time consuming operations on incoming data and cannot pass that data off to another thread may encounter throughput issues. Multiple threads or thread pools or strands interacting with the event loop method (executor) may be a solution in those environments.

## Application Customization Points

Chops Net IP strives to provide an application customization point (via function object) for every important step in the network processing chain. These include:

- Message framing customization - decoding a header and determining how to read the rest of the message.
- Message handling customization - processing a message once it arrives.
- IO state change customization - steps to perform when:
  - A connection or network endpoint goes down, whether by error or by graceful close.
  - A connection or network endpoint becomes available.

### State Change Customization Point

The state change "IO is ready" (i.e. a TCP connection has been created, or UDP endpoint is ready) callback interface is consistent across all protocol types and provides:

- IO object
  - The application calls `start_io` to initiate read processing and enable send processing. This object can then be copied and used for sending data (as needed; read processing is provided an IO object for "reply" data sending).
- Count of active IO objects
  - This provides additional information for TCP acceptors (i.e. number of active connections).

When an error or shutdown condition occurs (e.g. a TCP connection has been shutdown or broken), a "shutdown" callback interface is invoked, providing:

- IO object
  - The application can use this to correlate with the "IO is ready" condition and remove the IO object from any containers.
- Error code
  - The error code may contain a system error or a Chops Net IP error condition (e.g. for a graceful shutdown).
- Count of active IO objects (which may now be 0).


### TCP Message Frame Customization Point

A message frame customization point provides logic that determines when a message beings and ends. Typically a header is decoded which then determines the following message body len. This may be a complicated process with nested levels of header and body decoding.

Not all TCP applications need message framing logic. For example, many Internet protocols define a message delimeter at the end of a stream of bytes (e.g. a `newline` or similar sequence of bytes). Chops Net IP allows this alternative for message framing.

A non-trivial amount of decoding may be needed for message framing. There are multiple designs that allow the message framing state to be passed along to the message handling function object.

### Message Handling Customization Point

A message handling callback interface is consistent across all protocol types, although templatized between TCP and UDP.

An IO object is provided for "send data back to the originator" logic. The remote endpoint is provided to enable this for UDP (for TCP connections, the remote endpoint is always the same and is ignored when replying with data).

The full incoming byte buffer (message) is always provided to the message handling callback.

## Library Implementation Design Considerations

Reference counting (through `std::shared_ptr` facilities) is an aspect of many of the internal (`detail` namespace) Chops Net IP classes. This simplifies the lifetime management of all of the objects at the expense of the reference counting overhead.

Future versions of the library may have more move semantics and less reference counting, but will always implement safety over performance.

Most of the Chops Net IP public classes use `std::weak_ptr` references to the internal reference counted objects. This means that application code which ignores state changes (e.g. a TCP connection that has ended) will have an exception thrown by the Chops Net IP library when trying to access a non-existent object (e.g. trying to send data through a TCP connection that has gone away). This is preferred to "dangling pointers" that result in process crashes or requiring the application to continually query the Chops Net IP library for state information.

Where to provide the customization points in the API is one of the most crucial design choices. Using template parameters for function objects and passing them through call chains is preferred to storing the function object in a `std::function`. This does affect the API choices, and results in the `start` methods having many callback parameters.

Since data can be sent at any time and at any rate by the application, a sending queue is required. The queue can be queried to find out if congestion is occurring.

Mutex locking is kept to a minimum in the library. Instead, most of the internal handler classes take incoming parameters and post the data through the io context. This allows multiple threads to be calling into one internal handler and as long as the parameter data is thread-safe (which it is), thread safety is managed by the Networking TS executor and post queue code.

In the areas where data is directly accessed, it is protected by `std::atomic` wraps. For example, outgoing queue statistics and `is_started` flags are all `std::atomic`. While this guarantees that applications will not crash, it does mean that statistics might have temporary inconsistency with each other. For example, an outgoing buffer might be popped exactly between an application querying and accessing two outgoing counters. This potential inconsistency is not considered to be an issuse, since the queue counters are only meant for general congestion queries, not exact statistical gathering.

## Future Directions

- Older compiler (along with older C++ standard) support is likely to be implemented sooner than later depending on availability and collaboration support.
- The internal queue container may become a template parameter if the flexibility is needed. This would allow circular buffers (ring spans) or other data structures to be used instead of the default `std::queue` (which is instantiated to use a `std::deque`).
- SSL support may be added, depending on collaborators with expertise being available.
- Additional protocols may be added, but would be in a separate library (Bluetooth, serial I/O, MQTT, etc). Chops Net IP focuses on TCP, UDP unicast, and UDP multicast support. If a reliable UDP multicast protocol is popular enough, support may be added.

