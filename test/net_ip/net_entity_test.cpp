/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c net_entity class.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2018-2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "catch2/catch.hpp"

#include <memory> // std::shared_ptr
#include <functional> // std::ref
#include <system_error> // std::error_code
#include <set>
#include <chrono>
#include <thread>
#include <future>
#include <cstddef> // std::size_t
#include <iostream> // std::cerr

#include "net_ip/basic_io_interface.hpp"
#include "net_ip/basic_io_output.hpp"
#include "net_ip/net_entity.hpp"

#include "net_ip/detail/tcp_acceptor.hpp"
#include "net_ip/detail/tcp_connector.hpp"
#include "net_ip/detail/udp_entity_io.hpp"

#include "net_ip/io_type_decls.hpp"

#include "net_ip_component/worker.hpp"
#include "net_ip_component/error_delivery.hpp"

// using namespace chops::test;

const char* test_port_udp = "30555";
const char* test_port_tcp1 = "30556";
const char* test_port_tcp2 = "30557";
const char* test_host = "";
constexpr int NumMsgs = 30;
constexpr int ReconnTime = 400;

template <typename IOT>
struct io_state_chg {
  bool called = false;
  bool operator() (chops::net::basic_io_interface<IOT>, std::size_t, bool) {
    called = true;
    return true;
  }
};

template <typename S>
struct socket_visitor {
  // S is one of asio::ip::udp::socket, asio::ip::tcp::socket, asio::ip::tcp::acceptor&
  bool called = false;
  void operator() (S& sock) { called = true; }
};

template <typename IOT>
struct io_output_visitor {
  bool called = false;
  void operator() (chops::net::basic_io_output<IOT> io) { called = true; }
};

SCENARIO ( "Net entity default construction", "[net_entity]" ) {

  chops::net::net_entity net_ent { };

  GIVEN ("A default constructed net_entity") {
    WHEN ("is_valid is called") {
      THEN ("the return is false") {
        REQUIRE_FALSE (net_ent.is_valid());
      }
    }
    AND_WHEN ("methods are called on an invalid net_entity") {
      THEN ("an appropriate error code is returned") {
        REQUIRE_FALSE (net_ent.is_started());
        REQUIRE_FALSE (net_ent.visit_socket(socket_visitor<asio::ip::udp::socket>()));
        REQUIRE_FALSE (net_ent.visit_socket(socket_visitor<asio::ip::tcp::socket>()));
        REQUIRE_FALSE (net_ent.visit_socket(socket_visitor<asio::ip::tcp::acceptor>()));
        REQUIRE_FALSE (net_ent.visit_io_output(io_output_visitor<chops::net::tcp_io>()));
        REQUIRE_FALSE (net_ent.visit_io_output(io_output_visitor<chops::net::udp_io>()));
        REQUIRE_FALSE (net_ent.start(io_state_chg<chops::net::udp_io>(), 
                                     chops::net::udp_empty_error_func));
        REQUIRE_FALSE (net_ent.start(io_state_chg<chops::net::tcp_io>(), 
                                     chops::net::tcp_empty_error_func));
        REQUIRE_FALSE (net_ent.stop());
      }
    }
  } // end given

}

template <typename IOT, typename S>
void test_methods (chops::net::net_entity& net_ent, chops::net::err_wait_q& err_wq) {

  GIVEN ("A constructed net_entity") {
    WHEN ("is_valid is called") {
      THEN ("is_valid is true") {
        REQUIRE (net_ent.is_valid());
      }
    }
    AND_WHEN ("is_started is called") {
      THEN ("false is returned") {
        auto r = net_ent.is_started();
        REQUIRE_FALSE (*r);
      }
    }
    AND_WHEN ("stop is called") {
      THEN ("false is returned") {
        auto r = net_ent.stop();
        REQUIRE_FALSE (r);
        INFO("Error code: " << r.error());
      }
    }
    AND_WHEN ("start is called") {
      THEN ("the call succeeds and is_started is true") {
        REQUIRE (net_ent.start(io_state_chg<IOT>(), 
                 chops::net::make_error_func_with_wait_queue<IOT>(err_wq)));
        auto r = net_ent.is_started();
        REQUIRE (*r);
      }
    }
    AND_WHEN ("visit_socket is called") {
      THEN ("the call succeeds and a visit flag is set") {
        socket_visitor<S> sv;
        REQUIRE_FALSE (sv.called);
        REQUIRE (net_ent.visit_socket(sv));
        REQUIRE (sv.called);
      }
    }
    AND_WHEN ("stop is called") {
      THEN ("true is returned") {
        REQUIRE (net_ent.stop());
      }
    }
    AND_WHEN ("visit_io_output is called") {
      THEN ("the call succeeds and 0 is returned") {
        io_output_visitor<IOT> iov;
        REQUIRE_FALSE (iov.called);
        auto r = net_ent.visit_io_output(iov);
        REQUIRE (r);
        REQUIRE (*r == 0u);
      }
    }
  } // end given
}

SCENARIO ( "Net entity method testing, UDP entity, TCP acceptor, TCP connector", 
           "[net_entity] [udp_entity] [tcp_acceptor] [tcp_connector]" ) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();

  chops::net::err_wait_q err_wq;
  auto err_fut = std::async(std::launch::async, 
  chops::net::ostream_error_sink_with_wait_queue, std::ref(err_wq), std::ref(std::cerr));


  auto sp1 = std::make_shared<chops::net::detail::tcp_connector>(ioc, std::string_view(test_port_tcp1),
                                                                std::string_view(test_host),
                                                                std::chrono::milliseconds(ReconnTime));
  chops::net::net_entity ne1(sp1);
  test_methods<chops::net::tcp_io, asio::ip::tcp::socket>(ne1, err_wq);

  auto sp2 = std::make_shared<chops::net::detail::tcp_acceptor>(ioc, test_port_tcp2, test_host, true);
  chops::net::net_entity ne2(sp2);
  test_methods<chops::net::tcp_io, asio::ip::tcp::acceptor>(ne2, err_wq);

  auto sp3 = std::make_shared<chops::net::detail::udp_entity_io>(ioc, test_port_udp, test_host);
  chops::net::net_entity ne3(sp3);
  test_methods<chops::net::udp_io, asio::ip::udp::socket>(ne3, err_wq);

  std::this_thread::sleep_for(std::chrono::seconds(1)); // give network entities time to shut down

  while (!err_wq.empty()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  err_wq.close();
  auto err_cnt = err_fut.get();
  INFO ("Num err messages in sink: " << err_cnt);

  wk.reset();

}

/*
SCENARIO ( "Net entity comparison testing", "[net_entity]" ) {

  chops::net::net_entity net_ent1 { };
  auto e1 = std::make_shared<EH>();
  chops::net::net_entity net_ent2(e1);
  chops::net::net_entity net_ent3 { };
  auto e2 = std::make_shared<EH>();
  chops::net::net_entity net_ent4(e2);
  chops::net::net_entity net_ent5 { };

  GIVEN ("Three default constructed net_entities and two with entities") {
    WHEN ("all three are inserted in a multiset") {
      std::multiset<chops::net::net_entity> a_set { net_ent1, net_ent2, net_ent3, net_ent4, net_ent5 };
      THEN ("the invalid net_entitie are first in the set") {
        REQUIRE (a_set.size() == 5);
        auto i = a_set.cbegin();
        REQUIRE_FALSE (i->is_valid());
        ++i;
        REQUIRE_FALSE (i->is_valid());
        ++i;
        REQUIRE_FALSE (i->is_valid());
        ++i;
        REQUIRE (i->is_valid());
        ++i;
        REQUIRE (i->is_valid());
      }
    }
    AND_WHEN ("two invalid net_entities are compared for equality") {
      THEN ("they compare equal") {
        REQUIRE (net_ent1 == net_ent3);
        REQUIRE (net_ent3 == net_ent5);
      }
    }
    AND_WHEN ("two valid net_entities are compared for equality") {
      THEN ("they compare equal if both point to the same io handler") {
        REQUIRE_FALSE (net_ent2 == net_ent4);
        net_ent2 = net_ent4;
        REQUIRE (net_ent2 == net_ent4);
      }
    }
    AND_WHEN ("an invalid net_entity is order compared with a valid net_entity") {
      THEN ("the invalid compares less than the valid") {
        REQUIRE (net_ent1 < net_ent2);
      }
    }
  } // end given

}
*/



