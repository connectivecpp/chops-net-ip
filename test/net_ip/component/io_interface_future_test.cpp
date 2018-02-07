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

#include <chrono>
#include <thread>
#include <memory>

#include "net_ip/component/io_interface_future.hpp"
#include "net_ip/basic_net_entity.hpp"
#include "net_ip/basic_io_interface.hpp"


struct io_mock {
  using socket_type = int;
  using endpoint_type = long;

  socket_type& get_socket() { return magic; }

  int magic = 42;
};

using io_interface_mock = chops::net::basic_io_interface<io_mock>;

struct entity_mock {

  using socket_type = int;
  using endpoint_type = double;

  template <typename F>
  void start(F&& func) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    func(io_interface_mock(std::make_shared<io_mock>()), 1);
  }
};

using net_entity_mock = chops::net::basic_net_entity<entity_mock>;

SCENARIO ( "Testing make_io_interface_future_impl",
           "[io_interface_future]" ) {

  auto ent = net_entity_mock (std::make_shared<entity_mock>());
  
  GIVEN ("A entity object") {
    WHEN ("make_io_interface_future_impl is called") {
      auto fut = 
        chops::net::detail::make_io_interface_future_impl<io_interface_mock, net_entity_mock>(ent);

      THEN ("a future is returned, which returns a value when ready") {
          auto val = fut.get();
          REQUIRE (val.get_socket() == 42);
      }
    }
  } // end given
}



