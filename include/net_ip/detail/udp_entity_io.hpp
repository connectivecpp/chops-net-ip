/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Internal class that combines a UDP entity and UDP io handler.
 *
 *  @note For internal use only.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2018-2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef UDP_ENTITY_IO_HPP_INCLUDED
#define UDP_ENTITY_IO_HPP_INCLUDED

#include "asio/io_context.hpp"
#include "asio/post.hpp"
#include "asio/ip/udp.hpp"
#include "asio/buffer.hpp"

#include <memory> // std::shared_ptr, std::enable_shared_from_this
#include <system_error>

#include <cstddef> // std::size_t
#include <utility> // std::forward, std::move
#include <functional> // std::function
#include <future>

#include "net_ip/detail/io_common.hpp"
#include "net_ip/detail/net_entity_common.hpp"

#include "net_ip/queue_stats.hpp"
#include "net_ip/net_ip_error.hpp"

#include "net_ip/basic_io_output.hpp"
#include "net_ip/endpoints_resolver.hpp"

#include "marshall/shared_buffer.hpp"

//////
#include <iostream>

namespace chops {
namespace net {
namespace detail {

struct udp_queue_element {
  const_shared_buffer     m_buf;
  asio::ip::udp::endpoint m_endp;

  udp_queue_element (const const_shared_buffer& buf,
                     const asio::ip::udp::endpoint& endp) noexcept : 
        m_buf(buf), m_endp(endp) { }

  std::size_t size() const noexcept {
    return m_buf.size();
  }

};

class udp_entity_io : public std::enable_shared_from_this<udp_entity_io> {
public:
  using endpoint_type = asio::ip::udp::endpoint;

private:
  using byte_vec = chops::mutable_shared_buffer::byte_vec;

private:

  io_common<udp_queue_element>      m_io_common;
  net_entity_common<udp_entity_io>  m_entity_common;
  asio::io_context&                 m_ioc;
  asio::ip::udp::socket             m_socket;
  endpoint_type                     m_local_endp;
  endpoint_type                     m_default_dest_endp;
  std::string                       m_local_port_or_service;
  std::string                       m_local_intf;

  // TODO: multicast stuff

  // following members could be passed through handler, but are members for 
  // simplicity and less copying
  byte_vec                          m_byte_vec;
  std::size_t                       m_max_size;
  endpoint_type                     m_sender_endp;
  bool                              m_shutting_down;

public:

  udp_entity_io(asio::io_context& ioc, 
                const endpoint_type& local_endp) noexcept : 
    m_io_common(), m_entity_common(), m_ioc(ioc),
    m_socket(ioc), m_local_endp(local_endp), m_default_dest_endp(), 
    m_local_port_or_service(), m_local_intf(),
    m_byte_vec(), m_max_size(0), m_sender_endp(), 
    m_shutting_down(false) { }

  udp_entity_io(asio::io_context& ioc, 
                std::string_view local_port_or_service, std::string_view local_intf) noexcept :
    m_io_common(), m_entity_common(), m_ioc(ioc),
    m_socket(ioc), m_local_endp(), m_default_dest_endp(), 
    m_local_port_or_service(local_port_or_service), m_local_intf(local_intf),
    m_byte_vec(), m_max_size(0), m_sender_endp(), 
    m_shutting_down(false) { }

private:
  // no copy or assignment semantics for this class
  udp_entity_io(const udp_entity_io&) = delete;
  udp_entity_io(udp_entity_io&&) = delete;
  udp_entity_io& operator=(const udp_entity_io&) = delete;
  udp_entity_io& operator=(udp_entity_io&&) = delete;

public:

  // all of the methods in this public section can be called through either an io_interface
  // or a net_entity

  bool is_started() const noexcept { return m_entity_common.is_started(); }

  bool is_io_started() const noexcept { return m_io_common.is_io_started(); }

  template <typename F>
  void visit_socket(F&& f) {
    f(m_socket);
  }

  template <typename F>
  std::size_t visit_io_output(F&& func) {
    auto self = shared_from_this();
    std::promise<std::size_t> prom;
    auto fut = prom.get_future();
    // send to executor for concurrency protection
    asio::post(m_socket.get_executor(), [this, self, &func, p = std::move(prom)] () mutable {
        if (m_io_common.is_io_started()) {
          func(basic_io_output<udp_entity_io>(self));
          p.set_value(1u);
        }
        else {
          p.set_value(0u);
        }
      }
    );
    return fut.get();
  }

  output_queue_stats get_output_queue_stats() const noexcept {
    return m_io_common.get_output_queue_stats();
  }

  template <typename F1, typename F2>
  std::error_code start(F1&& io_state_chg, F2&& err_cb) {
    auto self = shared_from_this();
    return m_entity_common.start(std::forward<F1>(io_state_chg), std::forward<F2>(err_cb),
                                 m_socket.get_executor(),
             [this, self] () mutable { return do_start(); } );
  }

  template <typename MH>
  bool start_io(std::size_t max_size, MH&& msg_handler) {
std::cerr << "Inside start_io uno, max_size: " << max_size << std::endl;
    if (!m_io_common.set_io_started()) { // concurrency protected
      return false;
    }
    if (m_local_endp == endpoint_type()) { // mismatch between start_io and initalized UDP entity
      return false;
    }
    m_max_size = max_size;
std::cerr << "Ready to start read" << std::endl;
    start_read(std::forward<MH>(msg_handler));
    return true;
  }

  template <typename MH>
  bool start_io(const endpoint_type& endp, std::size_t max_size, MH&& msg_handler) {
std::cerr << "Inside start_io dos, max_size: " << max_size << std::endl;
    if (!m_io_common.set_io_started()) { // concurrency protected
      return false;
    }
    if (m_local_endp == endpoint_type()) {
      return false;
    }
    m_max_size = max_size;
    m_default_dest_endp = endp;
std::cerr << "Ready to start read" << std::endl;
    start_read(std::forward<MH>(msg_handler));
    return true;
  }

  bool start_io() {
std::cerr << "Inside start_io no read uno" << std::endl;
    if (!m_io_common.set_io_started()) { // concurrency protected
      return false;
    }
    return true;
  }

  bool start_io(const endpoint_type& endp) {
std::cerr << "Inside start_io no read dos, endp: " << endp << std::endl;
    if (!m_io_common.set_io_started()) { // concurrency protected
      return false;
    }
    m_default_dest_endp = endp;
    return true;
  }

  bool stop_io() {
    // handle start_io never called - close the open socket, etc
    bool ret = !m_io_common.is_io_started();
    close(std::make_error_code(net_ip_errc::udp_io_handler_stopped));
    return ret;
  }

  std::error_code stop() {
    auto self = shared_from_this();
    return m_entity_common.stop(m_socket.get_executor(),
             [this, self] () mutable {
               close(std::make_error_code(net_ip_errc::udp_entity_stopped));
               return std::error_code();
             }
    );
  }

  // io_common has concurrency protection
  bool send(const chops::const_shared_buffer& buf) {
    return send(buf, m_default_dest_endp);
  }

  bool send(const chops::const_shared_buffer& buf, const endpoint_type& endp) {
    auto ret = m_io_common.start_write(udp_queue_element(buf, endp), 
        [this] (const udp_queue_element& e) {
          start_write(e);
        }
      );
    return ret != io_common<udp_queue_element>::write_status::io_stopped;
  }

private:

  template <typename MH>
  void start_read(MH&& msg_hdlr) {
    auto self { shared_from_this() };
    m_byte_vec.resize(m_max_size);
    m_socket.async_receive_from(
              asio::mutable_buffer(m_byte_vec.data(), m_byte_vec.size()),
              m_sender_endp,
                [this, self, mh = std::move(msg_hdlr)] 
                  (const std::error_code& err, std::size_t nb) mutable {
        handle_read(err, nb, mh);
      }
    );
  }

  template <typename MH>
  void handle_read(const std::error_code&, std::size_t, MH&&);

  void start_write(const udp_queue_element&);

  void handle_write(const std::error_code&, std::size_t);

private:

  std::error_code do_start() {
    if (!m_local_port_or_service.empty()) {
      endpoints_resolver<asio::ip::udp> resolver(m_ioc);
      auto ret = resolver.make_endpoints(true, m_local_intf, m_local_port_or_service);
      if (!ret) {
        close(ret.error());
        return ret.error();
      }
      m_local_endp = ret->cbegin()->endpoint();
      m_local_port_or_service.clear();
      m_local_port_or_service.shrink_to_fit();
      m_local_intf.clear();
      m_local_intf.shrink_to_fit();
    }
    std::error_code ec;
    m_socket.open(m_local_endp.protocol(), ec);
    if (ec) {
      close(ec);
      return ec;
    }
    if (m_local_endp != endpoint_type()) { // local bind needed
      m_socket.bind(m_local_endp, ec);
      if (ec) {
        close(ec);
        return ec;
      }
    }
    // don't invoke io_state_chg callback from within context of start method, instead
    // post and perform it later
    auto self { shared_from_this() };
    asio::post(m_socket.get_executor(),
      [this, self] () mutable {
        if (!m_entity_common.call_io_state_chg_cb(self, 1, true)) {
          close(std::make_error_code(net_ip_errc::io_state_change_terminated));
        }
      }
    );
    return { };
  }

  void close(const std::error_code& err) {
    auto self { shared_from_this() };
    m_entity_common.call_error_cb(self, err);
    if (m_shutting_down) { // already been through close once
      return;
    }
    m_shutting_down = true;
    m_io_common.set_io_stopped();
    m_entity_common.set_stopped();
    m_io_common.clear();
    auto b = m_entity_common.call_io_state_chg_cb(self, 0, false);
    std::error_code ec;
    m_socket.close(ec);
    m_entity_common.call_error_cb(self, std::make_error_code(net_ip_errc::udp_entity_closed));
  }

};

// method implementations, split out just to make the class declaration a little more readable

template <typename MH>
void udp_entity_io::handle_read(const std::error_code& err, std::size_t num_bytes, MH&& msg_hdlr) {

  if (err) {
    close(err);
    return;
  }
  if (!msg_hdlr(asio::const_buffer(m_byte_vec.data(), num_bytes), 
                basic_io_output<udp_entity_io>(shared_from_this()), m_sender_endp)) {
    // message handler not happy, tear everything down
    close(std::make_error_code(net_ip_errc::message_handler_terminated));
    return;
  }
  start_read(std::forward<MH>(msg_hdlr));
}

inline void udp_entity_io::start_write(const udp_queue_element& e) {
  auto self { shared_from_this() };
  m_socket.async_send_to(asio::const_buffer(e.m_buf.data(), e.m_buf.size()), e.m_endp,
            [this, self] (const std::error_code& err, std::size_t nb) {
      handle_write(err, nb);
    }
  );
}

inline void udp_entity_io::handle_write(const std::error_code& err, std::size_t /* num_bytes */) {
  if (err) {
    close(err);
    return;
  }
  m_io_common.write_next_elem([this] (const udp_queue_element& e) {
      start_write(e);
    }
  );
}

using udp_entity_io_shared_ptr = std::shared_ptr<udp_entity_io>;
using udp_entity_io_weak_ptr = std::weak_ptr<udp_entity_io>;

} // end detail namespace

} // end net namespace
} // end chops namespace

#endif

