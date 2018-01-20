/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c net_entity_base detail class.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#include "catch.hpp"

#include <memory> // std::shared_ptr
#include <system_error> // std::error_code
#include <utility> // std::move
#include <functional> // std::ref
#include <cstddef> // std::size_t

#include "net_ip/detail/net_entity_base.hpp"
#include "net_ip/io_interface.hpp"
#include "net_ip/net_ip_error.hpp"

template<typename IOH>
struct state_change {
  bool called = false;
  std::size_t num;
  std::error_code err;
  bool ioh_valid;
  void operator() (chops::net::io_interface<IOH> ioh, std::size_t n) {
    called = true;
    num = n;
    ioh_valid = ioh.is_valid();
  }
  void operator() (chops::net::io_interface<IOH> ioh, std::error_code e, std::size_t n) {
    called = true;
    num = n;
    err = e;
    ioh_valid = ioh.is_valid();
  }
};

template <typename IOH>
void net_entity_base_test() {

  using namespace chops::net;

  state_change<IOH> state_chg;
  REQUIRE_FALSE (state_chg.called);

  detail::net_entity_base<IOH> ne { };
  REQUIRE_FALSE (ne.is_started());
  REQUIRE(ne.size() == 0);

  auto iohp = std::make_shared<IOH>();

  GIVEN ("A default constructed net_entity_base and a state change object") {

    WHEN ("Start is called") {
      ne.start(std::ref(state_chg), std::ref(state_chg));
      THEN ("net entity base is started") {
        REQUIRE (ne.is_started());
      }
    }

    AND_WHEN ("Add handler is called") {
      ne.add_handler(iohp);
      THEN ("size increases by 1") {
        REQUIRE (ne.size() == 1);
      }
    }

    AND_WHEN ("Remove handler is called") {
      ne.remove_handler(iohp);
      THEN ("size decreases by 1") {
        REQUIRE (ne.size() == 0);
      }
    }

    AND_WHEN ("Handlers are added and start state change is called") {
      ne.start(std::ref(state_chg), std::ref(state_chg));
      ne.add_handler(iohp);
      ne.add_handler(iohp);
      ne.call_start_change_cb(iohp);
      THEN ("state change internal vals are set correctly") {
        REQUIRE (state_chg.num == 2);
        REQUIRE (state_chg.ioh_valid);
      }
    }

    AND_WHEN ("Handlers are added and shutdown state change is called") {
      ne.start(std::ref(state_chg), std::ref(state_chg));
      ne.add_handler(iohp);
      ne.add_handler(iohp);
      ne.call_shutdown_change_cb(std::make_error_code(net_ip_errc::io_handler_stopped), 
                                 std::shared_ptr<IOH>());
      THEN ("state change internal vals are set correctly") {
        REQUIRE (state_chg.num == 2);
        REQUIRE (state_chg.err);
        REQUIRE_FALSE (state_chg.ioh_valid);
      }
    }

    AND_WHEN ("Distinct handlers are added and stop io all is called") {
      ne.start(std::ref(state_chg), std::ref(state_chg));
      auto iohp1 = std::make_shared<IOH>();
      REQUIRE_FALSE(iohp1->stop_io_called);
      ne.add_handler(iohp1);
      auto iohp2 = std::make_shared<IOH>();
      REQUIRE_FALSE(iohp2->stop_io_called);
      ne.add_handler(iohp2);
      ne.stop_io_all();
      THEN ("state change internal vals are set correctly") {
        REQUIRE(iohp1->stop_io_called);
        REQUIRE(iohp2->stop_io_called);
        ne.stop();
        REQUIRE(ne.size() == 0);
      }
    }

  } // end given
}

struct tcp_io_mock {
  bool stop_io_called = false;
  using endpoint_type = int;
  using socket_type = int;
  void stop_io() { stop_io_called = true; }
};

struct udp_io_mock {
  bool stop_io_called = false;
  using endpoint_type = double;
  using socket_type = double;
  void stop_io() { stop_io_called = true; }
};

SCENARIO ( "Net entity base test, udp",
           "[net_entity_base] [udp]" ) {
  net_entity_base_test<udp_io_mock>();
}

SCENARIO ( "Net entity base test, tcp",
           "[net_entity_base] [tcp]" ) {
  net_entity_base_test<tcp_io_mock>();
}

