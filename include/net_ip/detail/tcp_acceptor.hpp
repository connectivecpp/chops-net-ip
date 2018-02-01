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
#include <experimental/executor>

#include <system_error>
#include <memory>
#include <vector>
#include <utility> // std::move, std::forward
#include <cstddef> // for std::size_t
#include <functional> // std::bind

#include "net_ip/detail/tcp_io.hpp"
#include "net_ip/detail/net_entity_base.hpp"

#include "net_ip/io_interface.hpp"

#include "utility/erase_where.hpp"

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
  std::vector<tcp_io_ptr>   m_io_handlers;
  endpoint_type             m_acceptor_endp;
  bool                      m_reuse_addr;

public:
  tcp_acceptor(std::experimental::net::io_context& ioc, const endpoint_type& endp,
               bool reuse_addr) :
    m_entity_base(), m_acceptor(ioc), m_io_handlers(), m_acceptor_endp(endp), 
    m_reuse_addr(reuse_addr) { }

private:
  // no copy or assignment semantics for this class
  tcp_acceptor(const tcp_acceptor&) = delete;
  tcp_acceptor(tcp_acceptor&&) = delete;
  tcp_acceptor& operator=(const tcp_acceptor&) = delete;
  tcp_acceptor& operator=(tcp_acceptor&&) = delete;

public:

  bool is_started() const noexcept { return m_entity_base.is_started(); }

  socket_type& get_socket() noexcept { return m_acceptor; }

  template <typename R, typename S>
  void start(R&& start_chg, S&& shutdown_chg) {
    if (!m_entity_base.start(std::forward<S>(shutdown_chg))) {
      // already started
      return;
    }
    try {
      m_acceptor = socket_type(m_acceptor.get_executor().context(), m_acceptor_endp,
                               m_reuse_addr);
    }
    catch (const std::system_error& se) {
      m_entity_base.call_shutdown_change_cb(tcp_io_ptr(), se.code(), 0);
      stop();
      return;
    }
    start_accept(std::forward<R>(start_chg));
  }

  void stop() {
    if (!m_entity_base.stop()) {
      return; // stop already called
    }
    auto iohs = m_io_handlers;
    for (auto i : iohs) {
      i->close();
    }
    m_io_handlers.clear();
    m_entity_base.call_shutdown_change_cb(tcp_io_ptr(),
                                          std::make_error_code(net_ip_errc::tcp_acceptor_stopped), 
                                          0);
    std::error_code ec;
    m_acceptor.close(ec);
  }

private:

  template <typename R>
  void start_accept(R&& start_chg) {
    using namespace std::placeholders;

    auto self = shared_from_this();
    m_acceptor.async_accept( [this, self, sf = std::move(start_chg)] 
                               (const std::error_code& err, std::experimental::net::ip::tcp::socket sock) {
        if (err) {
          m_entity_base.call_shutdown_change_cb(tcp_io_ptr(), err, m_io_handlers.size());
          stop(); // is this the right thing to do? what are possible causes of errors?
          return;
        }
        tcp_io_ptr iop = std::make_shared<tcp_io>(std::move(sock), 
          tcp_io::entity_notifier_cb(std::bind(&tcp_acceptor::notify_me, shared_from_this(), _1, _2)));
        m_io_handlers.push_back(iop);
        sf(tcp_io_interface(iop), m_io_handlers.size());
        start_accept(std::move(sf));
      }
    );
  }

  void notify_me(std::error_code err, tcp_io_ptr iop) {
    auto self { shared_from_this() };
    post(m_acceptor.get_executor(), [this, self, err, iop] {
        iop->close();
        chops::erase_where(m_io_handlers, iop);
        m_entity_base.call_shutdown_change_cb(iop, err, m_io_handlers.size());
      }
    );
  }

};

using tcp_acceptor_ptr = std::shared_ptr<tcp_acceptor>;

} // end detail namespace
} // end net namespace
} // end chops namespace


#endif

