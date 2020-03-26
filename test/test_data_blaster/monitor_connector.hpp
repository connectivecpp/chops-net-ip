/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Monitor message marshalling, unmarshalling, sending, and shutdown msg handling.
 *
 *  @author (fill in)
 *
 *  Copyright (c) 2019 by Cliff Green, (fill in)
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef MONITOR_CONNECTOR_HPP_INCLUDED
#define MONITOR_CONNECTOR_HPP_INCLUDED

#include <string>
#include <string_view>
#include <cstddef> // std::size_t
#include <future>
#include <utility> // std::move
#include <type_traits> // std::is_same

#include "asio/ip/basic_endpoint.hpp"

#include "net_ip/net_ip.hpp"
#include "net_ip/net_entity.hpp"
#include "net_ip_component/error_delivery.hpp"

#include "test_data_blaster/monitor_msg.hpp"

namespace chops {
namespace test {

// shutdown message from monitor process will set promise, which causes future to pop, which
// tells DSR to shutdown
class monitor_connector {
public:

  monitor_connector(chops::net::net_ip& net_ip,
                    std::string_view monitor_port, std::string_view monitor_host,
                    std::promise<void> prom,
                    chops::net::err_wait_q& err_wq) : 
      m_monitor(), m_io_output(), m_prom(std::move(prom))
  {
     // make_tcp_connector, start, provide state chg func obj that does start_io,
     // providing msg handler for shutdown, make_err_func_with_wait_queue for error display
  }
  void send_monitor_msg (const monitor_msg_data& msg_data) const {

  }
  // temporary - used for testing DSR
  void shutdown() {
  // send shutdown msg
    m_prom.set_value();
  }

private:
  chops::net::net_entity     m_monitor;
  chops::net::tcp_io_output  m_io_output;
  std::promise<void>         m_prom;
  // more member data to fill in

};

}
}