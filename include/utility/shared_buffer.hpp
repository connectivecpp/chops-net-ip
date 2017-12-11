/** @file
 *
 *  @ingroup utility_module
 *
 *  @brief Reference counted byte buffer classes, both const and mutable versions.
 *
 *  The @c mutable_shared_buffer and @c const_shared_buffer classes provide byte
 *  buffer classes with internal reference counting. These classes are used within 
 *  the Chops Net library to manage data buffer lifetimes. The @c mutable_shared_buffer 
 *  class can be used to construct a data buffer, and then a @c const_shared_buffer can 
 *  be move constructed from the @c mutable_shared_buffer for use with the asynchronous 
 *  library functions (whether Chops Net or C++ Networking TS or Asio). Besides the 
 *  data buffer lifetime management, these utility classes eliminate data buffer copies.
 *
 *  This code is based on and modified from Chris Kohlhoff's Asio example code. It has
 *  been significantly modified by adding a @c mutable_shared_buffer class as well as 
 *  adding convenience methods to the @c shared_const_buffer class.
 *
 *  It is likely that this shared buffer design and code will change as the C++ 
 *  Networking TS buffer features are expanded, changed, or better understood.
 *
 *  Example code:
 *  @code
 *    ... fill in example code
 *  @endcode
 *
 *  @note Everything is declared @c noexcept except for the methods that allocate
 *  memory and might throw a memory exception. This is tighter than the @c noexcept
 *  declarations on the underlying @c std::vector methods, since @c std::byte 
 *  operations will never throw.
 *
 *  @authors Cliff Green, Chris Kohlhoff
 *  @date 2017
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef SHARED_BUFFER_HPP_INCLUDED
#define SHARED_BUFFER_HPP_INCLUDED

#include <experimental/buffer> // Networking TS include

#include <cstddef> // std::byte
#include <vector>
#include <memory> // std::shared_ptr

#include <utility> // std::move
#include <cstring> // std::memcpy

namespace chops {

class const_shared_buffer;

/**
 *  @brief A mutable (modifiable) byte buffer class with convenience methods, internally 
 *  reference-counted for efficient copying.
 *
 *  This class provides ownership, copying, and lifetime management for byte oriented 
 *  buffers. In particular, it is designed to be used in conjunction with the 
 *  @c const_shared_buffer class for efficient transfer and correct lifetime management 
 *  of buffers in asynchronous libraries (such as the C++ Networking TS). In particular, 
 *  a reference counted buffer can be passed among multiple layers of software without 
 *  any one layer "owning" the buffer.
 *
 *  A std::byte pointer returned by the @c data method may be invalidated if the 
 *  @c mutable_shared_buffer is modified in any way (this follows the usual constraints
 *  on @c std::vector iterator invalidation).
 *
 *  This class is similar to @c const_shared_buffer, but with mutable characteristics.
 *  While the mutable version can be used with the C++ Networking TS facilities, the 
 *  @c const_shared_buffer class is specifically designed to be used with 
 *  C++ Networking TS buffers.
 *
 *  @invariant There will always be an internal buffer of data, even if the size is zero.
 *
 *  @note Modifying the underlying buffer of data (for example by writing bytes using the 
 *  @c data method, or appending data) will show up in any other @c mutable_shared_buffer 
 *  objects that have been copied to or from the original object.
 *
 *  @ingroup utility_module
 *
 */

class mutable_shared_buffer {
public:
  using byte_vec = std::vector<std::byte>;
  using size_type = typename byte_vec::size_type;

private:
  std::shared_ptr<byte_vec> m_data;

private:

  friend class const_shared_buffer;

  friend bool operator==(const mutable_shared_buffer&, const mutable_shared_buffer&) noexcept;
  friend bool operator<(const mutable_shared_buffer&, const mutable_shared_buffer&) noexcept;

  friend bool operator==(const mutable_shared_buffer&, const const_shared_buffer&) noexcept;
  friend bool operator==(const const_shared_buffer&, const mutable_shared_buffer&) noexcept;

public:

  // default copy and move construction, copy and move assignment
  mutable_shared_buffer(const mutable_shared_buffer&) = default;
  mutable_shared_buffer(mutable_shared_buffer&&) = default;
  mutable_shared_buffer& operator=(const mutable_shared_buffer&) = default;
  mutable_shared_buffer& operator=(mutable_shared_buffer&&) = default;

/**
 *  @brief Default construct the @c mutable_shared_buffer.
 *
 */
  mutable_shared_buffer() noexcept : mutable_shared_buffer(size_type(0)) { }

/**
 *  @brief Construct a @c mutable_shared_buffer with an initial size, contents
 *  set to zero.
 *
 *  Allocate zero initialized space which can be overwritten with data as needed.
 *  The @c data method is called to get access to the underlying @c std::byte 
 *  buffer.
 *
 *  @param sz Size for internal @c std::byte buffer.
 */
  explicit mutable_shared_buffer(size_type sz)
    : m_data(std::make_shared<byte_vec>(sz)) { }

/**
 *  @brief Construct by copying from a @c std::byte array.
 *
 *  @pre Size cannot be greater than the source buffer.
 *
 *  @param buf @c std::byte array containing buffer of data. The data is
 *  copied into the internal buffer of the @c mutable_shared_buffer.
 *
 *  @param sz Size of buffer.
 */
  mutable_shared_buffer(const std::byte* buf, size_type sz)
    : m_data(std::make_shared<byte_vec>(buf, buf+sz)) { }

/**
 *  @brief Construct by copying bytes from an arbitrary pointer.
 *
 *  The pointer passed into this constructor is cast into a @c std::byte pointer and bytes 
 *  are then copied. In particular, this method can be used for @c char pointers, 
 *  @c void pointers, @ unsigned @c char pointers, etc.
 *
 *  @param buf Pointer to a buffer of data. The pointer must be convertible 
 *  to a @c void pointer and then to a @c std::byte pointer.
 *
 *  @param sz Size of buffer, in bytes.
 */
  template <typename T>
  mutable_shared_buffer(const T* buf, size_type sz)
    : mutable_shared_buffer(static_cast<const std::byte*>(static_cast<const void*>(buf)), sz) { }

/**
 *  @brief Construct from input iterators.
 *
 *  @pre Valid iterator range, where each element is convertible to a @c std::byte.
 *
 *  @param beg Beginning input iterator of range.
 *  @param end Ending input iterator of range.
 *
 */
  template <typename InIt>
  mutable_shared_buffer(InIt beg, InIt end)
    : m_data(std::make_shared<byte_vec>(beg, end)) { }

/**
 *  @brief Return @c std::byte pointer to beginning of buffer.
 *
 *  This method provides pointer access to the beginning of the buffer. If the
 *  buffer is empty the pointer cannot be dereferenced or undefined behavior will 
 *  occur.
 *
 *  Accessing past the end of the internal buffer (as defined by the @c size() 
 *  method) results in undefined behavior.
 *
 *  @return @c std::byte pointer to buffer.
 */
  std::byte* data() noexcept { return m_data->data(); }

/**
 *  @brief Return @c const @c std::byte pointer to beginning of buffer.
 *
 *  This method provides pointer access to the beginning of the buffer, same as 
 *  the non-const method, but providing @c const access.

 *  @return @c const @c std::byte pointer to buffer.
 */
  const std::byte* data() const noexcept { return m_data->data(); }

/**
 *  @brief Return size (number of bytes) of buffer.
 *
 *  @return Size of buffer, which may be zero.
 */
  size_type size() const noexcept { return m_data->size(); }

/**
 *  @brief Query to see if size is zero.
 *
 *  @return @c true if empty (size equals zero).
 */
  bool empty() const noexcept { return m_data->empty(); }

/**
 *  @brief Clear the internal contents back to an empty state.
 *
 *  This method is handy after a @c mutable_shared_buffer has been moved into 
 *  another object (e.g. a @c const_shared_buffer). At that point the contents
 *  are in a consistent but unknown state. Calling @c clear puts the internal
 *  buffer into a known and empty state.
 *
 */
  void clear() noexcept { m_data->clear(); }

/**
 *  @brief Resize internal buffer.
 *
 *  @param sz New size for buffer. If the buffer is expanded, new bytes are added,
 *  each zero initialized. The size can also be contracted. @c resize does not 
 *  destroy old data in the internal buffer, so @c clear may need to be called first.
 *
 *  Resizing to zero results in an empty buffer, although calling @c clear is 
 *  preferred.
 */
  void resize(size_type sz) { m_data->resize(sz); }

/**
 *  @brief Swap with the contents of another @c mutable_shared_buffer object.
 */
  void swap(mutable_shared_buffer& rhs) noexcept { m_data.swap(rhs.m_data); }

/**
 *  @brief Append a @c std::byte buffer to the end of the internal buffer.
 *
 *  @param buf @c std::byte array containing buffer of data.
 *
 *  @param sz Size of buffer.
 *
 *  @return Reference to @c this (to allow method chaining).
 */
  mutable_shared_buffer& append(const std::byte* buf, size_type sz) {
    size_type old_sz = size();
    resize(old_sz + sz); // set up buffer space
    std::memcpy(data() + old_sz, buf, sz);
    return *this;
  }

/**
 *  @brief Append the contents of another @c mutable_shared_buffer to the end.
 *
 *  @param rhs @c mutable_shared_buffer to append from.
 *
 *  @return Reference to @c this (to allow method chaining).
 */
  mutable_shared_buffer& append(const mutable_shared_buffer& rhs) {
    return append(rhs.data(), rhs.size());
  }

/**
 *  @brief Append the contents of another @c mutable_shared_buffer to the end.
 *
 *  See @c append method for details.
 */
  mutable_shared_buffer& operator+=(const mutable_shared_buffer& rhs) {
    return append(rhs);
  }

/**
 *  @brief Append a single @c std::byte to the end.
 *
 *  @param b Byte to append.
 *
 *  @return Reference to @c this (to allow method chaining).
 */
  mutable_shared_buffer& append(std::byte b) {
    return append(&b, 1);
  }

/**
 *  @brief Append a single @c std::byte to the end.
 *
 *  See @c append method (single @c std::byte) for details.
 */
  mutable_shared_buffer& operator+=(std::byte b) {
    return append(b);
  }

}; // end mutable_shared_buffer class

// non-member functions
/**
 *  @brief Swap two @c mutable_shared_buffer objects.
 *
 *  @relates mutable_shared_buffer
 */

inline void swap(mutable_shared_buffer& lhs, mutable_shared_buffer& rhs) noexcept {
  lhs.swap(rhs);
}

/**
 *  @brief Compare two @c mutable_shared_buffer objects for internal buffer 
 *  byte-by-byte equality.
 *
 *  Internally this invokes the @c std::vector @c operator== on @c std::byte 
 *  elements.
 *
 *  @return @c true if @c size() same for each, and each byte compares @c true.
 *
 *  @relates mutable_shared_buffer
 */
inline bool operator== (const mutable_shared_buffer& lhs, const mutable_shared_buffer& rhs) noexcept { 
  return *(lhs.m_data) == *(rhs.m_data);
}  

/**
 *  @brief Compare two @c mutable_shared_buffer objects for inequality.
 *
 *  @return Opposite of @c operator==.
 *
 *  @relates mutable_shared_buffer
 */
inline bool operator!= (const mutable_shared_buffer& lhs, const mutable_shared_buffer& rhs) noexcept {
  return !(lhs == rhs);
}

/**
 *  @brief Compare two @c mutable_shared_buffer objects for internal buffer 
 *  byte-by-byte less-than ordering.
 *
 *  Internally this invokes the @c std::vector @c operator< on @c std::byte 
 *  elements.
 *
 *  @return @c true if internal buffer of left is less than internal buffer of right.
 *
 *  @relates mutable_shared_buffer
 */
inline bool operator< (const mutable_shared_buffer& lhs, const mutable_shared_buffer& rhs) noexcept { 
  return *(lhs.m_data) < *(rhs.m_data);
}  

/**
 *  @brief A reference counted non-modifiable buffer class with various convenience methods, 
 *  providing efficient copying and convenient buffer lifetime management. 
 *
 *  The primary difference between this class and the @c mutable_shared_buffer class is that
 *  once a @c const_shared_buffer object is constructed, nothing inside it can be modified. This 
 *  allows it to be used with asynchronous IO functions which depend on the buffer staying the 
 *  same (i.e. the internal pointer to the data and the size) for the full lifetime of the 
 *  asynchronous operations.
 *
 *  There are declarations and methods to make it compatible with the C++ Networking TS
 *  library (and possibly with Chris' Asio library).
 *
 *  @invariant There will always be an internal buffer of data, even if the size is zero.
 *
 *  @ingroup utility_module
 *
 */

class const_shared_buffer {
public:
  using byte_vec = std::vector<std::byte>;
  using size_type = typename byte_vec::size_type;

private:
  std::shared_ptr<byte_vec> m_data;
  std::experimental::net::const_buffer m_cbuf;

public:
  // Implement the Asio ConstBufferSequence requirements, allowing easy
  // use by Asio code for writing output buffers (which won't be modified)
//  typedef boost::asio::const_buffer value_type;
//  typedef const boost::asio::const_buffer* const_iterator;
//  const_iterator begin() const { return &ac_buf; }
//  const_iterator end() const { return &ac_buf + 1; }

private:

  friend bool operator==(const const_shared_buffer&, const const_shared_buffer&) noexcept;
  friend bool operator<(const const_shared_buffer&, const const_shared_buffer&) noexcept;

  friend bool operator==(const mutable_shared_buffer&, const const_shared_buffer&) noexcept;
  friend bool operator==(const const_shared_buffer&, const mutable_shared_buffer&) noexcept;

public:

  const_shared_buffer() = delete;

  // default copy and move construction, should do the right thing
  const_shared_buffer(const const_shared_buffer&) = default;
  const_shared_buffer(const_shared_buffer&&) = default;
  // copy and move assignment disabled
  const_shared_buffer& operator=(const const_shared_buffer&) = delete;
  const_shared_buffer& operator=(const_shared_buffer&&) = delete;

/**
 *  @brief Construct by copying from a @c std::byte array.
 *
 *  @pre Size cannot be greater than the source buffer.
 *
 *  @param buf @c std::byte array containing buffer of data. The data is
 *  copied into the internal buffer of the @c const_shared_buffer.
 *
 *  @param sz Size of buffer.
 */
  const_shared_buffer(const std::byte* buf, size_type sz)
    : m_data(std::make_shared<byte_vec>(buf, buf+sz)), m_cbuf(m_data->data(), m_data->size()) { }

/**
 *  @brief Construct by copying bytes from an arbitrary pointer.
 *
 *  The pointer passed into this constructor is cast into a @c std::byte pointer and bytes 
 *  are then copied. In particular, this method can be used for @c char pointers, 
 *  @c void pointers, @ unsigned @c char pointers, etc.
 *
 *  @param buf Pointer to a buffer of data. The pointer must be convertible 
 *  to a @c void pointer and then to a @c std::byte pointer.
 *
 *  @param sz Size of buffer, in bytes.
 */
  template <typename T>
  const_shared_buffer(const T* buf, size_type sz)
    : const_shared_buffer(static_cast<const std::byte*>(static_cast<const void*>(buf)), sz) { }

/**
 *  @brief Construct by copying from a @c mutable_shared_buffer object.
 *
 *  This constructor will copy from a @c mutable_shared_buffer. There is an alternative
 *  constructor that will move from a @c mutable_shared_buffer instead of copying.
 *  
 *  @param rhs @c mutable_shared_buffer containing bytes to be copied.
 */
  explicit const_shared_buffer(const mutable_shared_buffer& rhs)
    : const_shared_buffer(rhs.data(), rhs.size()) { }

/**
 *  @brief Construct by moving from a @c mutable_shared_buffer object.
 *
 *  This constructor will move from a @c mutable_shared_buffer into a @c const_shared_buffer. 
 *  This allows efficient API boundaries, where application code can construct and fill in a
 *  @c mutable_shared_buffer, then @c std::move it into a @c const_shared_buffer for use
 *  with asynchronous functions.
 *  
 *  @param rhs @c mutable_shared_buffer to be moved from; after moving the 
 *  @c mutable_shared_buffer will be empty.
 */
  explicit const_shared_buffer(mutable_shared_buffer&& rhs) noexcept
    : m_data(std::move(rhs.m_data)), m_cbuf(m_data->data(), m_data->size()) {
          rhs.m_data = std::make_shared<byte_vec>(0); // set rhs back to invariant
        }

/**
 *  @brief Construct from input iterators.
 *
 *  @pre Valid iterator range, where each element is convertible to a @c std::byte.
 *
 *  @param beg Beginning input iterator of range.
 *  @param end Ending input iterator of range.
 *
 */
  template <typename InIt>
  const_shared_buffer(InIt beg, InIt end)
    : m_data(std::make_shared<byte_vec>(beg, end)), m_cbuf(m_data->data(), m_data->size()) { }

/**
 *  @brief Return @c const @c std::byte pointer to beginning of buffer.
 *
 *  This method provides pointer access to the beginning of the buffer. If the
 *  buffer is empty the pointer cannot be dereferenced or undefined behavior will 
 *  occur.
 *
 *  Accessing past the end of the internal buffer (as defined by the @c size() 
 *  method) results in undefined behavior.
 *
 *  @return @c const @c std::byte pointer to buffer.
 */
  const std::byte* data() const noexcept { return m_data->data(); }

/**
 *  @brief Return size (number of bytes) of buffer.
 *
 *  @return Size of buffer, which may be zero.
 */
  size_type size() const noexcept { return m_data->size(); }

/**
 *  @brief Query to see if size is zero.
 *
 *  @return @c true if empty (size equals zero).
 */
  bool empty() const noexcept { return (*m_data).empty(); }

}; // end const_shared_buffer class

// non-member functions
/**
 *  @brief Compare two @c const_shared_buffer objects for internal buffer 
 *  byte-by-byte equality.
 *
 *  Internally this invokes the @c std::vector @c operator== on @c std::byte 
 *  elements.
 *
 *  @return @c true if @c size() same for each, and each byte compares @c true.
 *
 *  @relates const_shared_buffer
 */
inline bool operator== (const const_shared_buffer& lhs, const const_shared_buffer& rhs) noexcept { 
  return *(lhs.m_data) == *(rhs.m_data);
}  

/**
 *  @brief Compare two @c const_shared_buffer objects for inequality.
 *
 *  @return Opposite of @c operator==.
 *
 *  @relates const_shared_buffer
 */
inline bool operator!= (const const_shared_buffer& lhs, const const_shared_buffer& rhs) noexcept {
  return !(lhs == rhs);
}

/**
 *  @brief Compare two @c const_shared_buffer objects for internal buffer 
 *  byte-by-byte less-than ordering.
 *
 *  Internally this invokes the @c std::vector @c operator< on @c std::byte 
 *  elements.
 *
 *  @return @c true if internal buffer of left is less than internal buffer of right.
 *
 *  @relates const_shared_buffer
 */
inline bool operator< (const const_shared_buffer& lhs, const const_shared_buffer& rhs) noexcept { 
  return *(lhs.m_data) < *(rhs.m_data);
}  

/**
 *  @brief Compare a @c const_shared_buffer object with a @c mutable_shared_buffer for 
 *  internal buffer byte-by-byte equality.
 *
 *  @return @c true if @c size() same for each, and each byte compares @c true.
 */
inline bool operator== (const const_shared_buffer& lhs, const mutable_shared_buffer& rhs) noexcept { 
  return *(lhs.m_data) == *(rhs.m_data);
}  

/**
 *  @brief Compare a @c mutable_shared_buffer object with a @c const_shared_buffer for 
 *  internal buffer byte-by-byte equality.
 *
 *  @return @c true if @c size() same for each, and each byte compares @c true.
 */
inline bool operator== (const mutable_shared_buffer& lhs, const const_shared_buffer& rhs) noexcept { 
  return *(lhs.m_data) == *(rhs.m_data);
}  

} // end namespace

#endif

