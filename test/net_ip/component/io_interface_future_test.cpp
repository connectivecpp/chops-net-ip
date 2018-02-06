/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenario for @c make_io_interface_future functions.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#include "catch.hpp"

#include <cstddef> // std::size_t
#include <chrono>
#include <thread>
#include <system_error>

#include "net_ip/component/io_interface_future.hpp"
#include "net_ip/net_entity.hpp"


struct io_mock1 {
  int magic = 42;
};

struct io_mock2 {
  int magic = 66;
};

template <typename IO>
struct entity_mock {
  using io_interface_type = IO;
  template <typename F>
  void start(F&& func) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    func(IO(), std::error_code(), 0);
  }
};

SCENARIO ( "Testing make_aaa_io_interface_future, where aaa is both tcp and udp",
           "[io_interface_future]" ) {

  using entity1 = entity_mock<io_mock1>;
  using entity2 = entity_mock<io_mock2>;

  auto ep1 = chops::net::net_entity<entity1>(std::make_shared<entity1>());
  auto ep2 = chops::net::net_entity<entity2>(std::make_shared<entity2>());
  
  GIVEN ("Two net_entity objects") {
    WHEN ("make_tcp_io_interface_future is called") {
      auto tcp_fut = chops::net::make_tcp_io_interface_future(ep1);
      THEN ("a future is returned, which returns a value when set") {
          auto tcp_val = tcp_fut.get();
          REQUIRE (tcp_val.magic == 42);
      }
    }
    AND_WHEN ("make_udp_io_interface_future is called") {
      auto udp_fut = chops::net::make_udp_io_interface_future(ep2);
      THEN ("a future is returned, which returns a value when set") {
          auto udp_val = udp_fut.get();
          REQUIRE (udp_val.magic == 66);
      }
    }
  } // end given
}



