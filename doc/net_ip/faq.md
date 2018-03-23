# FAQ

- Is Chops Net IP a framework?
  - No. It is a general purpose library. There are no specific network protocols required by the library and no wire protocols added by the library. It can communicate with any TCP or UDP application. Obviously the wire protocols and communication semantics need to be appropriately implemented by the application using the Chops Net IP library.
- Wny is a queue required for outgoing data?
  - Applications may send data faster than it can be consumed at the remote end (or passed through the local network stack). Queueing the outgoing data allows timely processing if this situation occurs. One design possibility for the library is to push the responsibility of sending the next chunk of data back to the application, but this requires a non-trivial API interaction between the library and the application. "Fire and forget" makes for an easy application API at the cost of the outgoing queue overhead.
- What if the outgoing data queue becomes large?
  - This is an indication that the remote end is not processing data fast enough (or that data is being produced too fast). The application can query outgoing queue stats to determine if this scenario is occurring.
- Why not provide a configuration API for a table-driven network application?
  - There are many different formats and types of configurations, including dynamically generated configurations. Configuration should be a separate concern from the Chops Net IP library. Configuration parsing for common formats (e.g. JSON) may be added to the `component` directory (non-dependent convenience classes and functions) in the future.
- Is Chops Net IP a complete wrapper over the C++ Networking TS?
  - No. There are access points that expose Networking TS internals, and Networking TS `endpoints` are used in the Chops Net IP API. In particular, Chops Net IP provides an interface to access the underlying Networking TS socket and the application can directly set (or query) socket options (versus wrapping and exactly duplicating the functionality).
- What are some of the subtle design challenges in the implementation?
  - Passing application supplied function objects through the library layers is central to the design. Since these are passed across thread boundaries at certain points, knowing when to call `std::move` versus `std::forward<F>` is crucial (and has been the source of more than one bug).
  - The shutdown notification logic is tricky and hard to get correct, specially between the TCP connection object and the TCP acceptor and TCP connector objects.
  - `std::atomic` variables can be used for critical section locks, but care must be taken, and the Chops Net IP implementation uses the `compare_exchange_strong` method on a bool atomic to guard against multiple threads calling `start` or `stop` at the same time.

