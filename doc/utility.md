# Utilities Detailed Overview

## Shared Buffer

There are two concrete classes:
- `mutable_shared_buffer`, a reference counted modifiable buffer class with convenience methods for appending data.
- `const_shared_buffer`, a reference counted non-modifiable buffer class. Once the object is constructed, it cannot be modified.

Interally all data is stored in a `std::vector` of `std::byte`s. There are ordering methods so that shared buffer objects can be stored in sequential or associative containers.

Efficient moving of data (versus copying) is enabled in multiple ways, including:
- Allowing a `const_shared_buffer` to be move constructed from a `mutable_shared_buffer`.
- Allowing a `std::vector` of `std::byte`s to be moved into either shared buffer type.

The implementation is adapted from Chris Kohlhoff's reference counted buffer examples (see [References Section](../README.md#references)). 


