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

private:
  using entity_notifier_cb = std::function<void (const std::error_code&, std::shared_ptr<tcp_io>)>;

private:

  socket_type          m_socket;
  io_common<tcp_io>    m_io_common;


public:

  tcp_io(socket_type sock, entity_notifier_cb cb) noexcept : 
    m_socket(std::move(sock)), m_io_common(cb) { }

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

  void stop_io();

  // use post for thread safety, multiple threads can call this method
  void send(chops::shared_const_buffer buf) {
    auto self { std::shared_from_this() };
    std::experimental::net::post(m_socket.get_executor(), [this, self, buf] { start_write(buf); } );
  }

public:
  // this method can only be called through a net entity, assumes all error codes have already
  // been reported back to the net entity
  void close();

private:

  template <typename MH, typename MF>
  void handle_read(MH&&, MF&&, chops::mutable_shared_buffer, std::size_t, const std::error_code& err, std::size_t);

  template <typename MH>
  void handle_read(MH&&, chops::mutable_shared_buffer, const std::string& delimiter, const std::error_code& err, std::size_t);

  void handle_read(chops::mutable_shared_buffer, const std::error_code& err, std::size_t);

  void start_write(chops::shared_const_buffer);

  void handle_write(const std::error_code&, std::size_t);

};

// method implementations, just to make the class declaration a little more readable

inline void tcp_io::close() {
  m_io_common.stop();
  m_socket.shutdown(); // attempt graceful shutdown
  auto self { std::shared_from_this() };
  std::experimental::net::post(m_socket.get_executor(), [this, self] { m_socket.close(); } );
}

template <typename MH, typename MF>
void tcp_io::start_io(MH&& msg_hdlr, MF&& msg_frame, std::size_t header_size) {
  if (!m_io_common.start_io_setup()) {
    return;
  }
  chops::mutable_shared_buffer buf(header_size);
  auto self { std::shared_from_this() };
  std::experimental::net::async_read(m_socket, std::experimental::net::mutable_buffer(buf.data(), buf.size()),
    [this, self, buf, mh = std::forward(msg_hdlr), mf = std::forward(msg_frame), header_size]
          (const std::error_code& err, std::size_t nb) {
      handle_read(mh, mf, buf, header_size, err, nb);
    }
  );
}

template <typename MH, typename MF>
void tcp_io::handle_read(MH&& msg_hdlr, MF&& msg_frame, chops::mutable_shared_buffer buf, 
                 std::size_t header_size, const std::error_code& err, std::size_t num_bytes) {

  using chops::net;

  if (!m_io_common.check_err_code(err, std::shared_from_this())) {
    return;
  }
  // assert num_bytes == buf.size()
  std::experimental::net::mutable_buffer mbuf;
  std::size_t next_read_size = msg_frame(buf);
  if (next_read_size == 0) { // msg fully received, now invoke message handler
    if (!msg_hdlr(buf, io_interface<tcp_io>(std::weak_from_this()), m_io_common.get_remote_endp(), msg_frame)) {
      // message handler not happy, tear everything down
      m_io_common.check_err_code(std::make_error_code(net_ip_errc::message_handler_terminated), 
                                 std::shared_from_this());
      return;
    }
    // buf may be empty if moved from
    buf.resize(header_size);
    mbuf = std::experimental::net::mutable_buffer(buf.data(), buf.size());
  }
  else {
    std::size_t old_size = buf.size();
    buf.resize(buf.size() + next_read_size);
    mbuf = std::experimental::net::mutable_buffer(buf.data() + old_size, next_read_size);
  }
  // start another read
  auto self { std::shared_from_this() };
  std::experimental::net:async_read(m_socket, mbuf,
    [this, self, buf, mh = std::forward(msg_hdlr), mf = std::forward(msg_frame), header_size]
          (const std::error_code& err, std::size_t nb) {
      handle_read(mh, mf, buf, header_size, err, nb);
    }
  );
}

template <typename MH>
void tcp_io::start_io(MH&& msg_hdlr, std::string_view delim) {
  if (!m_io_common.start_io_setup()) {
    return;
  }
  std::string delimiter (delim); // make sure no dangling references
  lsjdflkj use vector of bytes as dynamic buffer
  auto self { std::shared_from_this() };
  std::experimental::net::async_read_until(m_socket, chops::mutable_shared_buffer(), delimiter,
    [this, self, buf, mh = std::forward(msg_hdlr), delimiter] (const std::error_code& err, std::size_t nb) {
      handle_read(mh, buf, delimiter, err, nb);
    }
  );
}

template <typename MH>
void tcp_io::handle_read(MH&& msg_hdlr, chops::mutable_shared_buffer buf, std::string delimiter,
                 const std::error_code& err, std::size_t num_bytes) {

  using chops::net;

  if (!m_io_common.check_err_code(err, std::shared_from_this())) {
    return;
  }
  if (!msg_hdlr(buf, io_interface<tcp_io>(std::weak_from_this()), m_io_common.get_remote_endp())) {
      m_io_common.check_err_code(std::make_error_code(net_ip_errc::message_handler_terminated), 
                                 std::shared_from_this());
    return;
  }
  auto self { std::shared_from_this() };
  std::experimental::net::async_read_until(m_socket, something, delimiter,
    [this, self, buf, mh = std::forward(msg_hdlr), delimiter] (const std::error_code& err, std::size_t nb) {
      handle_read(mh, buf, delimiter, err, nb);
    }
  );
}

template <typename MH>
void tcp_io::start_io(MH&& msg_handler, std::size_t read_size) {
  using chops::net;
  using std::experimental::net::ip::tcp::endpoint;

  auto wrapped_mh = [mh = std::forward(msg_handler)] 
               (chops::mutable_shared_buffer buf, io_interface<tcp_io> io, endpoint endp, null_msg_frame) {
    return mh(buf, io, endp);
  };
  start_io(wrapped_mh, null_msg_frame, read_size);
}

inline void tcp_io::start_io() {
  using chops::net;
  using std::experimental::net::ip::tcp::endpoint;

  auto msg_hdlr = [] (chops::mutable_shared_buffer, io_interface<tcp_io>, endpoint, null_msg_frame) {
    return true;
  };
  start_io(msg_hdlr, null_msg_frame, 1);
}

inline void tcp_io::start_write(chops::shared_const_buffer buf) {
  if (!m_io_common.start_write_setup(buf)) {
    return; // buf queued or shutdown happening
  }
  auto self { std::shared_from_this() };
  std::experimental::net::async_write(m_socket, buf, [this, self] 
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
  std::experimental::net::async_write(m_socket, elem->first, [this, self]
      (const std::system::error_code& err, std::size_t nb) { handle_write(err, nb); }
}

} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

