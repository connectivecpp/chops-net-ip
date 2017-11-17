#Periodic Timer

If the application cares about precise (as possible) periodicity, timer design must adjust for processing time within the timer callback code as well as operating environment imprecision. For example, an application may desire a timer callback to be invoked once every 500 milliseconds and the callback takes 15 milliseconds to excecute. Also occasionally the operating environment takes an extra 10 or 20 milliseconds before invoking the callback. Without adjustment the actual interval is now 515 milliseconds between callback invocations, with occasional intervals up to 535 milliseconds. The Periodic Timer class adjusts for these slippages, up to the precision allowed by the system and environment.

An excellent article on this topic can be [read here](http://bulldozer00.com/2013/12/27/periodic-processing-with-standard-c11-facilities/).

An asynchronous timer is more resource-friendly regarding system resources than creating a thread that sleeps. In particular, creating hundreds or thousands of timers is very expensive in a "thread per timer" design.

Periodic Timer is a class corresponding to a single timer object. Designs needing many timer objects, possibly sorted by time (e.g. a priority queue based on a time stamp) will need to create their own containers and data structures, using Periodic Timer as a container element.

Asynchronous processing is performed by the `io_context` (C++ executor) passed in to the constructor by the application.

