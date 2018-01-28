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

  auto iohp = std::make_shared<IOH>();

  GIVEN ("A default constructed net_entity_base and a state change object") {

    WHEN ("Start is called") {
      ne.start(std::ref(state_chg), std::ref(state_chg));
      THEN ("net entity base is started") {
        REQUIRE (ne.is_started());
      }
    }

    AND_WHEN ("Stop is called") {
      ne.start(std::ref(state_chg), std::ref(state_chg));
      ne.stop();
      THEN ("net entity base is not started") {
        REQUIRE_FALSE (ne.is_started());
      }
    }

    AND_WHEN ("Start state change is called") {
      ne.start(std::ref(state_chg), std::ref(state_chg));
      ne.call_start_change_cb(iohp, 42);
      THEN ("state change internal vals are set correctly") {
        REQUIRE (state_chg.num == 42);
        REQUIRE (state_chg.ioh_valid);
      }
    }

    AND_WHEN ("Shutdown state change is called") {
      ne.start(std::ref(state_chg), std::ref(state_chg));
      ne.call_shutdown_change_cb(std::make_error_code(net_ip_errc::tcp_io_handler_stopped), 
                                 std::shared_ptr<IOH>(), 43);
      THEN ("state change internal vals are set correctly") {
        REQUIRE (state_chg.num == 43);
        REQUIRE (state_chg.err);
        REQUIRE_FALSE (state_chg.ioh_valid);
      }
    }

  } // end given
}

struct tcp_io_mock {
  using endpoint_type = int;
  using socket_type = int;
};

struct udp_io_mock {
  using endpoint_type = double;
  using socket_type = double;
};

SCENARIO ( "Net entity base test, udp",
           "[net_entity_base] [udp]" ) {
  net_entity_base_test<udp_io_mock>();
}

SCENARIO ( "Net entity base test, tcp",
           "[net_entity_base] [tcp]" ) {
  net_entity_base_test<tcp_io_mock>();
}

