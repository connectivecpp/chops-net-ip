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

#include "net_ip/detail/tcp_io.hpp"
#include "net_ip/detail/net_entity_common.hpp"

#include "net_ip/io_interface.hpp"

#include "utility/erase_where.hpp"

namespace chops {
namespace net {
namespace detail {

class tcp_acceptor : public std::enable_shared_from_this<tcp_acceptor> {
public:
  using socket_type = asio::ip::tcp::acceptor;
  using endpoint_type = asio::ip::tcp::endpoint;

private:
  net_entity_common<tcp_io>         m_entity_common;
  asio::io_context&                 m_io_context;
  socket_type                       m_acceptor;
  std::vector<tcp_io_shared_ptr>    m_io_handlers;
  endpoint_type                     m_acceptor_endp;
  bool                              m_reuse_addr;

public:
  tcp_acceptor(asio::io_context& ioc, const endpoint_type& endp,
               bool reuse_addr) :
    m_entity_common(), m_io_context(ioc), m_acceptor(ioc), m_io_handlers(), m_acceptor_endp(endp), 
    m_reuse_addr(reuse_addr) { }

private:
  // no copy or assignment semantics for this class
  tcp_acceptor(const tcp_acceptor&) = delete;
  tcp_acceptor(tcp_acceptor&&) = delete;
  tcp_acceptor& operator=(const tcp_acceptor&) = delete;
  tcp_acceptor& operator=(tcp_acceptor&&) = delete;

public:

  bool is_started() const noexcept { return m_entity_common.is_started(); }

  socket_type& get_socket() noexcept { return m_acceptor; }

  template <typename F1, typename F2>
  bool start(F1&& io_state_chg, F2&& err_func) {
    if (!m_entity_common.start(std::forward<F1>(io_state_chg), std::forward<F2>(err_func))) {
      // already started
      return false;
    }
    try {
      m_acceptor = socket_type(m_io_context, m_acceptor_endp, m_reuse_addr);
    }
    catch (const std::system_error& se) {
      m_entity_common.call_error_cb(tcp_io_shared_ptr(), se.code());
      stop();
      return false;
    }
    start_accept();
    return true;
  }

  bool stop() {
    if (!m_entity_common.stop()) {
      return false; // stop already called
    }
    auto iohs = m_io_handlers;
    for (auto i : iohs) {
      i->stop_io();
    }
    // m_io_handlers.clear(); // the stop_io on each tcp_io handler should clear the container
    m_entity_common.call_error_cb(tcp_io_shared_ptr(), std::make_error_code(net_ip_errc::tcp_acceptor_stopped));
    std::error_code ec;
    m_acceptor.close(ec);
    return true;
  }

private:

  void start_accept() {
    using namespace std::placeholders;

    auto self = shared_from_this();
    m_acceptor.async_accept( [this, self] 
            (const std::error_code& err, asio::ip::tcp::socket sock) mutable {
        if (err) {
          m_entity_common.call_error_cb(tcp_io_shared_ptr(), err);
          stop(); // is this the right thing to do? what are possible causes of errors?
          return;
        }
        tcp_io_shared_ptr iop = std::make_shared<tcp_io>(std::move(sock), 
          tcp_io::entity_notifier_cb(std::bind(&tcp_acceptor::notify_me, shared_from_this(), _1, _2)));
        m_io_handlers.push_back(iop);
        m_entity_common.call_io_state_chg_cb(iop, m_io_handlers.size(), true);
        start_accept();
      }
    );
  }

  void notify_me(std::error_code err, tcp_io_shared_ptr iop) {
    iop->close();
    m_entity_common.call_error_cb(iop, err);
    chops::erase_where(m_io_handlers, iop);
    m_entity_common.call_io_state_chg_cb(iop, m_io_handlers.size(), false);
  }

};

using tcp_acceptor_shared_ptr = std::shared_ptr<tcp_acceptor>;
using tcp_acceptor_weak_ptr = std::weak_ptr<tcp_acceptor>;

} // end detail namespace
} // end net namespace
} // end chops namespace


#endif

