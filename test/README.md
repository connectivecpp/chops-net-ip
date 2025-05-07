# Contents of Chops Net IP Test

## Unit Tests

All of the unit tests run on one host, with the networking unit tests using `localhost` (or equivalent).

### Shared Test

The `shared_test` directory contains common code used in the unit tests as well as the other testing applications. 

## Distributed Test Applications

Since the networking unit tests run on `localhost`, there is a another set of applications in `test` which allow distributed testing. Typically these would be run on an internal private network, although they can also be run through public networks where the firewall and other security settings can be appropriately set.

### Test Data Blaster (T-DB)

The [README](test_data_blaster/README.md) in the `test_data_blaster` directory contains details of the T-DB application.

