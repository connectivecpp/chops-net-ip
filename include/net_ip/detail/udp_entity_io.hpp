/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Internal class that is an IO handler for UDP datagram as well as
 *  an UDP entity class.
 *
 *  @note For internal use only.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef UDP_IO_HPP_INCLUDED
#define UDP_IO_HPP_INCLUDED

#include <experimental/executor>
#include <experimental/internet>
#include <experimental/buffer>

#include <memory> // std::shared_ptr, std::enable_shared_from_this
#include <system_error>

#include <cstddef> // std::size_t
#include <utility> // std::forward, std::move
#include <string>

#include "net_ip/detail/output_queue.hpp"
#include "net_ip/detail/io_common.hpp"
#include "net_ip/queue_stats.hpp"
#include "net_ip/net_ip_error.hpp"
#include "net_ip/io_interface.hpp"
#include "utility/shared_buffer.hpp"

namespace chops {
namespace net {
namespace detail {

class udp_io : public std::enable_shared_from_this<udp_io> {
public:
  using socket_type = std::experimental::net::ip::udp::socket;
  using endpoint_type = std::experimental::net::ip::tcp::endpoint;
  using byte_vec = chops::mutable_shared_buffer::byte_vec;

private:
  using entity_cb = typename io_common<udp_io>::entity_notifier_cb;

private:

  socket_type            m_socket;
  io_common<udp_io>      m_io_common;
  endpoint_type          m_incoming_endp;

  // the following members could be passed through handlers, but are stored here
  // for simplicity and to reduce copying
  byte_vec               m_byte_vec;
  std::size_t            m_read_size;

public:

  udp_io(socket_type sock, entity_cb cb) noexcept : 
    m_socket(std::move(sock)), m_io_common(cb), 
    m_byte_vec(), m_read_size(0), m_delimiter() { }

private:
  // no copy or assignment semantics for this class
  udp_io(const udp_io&) = delete;
  udp_io(udp_io&&) = delete;
  udp_io& operator=(const udp_io&) = delete;
  udp_io& operator=(udp_io&&) = delete;

public:
  // all of the methods in this public section can be called through an io_interface
  socket_type& get_socket() noexcept { return m_socket; }

  output_queue_stats get_output_queue_stats() const noexcept {
    return m_io_common.get_output_queue_stats();
  }

  bool is_started() const noexcept { return m_io_common.is_started(); }


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
  }

  void start_io() {
  }

  void stop_io() {
    m_io_common.check_err_code(std::make_error_code(net_ip_errc::io_handler_stopped), 
                               shared_from_this());
  }

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

inline void udp_io::close() {
  m_io_common.stop();
  // attempt graceful shutdown
  std::error_code ec;
  m_socket.shutdown(std::experimental::net::ip::tcp::socket::shutdown_both, ec);
  auto self { shared_from_this() };
  post(m_socket.get_executor(), [this, self] { m_socket.close(); } );
}

template <typename MH, typename MF>
void udp_io::handle_read(MH&& msg_hdlr, MF&& msg_frame, 
                         const std::error_code& err, std::size_t num_bytes) {

  if (!m_io_common.check_err_code(err, shared_from_this())) {
    return;
  }
  // assert num_bytes == m_byte_vec.size()
  std::experimental::net::mutable_buffer mbuf(m_byte_vec.data(), m_byte_vec.size());
  std::size_t next_read_size = msg_frame(mbuf);
  if (next_read_size == 0) { // msg fully received, now invoke message handler
    if (!msg_hdlr(std::experimental::net::const_buffer(m_byte_vec.data(), m_byte_vec.size()), 
                  io_interface<udp_io>(weak_from_this()), m_io_common.get_remote_endp())) {
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
void udp_io::handle_read_until(MH&& msg_hdlr, DB&& dyn_buf, 
                               const std::error_code& err, std::size_t num_bytes) {

  if (!m_io_common.check_err_code(err, shared_from_this())) {
    return;
  }
  if (!msg_hdlr(dyn_buf.data(), // includes delimiter bytes
                io_interface<udp_io>(weak_from_this()), m_io_common.get_remote_endp())) {
      m_io_common.check_err_code(std::make_error_code(net_ip_errc::message_handler_terminated), 
                                 shared_from_this());
    return;
  }
  dyn_buf.consume(num_bytes);
  start_read_until(msg_hdlr);
}


inline void udp_io::start_write(chops::const_shared_buffer buf) {
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

inline void udp_io::handle_write(const std::error_code& err, std::size_t /* num_bytes */) {
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

