# Periodic Timer

The C++ Networking TS does not directly provide periodic timers. Instead, application code using the Networking TS must chain together function object callbacks until satisfied.

The Periodic Timer class is a thin wrapper supplying the chaining, with two options for the periodicity. In addition, elapsed times are computed and provided to the application supplied callback.

One periodicity option is to invoke the application callback after a duration. It is simple and provides consistency, specially if the system clock is allowed to be adjusted.

The second periodicity option is to invoke the application callback based on timepoints. This works well in environments where activities must be processed on regular time intervals, regardless of how much processing is performed in each application callback. For example, an application may desire a timer callback to be invoked once every 500 milliseconds and the callback takes 15 milliseconds to excecute. Also occasionally the operating environment takes an extra 10 or 20 milliseconds before invoking the callback. Using a duration instead of a timepoint the actual interval is now 515 milliseconds between callback invocations, with occasional intervals up to 535 milliseconds. A timepoint timer will instead invoke the callback every 500 milliseconds (depending on operating system precision and load) regardless of how much time is taken by the previous callback.

The main disadvatange of timepoint based callbacks is that they can be affected by system clock adjustments. In addition, if the timepoint interval is small and a large amount of processing is performed by the callback, "overflow" can occur, where the next timepoint callback is overrun by the current processing.

An excellent article on this topic by Tony DaSilva can be [read here](http://bulldozer00.com/2013/12/27/periodic-processing-with-standard-c11-facilities/).

An asynchronous timer is more resource-friendly regarding system resources than creating a thread that sleeps. In particular, creating hundreds or thousands of timers is very expensive in a "thread per timer" design.

Periodic Timer is a class corresponding to a single timer object. Designs needing many periodic timer objects, possibly sorted by time (e.g. a priority queue based on a time stamp) will need to create their own containers and data structures, using Periodic Timer as a container element.

Asynchronous processing is performed by the `io_context` (C++ executor context) passed in to the constructor by the application.

