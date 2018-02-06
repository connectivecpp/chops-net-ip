/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief TCP connector class, for internal use.
 *
 *  @note For internal use only.
 *
 *  @author Cliff Green
 *  @date 2018
 *
 */

#ifndef TCP_CONNECTOR_HPP_INCLUDED
#define TCP_CONNECTOR_HPP_INCLUDED

#include <experimental/internet>
#include <experimental/socket>
#include <experimental/io_context>
#include <experimental/executor>
#include <experimental/timer>

#include <system_error>
#include <vector>
#include <memory>
#include <chrono>

#include <cstddef> // for std::size_t

#include "net_ip/detail/tcp_io.hpp"
#include "net_ip/detail/net_entity_base.hpp"

#include "net_ip/endpoints_resolver.hpp"

namespace chops {
namespace net {
namespace detail {

class tcp_connector : public std::enable_shared_from_this<tcp_connector> {
public:
  using socket_type = std::experimental::net::ip::tcp::socket;
  using endpoint_type = std::experimental::net::ip::tcp::endpoint;
  using io_interface_type = chops::net::tcp_io_interface;

private:
  using resolver_type = chops::net::endpoints_resolver<std::experimental::net::ip::tcp>;
  using resolver_results = 
    std::experimental::net::ip::basic_resolver_results<std::experimental::net::ip::tcp>;
  using endpoints = std::vector<endpoint_type>;
  using endpoints_iter = endpoints::const_iterator;

private:
  net_entity_base<tcp_io>               m_entity_base;
  socket_type                           m_socket;
  tcp_io_ptr                            m_io_handler;
  resolver_type                         m_resolver;
  endpoints                             m_endpoints;
  std::experimental::net::steady_timer  m_timer;
  std::chrono::milliseconds             m_reconn_time;
  std::string                           m_remote_host;
  std::string                           m_remote_port;

public:
  template <typename Iter>
  tcp_connector(std::experimental::net::io_context& ioc, 
                Iter beg, Iter end, std::chrono::milliseconds reconn_time) :
      m_entity_base(),
      m_socket(ioc),
      m_io_handler(),
      m_resolver(ioc),
      m_endpoints(beg, end),
      m_timer(ioc),
      m_reconn_time(reconn_time),
      m_remote_host(),
      m_remote_port()
    { }

  tcp_connector(std::experimental::net::io_context& ioc,
                std::string_view remote_port, std::string_view remote_host, 
                std::chrono::milliseconds reconn_time) :
      m_entity_base(),
      m_socket(ioc),
      m_io_handler(),
      m_resolver(ioc),
      m_endpoints(),
      m_timer(ioc),
      m_reconn_time(reconn_time),
      m_remote_host(remote_host),
      m_remote_port(remote_port)
    { }

private:
  // no copy or assignment semantics for this class
  tcp_connector(const tcp_connector&) = delete;
  tcp_connector(tcp_connector&&) = delete;
  tcp_connector& operator=(const tcp_connector&) = delete;
  tcp_connector& operator=(tcp_connector&&) = delete;

public:

  bool is_started() const noexcept { return m_entity_base.is_started(); }

  socket_type& get_socket() noexcept { return m_socket; }

  template <typename R, typename S>
  void start(R&& start_chg, S&& shutdown_chg) {
    if (!m_entity_base.start(std::forward<S>(shutdown_chg))) {
      // already started
      return;
    }
    // empty endpoints container is the flag that a resolve is needed
    if (m_endpoints.empty()) {
      auto self = shared_from_this();
      m_resolver.make_endpoints(false, m_remote_host, m_remote_port,
        [this, self, sf = std::move(start_chg)] 
             (std::error_code err, resolver_results res) mutable {
          handle_resolve(err, res, std::move(sf));
        }
      );
      return;
    }
    start_connect(std::move(start_chg));
  }

  void stop() {
    close();
    m_entity_base.call_shutdown_change_cb(tcp_io_ptr(), std::make_error_code(net_ip_errc::tcp_connector_stopped), 0);
  }


private:

  void close() {
    if (!m_entity_base.stop()) {
      return; // stop already called
    }
    if (m_io_handler) {
      m_io_handler->close();
    }
    m_timer.cancel();
    m_resolver.cancel();
    // socket should already be closed or moved from
  }

  template <typename R>
  void handle_resolve(std::error_code err, resolver_results res, R&& start_chg) {
    if (err) {
      m_entity_base.call_shutdown_change_cb(tcp_io_ptr(), err, 0);
      stop();
      return;
    }
    if (!is_started()) {
      return;
    }
    for (const auto& e : res) {
      m_endpoints.push_back(e.endpoint());
    }
    start_connect(std::move(start_chg));
  }

  template <typename R>
  void start_connect(R&& start_chg) {
    auto self = shared_from_this();
    std::experimental::net::async_connect(m_socket, m_endpoints.cbegin(), m_endpoints.cend(),
          [this, self, sf = std::move(start_chg)] 
                (const std::error_code& err, endpoints_iter iter) mutable {
        handle_connect(err, iter, std::move(sf));
      }
    );
  }

  template <typename R>
  void handle_connect (const std::error_code& err, endpoints_iter /* iter */, R&& start_chg) {
    using namespace std::placeholders;

    if (err) {
      m_entity_base.call_shutdown_change_cb(tcp_io_ptr(), err, 0);
      try {
        auto self = shared_from_this();
        m_timer.expires_after(m_reconn_time);
        m_timer.async_wait( [this, self, sf = std::move(start_chg)] 
                            (const std::error_code& err) mutable {
            if (!err) {
              start_connect(std::move(sf));
            }
          }
        );
      }
      catch (const std::system_error& se) {
        m_entity_base.call_shutdown_change_cb(tcp_io_ptr(), se.code(), 0);
        stop();
      }
      return;
    }
    m_io_handler = std::make_shared<tcp_io>(std::move(m_socket), 
      tcp_io::entity_notifier_cb(std::bind(&tcp_connector::notify_me, shared_from_this(), _1, _2)));
    auto self { shared_from_this() };
    post(m_socket.get_executor(), [this, self, strt = std::move(start_chg)] () mutable {
        strt(tcp_io_interface(m_io_handler), 1);
      }
    );
  }

  void notify_me(std::error_code err, tcp_io_ptr iop) {
    auto self { shared_from_this() };
    post(m_socket.get_executor(), [this, self, err, iop] () mutable {
        iop->close();
        m_entity_base.call_shutdown_change_cb(iop, err, 0);
        stop();
      }
    );
  }

};

using tcp_connector_ptr = std::shared_ptr<tcp_connector>;

} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

