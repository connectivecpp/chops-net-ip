# Queue Detailed Overview

## Wait Queue

Multiple writer and reader threads can access a Wait Queue simultaneously, although when a value is pushed on the queue, only one reader thread will be notified to consume the value.

Close semantics are simple, and consist of setting an internal flag and notifying all waiting reader threads. Subsequent pushes are disallowed (an error is returned on the push). On close, data is *not* flushed (i.e. elements remaining in the queue will be destructed when the Wait Queue object is destructed, as typical in C++). A closed Wait Queue can be reopened by calling `open`.

Wait Queue uses C++ standard library concurrency facilities (mutex, condition variables) in its implementation. It is not a lock-free queue, but it has been designed to be used in memory constrained environments or where deterministic performance is needed. In particular, Wait Queue:

- Has been tested with Martin Moene's `ring_span` library for the internal container (see [References Section](../README.md#references)). A `ring_span` is traditionally known as a "ring buffer" or "circular buffer". This implies that the Wait Queue can be used in environments where dynamic memory management (heap) is not allowed or is problematic. In particular, no heap memory is directly allocated within the Wait Queue.

- Has been tested with Boost Circular Buffer. Boost Circular Buffer (as of Boost 1.65.1) does not support `emplace`, so the `emplace_push` method cannot be used when the Wait Queue is instantiated with a Boost Circular Buffer. 

- Does not throw or catch exceptions anywhere in its code base. Elements passed through the queue may throw exceptions, which must be handled at an application level. Exceptions may be thrown by C++ std library concurrency calls (`std::mutex` locks, etc), although this usually indicates an application design issue or issues at the operating system level.

- If the C++ std library concurrency calls become `noexcept`, every Wait Queue method will become `noexcept` or conditionally `noexcept` (depending on the type of the data passed through the Wait Queue).

The only requirement on the type passed through a Wait Queue is that it supports either copy construction or move construction. In particular, a default constructor is not required (this is enabled by using `std::optional`, which does not require a default constructor).

The implementation is adapted from the book Concurrency in Action, Practical Multithreading, by Anthony Williams (see [References Section](../README.md#references)). 

The core logic in this library is the same as provided by Anthony in his book, but the API has changed and additional features added. The name of the utility class template in Anthony's book is `threadsafe_queue`.


