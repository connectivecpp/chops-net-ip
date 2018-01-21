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
#include <functional>

#include "net_ip/detail/output_queue.hpp"
#include "net_ip/detail/io_base.hpp"
#include "net_ip/queue_stats.hpp"
#include "net_ip/net_ip_error.hpp"
#include "net_ip/io_interface.hpp"
#include "utility/shared_buffer.hpp"

namespace chops {
namespace net {
namespace detail {

std::size_t null_msg_frame (std::experimental::net::mutable_buffer) noexcept;
bool null_msg_hdlr (std::experimental::net::const_buffer, io_interface<tcp_io>, 
                    std::experimental::net::ip::tcp::endpoint) noexcept;

class tcp_io : public std::enable_shared_from_this<tcp_io> {
public:
  using socket_type = std::experimental::net::ip::tcp::socket;
  using endpoint_type = std::experimental::net::ip::tcp::endpoint;
  using byte_vec = chops::mutable_shared_buffer::byte_vec;
  using entity_cb = typename io_base<tcp_io>::entity_notifier_cb;

private:

  socket_type            m_socket;
  io_base<tcp_io>        m_io_base;

  // the following members are only used for read processing; they could be 
  // passed through handlers, but are members for simplicity and to reduce 
  // copying or moving
  byte_vec               m_byte_vec;
  std::size_t            m_read_size;
  std::string            m_delimiter;

public:

  tcp_io(socket_type sock, entity_cb cb) noexcept : 
    m_socket(std::move(sock)), m_io_base(cb), 
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
    return m_io_base.get_output_queue_stats();
  }

  bool is_started() const noexcept { return m_io_base.is_started(); }

  template <typename MH, typename MF>
  void start_io(MH&& msg_handler, MF&& msg_frame, std::size_t header_size) {
    if (!start_io_setup()) {
      return;
    }
    m_read_size = header_size;
    m_byte_vec.resize(m_read_size);
    start_read(std::forward<MH>(msg_handler), std::forward<MF>(msg_frame),
               std::experimental::net::mutable_buffer(m_byte_vec.data(), m_byte_vec.size()));
  }

  template <typename MH>
  void start_io(MH&& msg_handler, std::string_view delimiter) {
    if (!start_io_setup()) {
      return;
    }
    m_delimiter = delimiter;
    start_read_until(std::forward<MH>(msg_handler));
  }

  template <typename MH>
  void start_io(MH&& msg_handler, std::size_t read_size) {
    start_io(std::forward<MH>(msg_handler), null_msg_frame, read_size);
  }

  void start_io() {
    start_io(null_msg_hdlr, null_msg_frame, 1);
  }

  void stop_io() {
    m_io_base.process_err_code(std::make_error_code(net_ip_errc::tcp_io_handler_stopped), 
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

  bool start_io_setup() {
    std::error_code ec;
    endpoint_type endp = m_socket.remote_endpoint(ec);
    if (ec) {
      m_io_base.process_err_code(ec, shared_from_this());
      return false;
    }
    return m_io_base.start_io_setup(endp);
  }

  template <typename MH, typename MF>
  void start_read(MH&& msg_hdlr, MF&& msg_frame, std::experimental::net::mutable_buffer mbuf) {
    // std::move in lambda instead of std::forward since an explicit copy or move of the function
    // object is desired so there are no dangling references
    auto self { shared_from_this() };
    std::experimental::net::async_read(m_socket, mbuf,
      [this, self, mh = std::move(msg_hdlr), mf = std::move(msg_frame), mbuf]
            (const std::error_code& err, std::size_t nb) mutable {
        handle_read(std::move(mh), std::move(mf), mbuf, err, nb);
      }
    );
  }

  template <typename MH, typename MF>
  void handle_read(MH&&, MF&&, std::experimental::net::mutable_buffer, 
                   const std::error_code&, std::size_t);

  template <typename MH>
  void start_read_until(MH&& msg_hdlr) {
    auto self { shared_from_this() };
    std::experimental::net::async_read_until(m_socket, 
                                             std::experimental::net::dynamic_buffer(m_byte_vec), 
                                             m_delimiter,
          [this, self, mh = std::move(msg_hdlr)] 
            (const std::error_code& err, std::size_t nb) mutable {
        handle_read_until(std::move(mh), err, nb);
      }
    );
  }

  template <typename MH>
  void handle_read_until(MH&&, const std::error_code&, std::size_t);

  void start_write(chops::const_shared_buffer);

  void handle_write(const std::error_code&, std::size_t);

};

// method implementations, just to make the class declaration a little more readable

inline void tcp_io::close() {
  m_io_base.stop();
  // attempt graceful shutdown
  std::error_code ec;
  m_socket.shutdown(std::experimental::net::ip::tcp::socket::shutdown_both, ec);
  auto self { shared_from_this() };
  post(m_socket.get_executor(), [this, self] { m_socket.close(); } );
}

template <typename MH, typename MF>
void tcp_io::handle_read(MH&& msg_hdlr, MF&& msg_frame, std::experimental::net::mutable_buffer mbuf, 
                         const std::error_code& err, std::size_t num_bytes) {

  if (err) {
    m_io_base.process_err_code(err, shared_from_this());
    return;
  }
  // assert num_bytes == mbuf.size()
  std::size_t next_read_size = msg_frame(mbuf);
  if (next_read_size == 0) { // msg fully received, now invoke message handler
    if (!msg_hdlr(std::experimental::net::const_buffer(m_byte_vec.data(), m_byte_vec.size()), 
                  io_interface<tcp_io>(weak_from_this()), m_io_base.get_remote_endp())) {
      // message handler not happy, tear everything down
      m_io_base.process_err_code(std::make_error_code(net_ip_errc::message_handler_terminated), 
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
  start_read(std::forward<MH>(msg_hdlr), std::forward<MF>(msg_frame), mbuf);
}

template <typename MH>
void tcp_io::handle_read_until(MH&& msg_hdlr, const std::error_code& err, std::size_t num_bytes) {

  if (err) {
    m_io_base.process_err_code(err, shared_from_this());
    return;
  }
  // beginning of m_byte_vec to num_bytes is buf, includes delimiter bytes
  if (!msg_hdlr(std::experimental::net::const_buffer(m_byte_vec.data(), num_bytes),
                io_interface<tcp_io>(weak_from_this()), m_io_base.get_remote_endp())) {
      m_io_base.process_err_code(std::make_error_code(net_ip_errc::message_handler_terminated), 
                                   shared_from_this());
    return;
  }
  m_byte_vec.erase(m_byte_vec.begin(), m_byte_vec.begin() + num_bytes);
  start_read_until(std::forward<MH>(msg_hdlr));
}


inline void tcp_io::start_write(chops::const_shared_buffer buf) {
  if (!m_io_base.start_write_setup(buf)) {
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
  if (err) {
    m_io_base.process_err_code(err, shared_from_this());
    return;
  }
  auto elem = m_io_base.get_next_element();
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

using tcp_io_ptr = std::shared_ptr<tcp_io>;

inline std::size_t null_msg_frame (std::experimental::net::mutable_buffer) noexcept {
  return 0;
}

inline bool null_msg_hdlr (std::experimental::net::const_buffer, io_interface<tcp_io>, 
                           std::experimental::net::ip::tcp::endpoint) noexcept {
  return true;
}

} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

