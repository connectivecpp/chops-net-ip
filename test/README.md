# Contents of Chops Net IP Test

## Unit Tests

There is a `Catch2` main source in the `test` directory, which is used in building `Catch2` based unit tests. Each individual unit test 
is in a subdirectory corresponding to the production source (which is header-only).

All of the unit tests run on one host, with the networking unit tests using `localhost` (or equivalent).

### Shared Test

The `shared_test` directory contains common test code used in unit tests and other testing applications. 
## Distributed Test Applications

Since the networking unit tests run on `localhost`, there is a different set of applications in `test` which allow distributed testing. Typically these would be run on an internal private network, although they can also be run through public networks where the firewall 
and other security settings can be appropriately set.

### Data Blaster

The Data Blaster test application consists of two executables:
- Data sender / receiver
- Statistics and logging monitor

