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
 *  Copyright (c) 2018-2025 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef TCP_ACCEPTOR_HPP_INCLUDED
#define TCP_ACCEPTOR_HPP_INCLUDED

#include "asio/ip/tcp.hpp"
#include "asio/io_context.hpp"
#include "asio/post.hpp"

#include <system_error>
#include <memory>// std::shared_ptr, std::weak_ptr
#include <vector>
#include <utility> // std::move, std::forward
#include <cstddef> // for std::size_t
#include <functional> // std::bind
#include <string>
#include <string_view>
#include <future>
#include <chrono>

#include "net_ip/endpoints_resolver.hpp"
#include "net_ip/detail/tcp_io.hpp"
#include "net_ip/detail/net_entity_common.hpp"

#include "net_ip/basic_io_output.hpp"

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
  bool                              m_shutting_down;

public:
  tcp_acceptor(asio::io_context& ioc, const endpoint_type& endp,
               bool reuse_addr) :
    m_entity_common(), m_ioc(ioc), m_acceptor(ioc), m_io_handlers(), m_acceptor_endp(endp), 
    m_local_port_or_service(), m_listen_intf(),
    m_reuse_addr(reuse_addr), m_shutting_down(false) { }

  tcp_acceptor(asio::io_context& ioc, 
               std::string_view local_port_or_service, std::string_view listen_intf,
               bool reuse_addr) :
    m_entity_common(), m_ioc(ioc), m_acceptor(ioc), m_io_handlers(), m_acceptor_endp(), 
    m_local_port_or_service(local_port_or_service), m_listen_intf(listen_intf),
    m_reuse_addr(reuse_addr), m_shutting_down(false) { }

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
  std::size_t visit_io_output(F&& func) {
    auto self = shared_from_this();
    std::promise<std::size_t> prom;
    auto fut = prom.get_future();
    // send to executor for concurrency protection
    asio::post(m_ioc, [this, self, &func, p = std::move(prom)] () mutable {
        std::size_t sum = 0u;
        if (m_shutting_down) {
          p.set_value(sum);
          return;
        }
        for (auto& ioh : m_io_handlers) {
          if (ioh->is_io_started()) {
            func(basic_io_output<tcp_io>(ioh));
            sum += 1u;
          }
        }
        p.set_value(sum);
      }
    );
    return fut.get();
  }

  template <typename F1, typename F2>
  std::error_code start(F1&& io_state_chg, F2&& err_func) {
    auto self = shared_from_this();
    return m_entity_common.start(std::forward<F1>(io_state_chg), std::forward<F2>(err_func),
                                 m_acceptor.get_executor(),
             [this, self] () { return do_start(); } );
  }
                                
  std::error_code stop() {
    auto self = shared_from_this();
    return m_entity_common.stop(m_acceptor.get_executor(),
             [this, self] () {
               close(std::make_error_code(net_ip_errc::tcp_acceptor_stopped));
               return std::error_code();
             }
    );
  }

private:

  std::error_code do_start() {
    if (!m_local_port_or_service.empty()) {
      endpoints_resolver<asio::ip::tcp> resolver(m_ioc);
      auto ret = resolver.make_endpoints(true, m_listen_intf, m_local_port_or_service);
      if (!ret) {
        close(ret.error());
        return ret.error();
      }
      m_acceptor_endp = ret->cbegin()->endpoint();
      m_local_port_or_service.clear();
      m_local_port_or_service.shrink_to_fit();
      m_listen_intf.clear();
      m_listen_intf.shrink_to_fit();
    }
    std::error_code ec;
    m_acceptor.open(m_acceptor_endp.protocol(), ec);
    if (ec) {
      close(ec);
      return ec;
    }
    if (m_reuse_addr) {
      m_acceptor.set_option(asio::socket_base::reuse_address(true), ec);
      if (ec) {
        close(ec);
        return ec;
      }
    }
    m_acceptor.bind(m_acceptor_endp, ec);
    if (ec) {
      close(ec);
      return ec;
    }
    m_acceptor.listen(asio::socket_base::max_listen_connections, ec);
    if (ec) {
      close(ec);
      return ec;
    }

    start_accept();
    return { };
  }

  void close(const std::error_code& err) {
    m_entity_common.call_error_cb(tcp_io_shared_ptr(), err);
    if (m_shutting_down) {
      return; // already shutting down, bypass closing again
    }
    m_shutting_down = true;
    m_entity_common.set_stopped(); // in case of internal call to close
    // the following copy is important, since the notify_me modifies the 
    // m_io_handlers container
    auto iohs = m_io_handlers;
    for (auto& i : iohs) {
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

    if (m_shutting_down) {
      return;
    }
    auto self = shared_from_this();
    m_acceptor.async_accept( [this, self] 
            (const std::error_code& err, asio::ip::tcp::socket sock) {
        if (err || m_shutting_down ) {
          return;
        }
        tcp_io_shared_ptr iop = std::make_shared<tcp_io>(std::move(sock), 
          tcp_io::entity_notifier_cb(std::bind(&tcp_acceptor::notify_me, shared_from_this(), _1, _2)));
        m_io_handlers.push_back(iop);
        // make sure app doesn't do any strangeness during callback
        // even if another accept completes, post order should invoke callback before next
        // accept handler is invoked
        asio::post(m_ioc, [this, self, iop] () {
            m_entity_common.call_io_state_chg_cb(iop, m_io_handlers.size(), true);
          }
        );
        start_accept();
      }
    );
  }

  // this code invoked via a posted function object, allowing the TCP IO handler
  // to completely shut down 
  void notify_me(std::error_code err, tcp_io_shared_ptr iop) {
    std::erase_if (m_io_handlers, [iop] (auto sp) { return iop == sp; } );
    m_entity_common.call_error_cb(iop, err);
    m_entity_common.call_io_state_chg_cb(iop, m_io_handlers.size(), false);
  }

};

using tcp_acceptor_shared_ptr = std::shared_ptr<tcp_acceptor>;
using tcp_acceptor_weak_ptr = std::weak_ptr<tcp_acceptor>;

} // end detail namespace
} // end net namespace
} // end chops namespace


#endif

