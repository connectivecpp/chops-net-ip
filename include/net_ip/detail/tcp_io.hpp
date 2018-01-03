/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Internal handler class for TCP stream input and output.
 *
 *  @note For internal use only.
 *
 *  @author Cliff Green
 *  @date 2017, 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef TCP_IO_HPP_INCLUDED
#define TCP_IO_HPP_INCLUDED

#include <experimental/executor>
#include <experimental/internet>
#include <experimental/buffer>

#include <memory> // std::shared_ptr, std::enable_shared_from_this
#include <system_error>

#include <cstddef> // std::size_t
#include <utility> // std::forward, std::move
#include <string>
#include <string_view>

#include "net_ip/detail/output_queue.hpp"
#include "net_ip/queue_stats.hpp"
#include "net_ip/net_ip_error.hpp"
#include "net_ip/io_interface.hpp"
#include "utility/shared_buffer.hpp"

namespace chops {
namespace net {
namespace detail {

class tcp_io : std::enable_shared_from_this<tcp_io> {
public:
  using socket_type = std::experimental::net::ip::tcp::socket;
  using protocol_type = std::experimental::net::ip::tcp;
  using byte_vec = chops::mutable_shared_buffer::byte_vec;

private:
  using entity_notifier_cb = std::function<void (const std::error_code&, std::shared_ptr<tcp_io>)>;
  using namespace std::experimental::net;

private:

  socket_type            m_socket;
  io_common<tcp_io>      m_io_common;
  // the following members could be passed through handlers, but are stored here
  // for simplicity
  byte_vec               m_byte_vec;
  dynamic_vector_buffer  m_dyn_buf;
  std::string            m_delimiter;

public:

  tcp_io(socket_type sock, entity_notifier_cb cb) noexcept : 
    m_socket(std::move(sock)), m_io_common(cb), m_byte_vec(), m_dyn_buf(m_byte_vec), m_delimiter() { }

public:
  // all of the methods in this public section can be called through an io_interface
  socket_type& get_socket() noexcept { return m_socket; }

  chops::net::queue_stats get_output_queue_stats() const noexcept {
    return m_io_common.get_output_queue_stats();
  }

  bool is_started() const noexcept { return m_io_common.is_started(); }

  template <typename MH, typename MF>
  void start_io(MH && msg_handler, MF&& msg_frame, std::size_t header_size);

  template <typename MH>
  void start_io(MH&& msg_handler, std::string_view delimiter);

  template <typename MH>
  void start_io(MH&& msg_handler, std::size_t read_size);

  void start_io();

  void stop_io() {
    m_io_common.check_err_code(std::make_error_code(net_ip_errc::io_handler_stopped), 
                               std::shared_from_this());
  }

  // use post for thread safety, multiple threads can call this method
  void send(chops::shared_const_buffer buf) {
    auto self { std::shared_from_this() };
    post(m_socket.get_executor(), [this, self, buf] { start_write(buf); } );
  }

public:
  // this method can only be called through a net entity, assumes all error codes have already
  // been reported back to the net entity
  void close();

private:

  template <typename MH, typename MF>
  void handle_read(MH&&, MF&&, std::size_t, const std::error_code& err, std::size_t);

  template <typename MH>
  void handle_read(MH&&, const std::error_code& err, std::size_t);

  void handle_read(const std::error_code& err, std::size_t);

  void start_write(chops::shared_const_buffer);

  void handle_write(const std::error_code&, std::size_t);

};

// method implementations, just to make the class declaration a little more readable

inline void tcp_io::close() {
  m_io_common.stop();
  m_socket.shutdown(); // attempt graceful shutdown
  auto self { std::shared_from_this() };
  post(m_socket.get_executor(), [this, self] { m_socket.close(); } );
}

template <typename MH, typename MF>
void tcp_io::start_io(MH&& msg_hdlr, MF&& msg_frame, std::size_t header_size) {

  if (!m_io_common.start_io_setup()) {
    return;
  }
  m_byte_vec.resize(header_size);
  auto self { std::shared_from_this() };
  async_read(m_socket, mutable_buffer(m_byte_vec),
    [this, self, mh = std::forward(msg_hdlr), mf = std::forward(msg_frame), header_size]
          (const std::error_code& err, std::size_t nb) {
      handle_read(mh, mf, header_size, err, nb);
    }
  );
}

template <typename MH, typename MF>
void tcp_io::handle_read(MH&& msg_hdlr, MF&& msg_frame, 
                 std::size_t header_size, const std::error_code& err, std::size_t num_bytes) {

  if (!m_io_common.check_err_code(err, std::shared_from_this())) {
    return;
  }
  // assert num_bytes == m_byte_vec.size()
  mutable_buffer mbuf;
  const_buffer cb(m_byte_vec.data(), m_byte_vec.size());
  std::size_t next_read_size = msg_frame(cb);
  if (next_read_size == 0) { // msg fully received, now invoke message handler
    if (!msg_hdlr(cb, chops::net::io_interface<tcp_io>(std::weak_from_this()), m_io_common.get_remote_endp(), msg_frame)) {
      // message handler not happy, tear everything down
      m_io_common.check_err_code(std::make_error_code(net_ip_errc::message_handler_terminated), 
                                 std::shared_from_this());
      return;
    }
    m_byte_vec.resize(header_size);
    mbuf = mutable_buffer(buf.data(), buf.size());
  }
  else {
    std::size_t old_size = m_byte_vec.size();
    m_byte_vec.resize(old_size + next_read_size);
    mbuf = mutable_buffer(m_byte_vec.data() + old_size, next_read_size);
  }
  // start another read
  auto self { std::shared_from_this() };
  async_read(m_socket, mbuf,
    [this, self, mh = std::forward(msg_hdlr), mf = std::forward(msg_frame), header_size]
          (const std::error_code& err, std::size_t nb) {
      handle_read(mh, mf, header_size, err, nb);
    }
  );
}

template <typename MH>
void tcp_io::start_io(MH&& msg_hdlr, std::string_view delim) {

  if (!m_io_common.start_io_setup()) {
    return;
  }
  m_delimiter = delim;

  auto self { std::shared_from_this() };
  async_read_until(m_socket, m_dyn_buf, m_delimiter,
        [this, self, mh = std::forward(msg_hdlr)] (const std::error_code& err, std::size_t nb) {
      handle_read(mh, err, nb);
    }
  );
}

template <typename MH>
void tcp_io::handle_read(MH&& msg_hdlr, const std::error_code& err, std::size_t num_bytes) {

  if (!m_io_common.check_err_code(err, std::shared_from_this())) {
    return;
  }
  // m_dyn_buf contains up to delimiter
  if (!msg_hdlr(m_dyn_buf.data(), chops::net::io_interface<tcp_io>(std::weak_from_this()), m_io_common.get_remote_endp())) {
      m_io_common.check_err_code(std::make_error_code(net_ip_errc::message_handler_terminated), 
                                 std::shared_from_this());
    return;
  }
  m_dyn_buf.consume(num_bytes);

  auto self { std::shared_from_this() };
  async_read_until(m_socket, m_dyn_buf, delimiter, 
          [this, self, mh = std::forward(msg_hdlr)] (const std::error_code& err, std::size_t nb) {
      handle_read(mh, err, nb);
    }
  );
}

template <typename MH>
void tcp_io::start_io(MH&& msg_handler, std::size_t read_size) {
  auto wrapped_mh = [mh = std::forward(msg_handler)] 
               (const_buffer cb, chops::net::io_interface<tcp_io> io, endpoint endp, null_msg_frame) {
    return mh(cb, io, endp);
  };
  start_io(wrapped_mh, null_msg_frame, read_size);
}

inline void tcp_io::start_io() {

  auto msg_hdlr = [] (const_buffer, chops::net::io_interface<tcp_io>, endpoint, null_msg_frame) {
    return true;
  };
  start_io(msg_hdlr, null_msg_frame, 1);
}

inline void tcp_io::start_write(chops::shared_const_buffer buf) {
  if (!m_io_common.start_write_setup(buf)) {
    return; // buf queued or shutdown happening
  }
  auto self { std::shared_from_this() };
  async_write(m_socket, buf, [this, self] 
      (const std::system::error_code& err, std::size_t nb) { handle_write(err, nb); }
  );
}

inline void tcp_io::handle_write(const std::system::error_code& err, std::size_t /* num_bytes */) {
  if (err) {
    return;
  }
  auto elem = m_io_common.get_next_element();
  if (!elem) {
    return;
  }
  auto self { std::shared_from_this() };
  async_write(m_socket, elem->first, [this, self]
      (const std::system::error_code& err, std::size_t nb) { handle_write(err, nb); }
}

} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

