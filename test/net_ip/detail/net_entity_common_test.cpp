/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c net_entity_common detail class.
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

#include "net_ip/detail/net_entity_common.hpp"
#include "net_ip/basic_io_interface.hpp"
#include "net_ip/net_ip_error.hpp"

template<typename IOH>
struct state_change {

  bool called = false;
  std::size_t num = 0;
  bool ioh_valid = false;

  void operator() (chops::net::basic_io_interface<IOH> ioh, std::size_t n, bool starting) {
    called = true;
    num = n;
    ioh_valid = ioh.is_valid();
  }
};

template<typename IOH>
struct err_callback {

  bool called = false;
  bool ioh_valid = false;
  std::error_code err;

  void operator() (chops::net::basic_io_interface<IOH> ioh, std::error_code e) {
    called = true;
    err = e;
    ioh_valid = ioh.is_valid();
  }
};

template <typename IOH>
void net_entity_common_test() {

  using namespace chops::net;

  state_change<IOH> state_chg;
  REQUIRE_FALSE (state_chg.called);
  REQUIRE_FALSE (state_chg.ioh_valid);

  err_callback<IOH> err_cb;
  REQUIRE_FALSE (err_cb.called);
  REQUIRE_FALSE (err_cb.ioh_valid);

  detail::net_entity_common<IOH> ne { };
  REQUIRE_FALSE (ne.is_started());

  auto iohp = std::make_shared<IOH>();

  GIVEN ("A default constructed net_entity_common and a state change object") {

    WHEN ("Start with both function objects is called") {
      ne.start(std::ref(state_chg), std::ref(err_cb));
      THEN ("net entity base is started") {
        REQUIRE (ne.is_started());
      }
    }

    AND_WHEN ("Start with only the state change function object is called") {
      ne.start(std::ref(state_chg));
      THEN ("net entity base is started") {
        REQUIRE (ne.is_started());
      }
    }

    AND_WHEN ("Stop is called") {
      ne.start(std::ref(state_chg), std::ref(err_cb));
      ne.stop();
      THEN ("net entity base is not started") {
        REQUIRE_FALSE (ne.is_started());
      }
    }

    AND_WHEN ("State change and error callbacks are invoked") {
      ne.start(std::ref(state_chg), std::ref(err_cb));
      ne.call_state_chg_cb(iohp, 43, true);
      ne.call_error_cb(iohp, std::make_error_code(net_ip_errc::tcp_io_handler_stopped));
      THEN ("function object vals are set correctly") {
        REQUIRE (state_chg.called);
        REQUIRE (state_chg.ioh_valid);
        REQUIRE (state_chg.num == 43);

        REQUIRE (err_cb.called);
        REQUIRE (err_cb.ioh_valid);
        REQUIRE (err_cb.err);
      }
    }

    AND_WHEN ("Error callback is invoked with an empty function") {
      ne.start(std::ref(state_chg));
      ne.call_error_cb(std::shared_ptr<IOH>(),
                       std::make_error_code(net_ip_errc::tcp_io_handler_stopped));
      THEN ("function object vals are not set") {
        REQUIRE_FALSE (err_cb.called);
        REQUIRE_FALSE (err_cb.ioh_valid);
        REQUIRE_FALSE (err_cb.err);
      }
    }

  } // end given
}

struct io_mock {
  using endpoint_type = double;
  using socket_type = double;

};

SCENARIO ( "Net entity base test", "[net_entity_common]" ) {
  net_entity_common_test<io_mock>();
}

