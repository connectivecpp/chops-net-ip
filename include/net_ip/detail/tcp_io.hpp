/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Internal handler class for TCP stream input and output.
 *
 *  @note For internal use only.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2017-2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef TCP_IO_HPP_INCLUDED
#define TCP_IO_HPP_INCLUDED

#include "asio/io_context.hpp"
#include "asio/executor.hpp"
#include "asio/read.hpp"
#include "asio/read_until.hpp"
#include "asio/write.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/buffer.hpp"

#include <memory> // std::shared_ptr, std::enable_shared_from_this
#include <system_error>

#include <cstddef> // std::size_t
#include <utility> // std::forward, std::move
#include <string>
#include <string_view>
#include <functional>

#include "net_ip/detail/output_queue.hpp"
#include "net_ip/detail/io_common.hpp"
#include "net_ip/queue_stats.hpp"
#include "net_ip/net_ip_error.hpp"

#include "net_ip/basic_io_output.hpp"
#include "net_ip/simple_variable_len_msg_frame.hpp"

#include "marshall/shared_buffer.hpp"
#include "utility/cast_ptr_to.hpp"

namespace chops {
namespace net {
namespace detail {

inline std::size_t null_msg_frame (asio::mutable_buffer) noexcept { return 0u; }

class tcp_io : public std::enable_shared_from_this<tcp_io> {
public:
  using endpoint_type = asio::ip::tcp::endpoint;
  using entity_notifier_cb = std::function<void (std::error_code, std::shared_ptr<tcp_io>)>;

private:
  using byte_vec = chops::mutable_shared_buffer::byte_vec;

private:

  asio::ip::tcp::socket  m_socket;
  io_common<tcp_io>      m_io_common;
  entity_notifier_cb     m_notifier_cb;
  endpoint_type          m_remote_endp;

  // the following members are only used for read processing; they could be 
  // passed through handlers, but are members for simplicity and to reduce 
  // copying or moving
  byte_vec               m_byte_vec;
  std::size_t            m_read_size;
  std::string            m_delimiter;

public:

  tcp_io(asio::ip::tcp::socket sock, entity_notifier_cb cb) noexcept : 
    m_socket(std::move(sock)), m_io_common(), 
    m_notifier_cb(cb), m_remote_endp(),
    m_byte_vec(), m_read_size(0u), m_delimiter() { }

private:
  // no copy or assignment semantics for this class
  tcp_io(const tcp_io&) = delete;
  tcp_io(tcp_io&&) = delete;
  tcp_io& operator=(const tcp_io&) = delete;
  tcp_io& operator=(tcp_io&&) = delete;

public:
  // all of the methods in this public section can be called through a basic_io_interface
  // or basic_io_output

  template <typename F>
  void visit_socket(F&& f) {
    f(m_socket);
  }

  output_queue_stats get_output_queue_stats() const noexcept {
    return m_io_common.get_output_queue_stats();
  }

  bool is_io_started() const noexcept { return m_io_common.is_io_started(); }

  template <typename MH, typename MF>
  bool start_io(std::size_t header_size, MH&& msg_handler, MF&& msg_frame) {
    if (!start_io_setup()) {
      return false;
    }
    m_read_size = header_size;
    m_byte_vec.resize(m_read_size);
    start_read(asio::mutable_buffer(m_byte_vec.data(), m_byte_vec.size()),
               std::forward<MH>(msg_handler), std::forward<MF>(msg_frame));
    return true;
  }

  template <typename MH>
  bool start_io(std::size_t header_size, MH&& msg_handler, hdr_decoder_func func) {
    return start_io(header_size, std::forward<MH>(msg_handler), 
                               simple_variable_len_msg_frame(func));
  }

  template <typename MH>
  bool start_io(std::string_view delimiter, MH&& msg_handler) {
    if (!start_io_setup()) {
      return false;
    }
    m_delimiter = delimiter;
    start_read_until(std::forward<MH>(msg_handler));
    return true;
  }

  template <typename MH>
  bool start_io(std::size_t read_size, MH&& msg_handler) {
    return start_io(read_size, std::forward<MH>(msg_handler), null_msg_frame);
  }

  bool start_io() {
    return start_io(1, 
                    [] (asio::const_buffer, basic_io_output<tcp_io>, 
                        asio::ip::tcp::endpoint) mutable {
                          return true;
                    }, null_msg_frame );
  }


  bool stop_io() {
    bool ret = true;
    if (!m_io_common.is_io_started()) {
      // handle incongruous case where start_io never called - close the open socket, 
      // notify acceptor or connector so tcp_io object can be removed
      ret = false;
       m_io_common.set_io_started();
    }
    close(std::make_error_code(net_ip_errc::tcp_io_handler_stopped));
    return ret;
  }

  // use post for thread safety, multiple threads can call this method
  bool send(chops::const_shared_buffer buf) {
    auto self { shared_from_this() };
    asio::post(m_socket.get_executor(), [this, self, buf] {
        if (!m_io_common.start_write_setup(buf)) {
          return false; // buf queued or shutdown happening
        }
        start_write(buf);
        return true;
      }
    );
  }

  bool send(const chops::const_shared_buffer& buf, const endpoint_type&) {
    return send(buf);
  }

private:
  void close(const std::error_code& err) {
    if (!m_io_common.stop()) {
      return; // already stopped, short circuit any late handler callbacks
    }
    std::error_code ec;
    m_socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    m_socket.close(ec); 
    auto self { shared_from_this() };
    // notify the net entity via posting function object
    asio::post(m_socket.get_executor(), [this, self, err] () mutable { 
      m_notifier_cb(err, self); } );
  }

private:

  bool start_io_setup() {
    if (!m_io_common.set_io_started()) { // concurrency protected
      return false;
    }
    std::error_code ec;
    m_remote_endp = m_socket.remote_endpoint(ec);
    if (ec) {
      close(ec);
      return false;
    }
    return true;
  }

  template <typename MH, typename MF>
  void start_read(asio::mutable_buffer mbuf, MH&& msg_hdlr, MF&& msg_frame) {
    // std::move in lambda instead of std::forward since an explicit copy or move of the function
    // object is desired so there are no dangling references
    auto self { shared_from_this() };
    asio::async_read(m_socket, mbuf,
      [this, self, mbuf, mh = std::move(msg_hdlr), mf = std::move(msg_frame)]
            (const std::error_code& err, std::size_t nb) mutable {
        handle_read(mbuf, err, nb, std::move(mh), std::move(mf));
      }
    );
  }

  template <typename MH, typename MF>
  void handle_read(asio::mutable_buffer, 
                   const std::error_code&, std::size_t, MH&&, MF&&);

  template <typename MH>
  void start_read_until(MH&& msg_hdlr) {
    auto self { shared_from_this() };
    asio::async_read_until(m_socket, asio::dynamic_buffer(m_byte_vec), m_delimiter,
      [this, self, mh = std::move(msg_hdlr)] (const std::error_code& err, std::size_t nb) mutable {
        handle_read_until(err, nb, std::move(mh));
      }
    );
  }

  template <typename MH>
  void handle_read_until(const std::error_code&, std::size_t, MH&&);

  void start_write(chops::const_shared_buffer);

  void handle_write(const std::error_code&, std::size_t);

};

// method implementations, just to make the class declaration a little more readable

template <typename MH, typename MF>
void tcp_io::handle_read(asio::mutable_buffer mbuf, 
                         const std::error_code& err, std::size_t /* num_bytes */,
                         MH&& msg_hdlr, MF&& msg_frame) {

  if (err) {
    close(err);
    return;
  }
  // assert num_bytes == mbuf.size()
  std::size_t next_read_size = msg_frame(mbuf);
  if (next_read_size == 0u) { // msg fully received, now invoke message handler
    if (!msg_hdlr(asio::const_buffer(m_byte_vec.data(), m_byte_vec.size()), 
                  basic_io_output(this), m_remote_endp)) {
      // message handler not happy, tear everything down, post function object
      // instead of directly calling close to give a return message a possibility
      // of getting through
      auto err = std::make_error_code(net_ip_errc::message_handler_terminated);
      auto self { shared_from_this() };
      asio::post(m_socket.get_executor(), [this, self, err] () mutable { 
        close(err); } );
      return;
    }
    m_byte_vec.resize(m_read_size);
    mbuf = asio::mutable_buffer(m_byte_vec.data(), m_byte_vec.size());
  }
  else {
    std::size_t old_size = m_byte_vec.size();
    m_byte_vec.resize(old_size + next_read_size);
    mbuf = asio::mutable_buffer(m_byte_vec.data() + old_size, next_read_size);
  }
  start_read(mbuf, std::forward<MH>(msg_hdlr), std::forward<MF>(msg_frame));
}

template <typename MH>
void tcp_io::handle_read_until(const std::error_code& err, std::size_t num_bytes, MH&& msg_hdlr) {

  if (err) {
    close(err);
    return;
  }
  // beginning of m_byte_vec to num_bytes is buf, includes delimiter bytes
  if (!msg_hdlr(asio::const_buffer(m_byte_vec.data(), num_bytes),
                basic_io_output(this), m_remote_endp)) {
      auto err = std::make_error_code(net_ip_errc::message_handler_terminated);
      auto self { shared_from_this() };
      asio::post(m_socket.get_executor(), [this, self, err] () mutable { 
        close(err); } );
    return;
  }
  m_byte_vec.erase(m_byte_vec.begin(), m_byte_vec.begin() + num_bytes);
  start_read_until(std::forward<MH>(msg_hdlr));
}


inline void tcp_io::start_write(chops::const_shared_buffer buf) {
  auto self { shared_from_this() };
  asio::async_write(m_socket, asio::const_buffer(buf.data(), buf.size()),
            [this, self] (const std::error_code& err, std::size_t nb) {
      handle_write(err, nb);
    }
  );
}

inline void tcp_io::handle_write(const std::error_code& err, std::size_t /* num_bytes */) {
  if (err) {
    // read pops first, so usually no error is needed in write handlers
    // m_notifier_cb(err, shared_from_this());
    return;
  }
  auto elem = m_io_common.get_next_element();
  if (!elem) {
    return;
  }
  start_write(elem->first);
}

using tcp_io_shared_ptr = std::shared_ptr<tcp_io>;
using tcp_io_weak_ptr = std::weak_ptr<tcp_io>;

} // end detail namespace

} // end net namespace
} // end chops namespace

#endif

