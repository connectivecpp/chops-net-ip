/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief TCP acceptor, for internal use.
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

#ifndef TCP_ACCEPTOR_HPP_INCLUDED
#define TCP_ACCEPTOR_HPP_INCLUDED

#include "asio/ip/tcp.hpp"
#include "asio/io_context.hpp"

#include <system_error>
#include <memory>// std::shared_ptr, std::weak_ptr
#include <vector>
#include <utility> // std::move, std::forward
#include <cstddef> // for std::size_t
#include <functional> // std::bind
#include <string>
#include <string_view>

#include "nonstd/expected.hpp"

#include "net_ip/endpoints_resolver.hpp"
#include "net_ip/detail/tcp_io.hpp"
#include "net_ip/detail/net_entity_common.hpp"

#include "net_ip/basic_io_output.hpp"

#include "utility/erase_where.hpp"

namespace chops {
namespace net {
namespace detail {

class tcp_acceptor : public std::enable_shared_from_this<tcp_acceptor> {
public:
  using endpoint_type = asio::ip::tcp::endpoint;

private:
  net_entity_common<tcp_io>         m_entity_common;
  asio::io_context&                 m_ioc;
  asio::ip::tcp::acceptor           m_acceptor;
  std::vector<tcp_io_shared_ptr>    m_io_handlers;
  endpoint_type                     m_acceptor_endp;
  std::string                       m_local_port_or_service;
  std::string                       m_listen_intf;
  bool                              m_reuse_addr;

public:
  tcp_acceptor(asio::io_context& ioc, const endpoint_type& endp,
               bool reuse_addr) :
    m_entity_common(), m_ioc(ioc), m_acceptor(ioc), m_io_handlers(), m_acceptor_endp(endp), 
    m_local_port_or_service(), m_listen_intf(),
    m_reuse_addr(reuse_addr) { }

  tcp_acceptor(asio::io_context& ioc, 
               std::string_view local_port_or_service, std::string_view listen_intf,
               bool reuse_addr) :
    m_entity_common(), m_ioc(ioc), m_acceptor(ioc), m_io_handlers(), m_acceptor_endp(), 
    m_local_port_or_service(local_port_or_service), m_listen_intf(listen_intf),
    m_reuse_addr(reuse_addr) { }

private:
  // no copy or assignment semantics for this class
  tcp_acceptor(const tcp_acceptor&) = delete;
  tcp_acceptor(tcp_acceptor&&) = delete;
  tcp_acceptor& operator=(const tcp_acceptor&) = delete;
  tcp_acceptor& operator=(tcp_acceptor&&) = delete;

public:

  bool is_started() const noexcept { return m_entity_common.is_started(); }

  template <typename F>
  void visit_socket(F&& f) {
    f(m_acceptor);
  }

  template <typename F>
  std::size_t visit_io_output(F&& f) {
    for (auto ioh : m_io_handlers) {
      std::size_t sum = 0u;
      if (ioh->is_io_started()) {
        f(basic_io_output(ioh));
        sum += 1u;
      }
    }
  }


  template <typename F1, typename F2>
  auto start(F1&& io_state_chg, F2&& err_func) ->
        nonstd::expected<void, std::error_code> {
    if (!m_entity_common.start(std::forward<F1>(io_state_chg), std::forward<F2>(err_func))) {
      // already started
      return nonstd::make_unexpected(std::make_error_code(net_ip_errc::tcp_acceptor_already_started));
    }
    if (!m_local_port_or_service.empty()) {
      endpoints_resolver<asio::ip::tcp> resolver(m_ioc);
      auto ret = resolver.make_endpoints(true, m_listen_intf, m_local_port_or_service);
      if (!ret) {
        close(ret.error());
        return nonstd::make_unexpected(ret.error());
      }
      m_acceptor_endp = ret->cbegin()->endpoint();
      m_local_port_or_service.clear();
      m_local_port_or_service.shrink_to_fit();
      m_listen_intf.clear();
      m_listen_intf.shrink_to_fit();
    }
    try {
      m_acceptor = asio::ip::tcp::acceptor(m_ioc, m_acceptor_endp, m_reuse_addr);
    }
    catch (const std::system_error& se) {
      close(se.code());
      return nonstd::make_unexpected(se.code());
    }
    start_accept();
    return { };
  }

  auto stop() ->
        nonstd::expected<void, std::error_code> {
    if (!m_entity_common.is_started()) {
      // already stopped
      return nonstd::make_unexpected(std::make_error_code(net_ip_errc::tcp_acceptor_already_stopped));
    }
    close(std::make_error_code(net_ip_errc::tcp_acceptor_stopped));
    return { };
  }

private:

  void close(const std::error_code& err) {
    if (!m_entity_common.stop()) {
      return; // already closed
    }
    m_entity_common.call_error_cb(tcp_io_shared_ptr(), err);
    // the following copy is important, since the notify_me modifies the 
    // m_io_handlers container
    auto iohs = m_io_handlers;
    for (auto i : iohs) {
      i->stop_io();
    }
    // m_io_handlers.clear(); // the stop_io on each tcp_io handler should clear the container
    std::error_code ec;
    m_acceptor.close(ec);
    if (ec) {
      m_entity_common.call_error_cb(tcp_io_shared_ptr(), ec);
    }
    m_entity_common.call_error_cb(tcp_io_shared_ptr(), 
          std::make_error_code(net_ip_errc::tcp_acceptor_closed));
  }


private:

  void start_accept() {
    using namespace std::placeholders;

    auto self = shared_from_this();
    m_acceptor.async_accept( [this, self] 
            (const std::error_code& err, asio::ip::tcp::socket sock) mutable {
        if (err) {
          close(err);
          return;
        }
        tcp_io_shared_ptr iop = std::make_shared<tcp_io>(std::move(sock), 
          tcp_io::entity_notifier_cb(std::bind(&tcp_acceptor::notify_me, shared_from_this(), _1, _2)));
        m_io_handlers.push_back(iop);
        if (m_entity_common.call_io_state_chg_cb(iop, m_io_handlers.size(), true)) {
          start_accept();
        }
        else {
          asio::post(m_acceptor.get_executor(), [this, self] () mutable {
              close(std::make_error_code(net_ip_errc::io_state_change_terminated));
            } );
        }
      }
    );
  }

  // this method invoked via a posted function object, allowing the TCP IO handler
  // to completely shut down 
  void notify_me(std::error_code err, tcp_io_shared_ptr iop) {
    chops::erase_where(m_io_handlers, iop);
    m_entity_common.call_error_cb(iop, err);
    if (m_entity_common.call_io_state_chg_cb(iop, m_io_handlers.size(), false)) {
      return;
    }
    // the following logic branches can be tricky; there are different paths to calling
    // close - stop method (from app), error, or false return from state change
    // callback; in all cases, the first time through the close method the 
    // atomic started flag is set false, and this flag is checked in subsequent calls 
    // to close, protecting recursive shutdown of various resources
    auto self { shared_from_this() };
    asio::post(m_acceptor.get_executor(), [this, self] () mutable {
        close(std::make_error_code(net_ip_errc::io_state_change_terminated));
      } );
  }

};

using tcp_acceptor_shared_ptr = std::shared_ptr<tcp_acceptor>;
using tcp_acceptor_weak_ptr = std::weak_ptr<tcp_acceptor>;

} // end detail namespace
} // end net namespace
} // end chops namespace


#endif

