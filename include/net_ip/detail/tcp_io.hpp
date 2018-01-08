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
#include "net_ip/detail/io_common.hpp"
#include "net_ip/queue_stats.hpp"
#include "net_ip/net_ip_error.hpp"
#include "net_ip/io_interface.hpp"
#include "utility/shared_buffer.hpp"

namespace chops {
namespace net {
namespace detail {

class tcp_io : public std::enable_shared_from_this<tcp_io> {
public:
  using socket_type = std::experimental::net::ip::tcp::socket;
  using endpoint_type = std::experimental::net::ip::tcp::endpoint;
  using byte_vec = chops::mutable_shared_buffer::byte_vec;

private:
  using entity_cb = typename io_common<tcp_io>::entity_notifier_cb;

private:

  socket_type            m_socket;
  io_common<tcp_io>      m_io_common;
  // the following members could be passed through handlers, but are stored here
  // for simplicity and to reduce copying
  byte_vec               m_byte_vec;
  std::size_t            m_read_size;
  std::string            m_delimiter;

public:

  tcp_io(socket_type sock, entity_cb cb) noexcept : 
    m_socket(std::move(sock)), m_io_common(cb), 
    m_byte_vec(), m_read_size(0), m_delimiter() { }

private:
  // no copy or assignment semantics for this class
  tcp_io(const tcp_io&) = delete;
  tcp_io(tcp_io&&) = delete;
  tcp_io& operator=(const tcp_io&) = delete;
  tcp_io& operator=(tcp_io&&) = delete;

public:
  // all of the methods in this public section can be called through an io_interface
  socket_type& get_socket() noexcept { return m_socket; }

  output_queue_stats get_output_queue_stats() const noexcept {
    return m_io_common.get_output_queue_stats();
  }

  bool is_started() const noexcept { return m_io_common.is_started(); }

  template <typename MH, typename MF>
  void start_io(MH && msg_handler, MF&& msg_frame, std::size_t header_size) {
    if (!m_io_common.start_io_setup(m_socket)) {
      return;
    }
    m_read_size = header_size;
    m_byte_vec.resize(m_read_size);
    start_read(msg_handler, msg_frame,
               std::experimental::net::mutable_buffer(m_byte_vec.data(), m_byte_vec.size()));
  }

  template <typename MH>
  void start_io(MH&& msg_handler, std::string_view delimiter) {
    if (!m_io_common.start_io_setup(m_socket)) {
      return;
    }
    m_delimiter = delimiter;
    start_read_until(msg_handler);
  }

  template <typename MH>
  void start_io(MH&& msg_handler, std::size_t read_size) {
    start_io(msg_handler, null_msg_frame, read_size);
  }

  void start_io() {
    auto msg_hdlr = [] (std::experimental::net::const_buffer, 
                        io_interface<tcp_io>, std::experimental::net::ip::tcp::endpoint) {
      return true;
    };
    start_io(msg_hdlr, null_msg_frame, 1);
  }

  void stop_io() {
    m_io_common.check_err_code(std::make_error_code(net_ip_errc::io_handler_stopped), 
                               shared_from_this());
  }

  // use post for thread safety, multiple threads can call this method
  void send(chops::const_shared_buffer buf) {
    auto self { shared_from_this() };
    post(m_socket.get_executor(), [this, self, buf] { start_write(buf); } );
  }

public:
  // this method can only be called through a net entity, assumes all error codes have already
  // been reported back to the net entity
  void close();

private:

  template <typename MH, typename MF>
  void start_read(MH&& msg_hdlr, MF&& msg_frame, std::experimental::net::mutable_buffer mbuf) {
    auto self { shared_from_this() };
    std::experimental::net::async_read(m_socket, mbuf,
      [this, self, mh = std::forward<MH>(msg_hdlr), mf = std::forward<MF>(msg_frame)]
            (const std::error_code& err, std::size_t nb) {
        handle_read(mh, mf, err, nb);
      }
    );
  }

  template <typename MH, typename MF>
  void handle_read(MH&&, MF&&, const std::error_code&, std::size_t);

  template <typename MH>
  void start_read_until(MH&& msg_hdlr) {
    auto dyn_buf = std::experimental::net::dynamic_buffer(m_byte_vec);
    auto self { shared_from_this() };
    std::experimental::net::async_read_until(m_socket, dyn_buf, m_delimiter,
          [this, self, mh = std::forward<MH>(msg_hdlr), db = std::move(dyn_buf)] 
            (const std::error_code& err, std::size_t nb) {
        handle_read_until(mh, db, err, nb);
      }
    );
  }

  template <typename MH, typename DB>
  void handle_read_until(MH&&, DB&&, const std::error_code&, std::size_t);

  void start_write(chops::const_shared_buffer);

  void handle_write(const std::error_code&, std::size_t);

};

// method implementations, just to make the class declaration a little more readable

inline void tcp_io::close() {
  m_io_common.stop();
  // attempt graceful shutdown
  std::error_code ec;
  m_socket.shutdown(std::experimental::net::ip::tcp::socket::shutdown_both, ec);
  auto self { shared_from_this() };
  post(m_socket.get_executor(), [this, self] { m_socket.close(); } );
}

template <typename MH, typename MF>
void tcp_io::handle_read(MH&& msg_hdlr, MF&& msg_frame, 
                         const std::error_code& err, std::size_t num_bytes) {

  if (!m_io_common.check_err_code(err, shared_from_this())) {
    return;
  }
  // assert num_bytes == m_byte_vec.size()
  std::experimental::net::mutable_buffer mbuf(m_byte_vec.data(), m_byte_vec.size());
  std::size_t next_read_size = msg_frame(mbuf);
  if (next_read_size == 0) { // msg fully received, now invoke message handler
    if (!msg_hdlr(std::experimental::net::const_buffer(m_byte_vec.data(), m_byte_vec.size()), 
                  io_interface<tcp_io>(weak_from_this()), m_io_common.get_remote_endp())) {
      // message handler not happy, tear everything down
      m_io_common.check_err_code(std::make_error_code(net_ip_errc::message_handler_terminated), 
                                 shared_from_this());
      return;
    }
    m_byte_vec.resize(m_read_size);
    mbuf = std::experimental::net::mutable_buffer(m_byte_vec.data(), m_byte_vec.size());
  }
  else {
    std::size_t old_size = m_byte_vec.size();
    m_byte_vec.resize(old_size + next_read_size);
    mbuf = std::experimental::net::mutable_buffer(m_byte_vec.data() + old_size, next_read_size);
  }
  start_read(msg_hdlr, msg_frame, mbuf);
}

template <typename MH, typename DB>
void tcp_io::handle_read_until(MH&& msg_hdlr, DB&& dyn_buf, 
                               const std::error_code& err, std::size_t num_bytes) {

  if (!m_io_common.check_err_code(err, shared_from_this())) {
    return;
  }
  if (!msg_hdlr(dyn_buf.data(), // includes delimiter bytes
                io_interface<tcp_io>(weak_from_this()), m_io_common.get_remote_endp())) {
      m_io_common.check_err_code(std::make_error_code(net_ip_errc::message_handler_terminated), 
                                 shared_from_this());
    return;
  }
  dyn_buf.consume(num_bytes);
  start_read_until(msg_hdlr);
}


inline void tcp_io::start_write(chops::const_shared_buffer buf) {
  if (!m_io_common.start_write_setup(buf)) {
    return; // buf queued or shutdown happening
  }
  auto self { shared_from_this() };
  std::experimental::net::async_write(m_socket, 
          std::experimental::net::const_buffer(buf.data(), buf.size()),
            [this, self] (const std::error_code& err, std::size_t nb) {
      handle_write(err, nb);
    }
  );
}

inline void tcp_io::handle_write(const std::error_code& err, std::size_t /* num_bytes */) {
  if (!m_io_common.check_err_code(err, shared_from_this())) {
    return;
  }
  auto elem = m_io_common.get_next_element();
  if (!elem) {
    return;
  }
  auto self { shared_from_this() };
  std::experimental::net::async_write(m_socket, 
              std::experimental::net::const_buffer(elem->first.data(), elem->first.size()),
              [this, self] (const std::error_code& err, std::size_t nb) {
      handle_write(err, nb);
    }
  );
}

} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

