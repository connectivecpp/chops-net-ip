/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief TCP acceptor, for internal use.
 *
 *  @note For internal use only.
 *
 *  @author Cliff Green
 *  @date 2018
 *
 */

#ifndef TCP_ACCEPTOR_HPP_INCLUDED
#define TCP_ACCEPTOR_HPP_INCLUDED

#include <experimental/internet>
#include <experimental/io_context>

#include <system_error>
#include <memory>
#include <utility> // std::move
#include <cstddef> // for std::size_t
#include <functional> // std::bind

#include "net_ip/detail/tcp_io.hpp"
#include "net_ip/detail/net_entity_base.hpp"

namespace chops {
namespace net {
namespace detail {

class tcp_acceptor : public std::enable_shared_from_this<tcp_acceptor> {
public:
  using socket_type = std::experimental::net::ip::tcp::acceptor;
  using endpoint_type = std::experimental::net::ip::tcp::endpoint;

private:
  net_entity_base<tcp_io>   m_entity_base;
  socket_type               m_acceptor;
  endpoint_type             m_acceptor_endp;
  bool                      m_reuse_addr;

public:
  tcp_acceptor(std::experimental::net::io_context& ioc, 
               const std::experimental::net::ip::tcp::endpoint& endp,
               bool reuse_addr) :
    m_entity_base(), m_acceptor(ioc), m_acceptor_endp(endp), m_reuse_addr(reuse_addr) { }

public:

  bool is_started() const noexcept { return m_entity_base.is_started(); }

  socket_type& get_socket() noexcept { return m_acceptor; }

  template <typename R, typename S>
  void start(R&& start_chg, S&& shutdown_chg) {
    if (!m_entity_base.start(std::forward<R>(start_chg), std::forward<S>(shutdown_chg))) {
      // already started
      return;
    }
    try {
      m_acceptor = 
        std::experimental::net::ip::tcp::acceptor(m_acceptor.get_executor().context(), 
          m_acceptor_endp, m_reuse_addr);
    }
    catch (const std::system_error& se) {
      m_entity_base.stop(); // should not have any tcp io handlers
      m_entity_base.call_shutdown_change_cb(se.code(), tcp_io_ptr());
      return;
    }
    start_accept();
  }

  void stop() {
    close();
    m_entity_base.call_shutdown_change_cb(std::make_error_code(net_ip_errc::tcp_acceptor_stopped), tcp_io_ptr());
  }

private:

  void close() {
    m_entity_base.stop_io_all();
    m_entity_base.stop(); // toggle started flag, clear container of tcp io handlers
    std::error_code ec;
    m_acceptor.close(ec);
  }

  void start_accept() {
    using namespace std::placeholders;

    auto self = shared_from_this();
    m_acceptor.async_accept( [this, self] (const std::error_code& err,
                                std::experimental::net::ip::tcp::socket sock) {
        if (err) {
          close(); // is this the right thing to do? what are possible causes of errors?
          m_entity_base.call_shutdown_change_cb(err, tcp_io_ptr());
          return;
        }
        tcp_io_ptr iop = std::make_shared<tcp_io>(std::move(sock), 
          tcp_io::entity_cb(std::bind(&tcp_acceptor::notify_me, shared_from_this(), _1, _2)));
        m_entity_base.add_handler(iop);
        m_entity_base.call_start_change_cb(iop);
        start_accept();
      }
    );
  }

  void notify_me(std::error_code err, tcp_io_ptr iop) {
    iop->close();
    m_entity_base.remove_handler(iop);
    m_entity_base.call_shutdown_change_cb(err, iop);
  }

};

using tcp_acceptor_ptr = std::shared_ptr<tcp_acceptor>;

} // end detail namespace
} // end net namespace
} // end chops namespace


#endif

