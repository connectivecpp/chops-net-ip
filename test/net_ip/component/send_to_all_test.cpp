/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenario for @c send_to_all class template.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#include "catch.hpp"

#include <cstddef> // std::size_t

#include <memory> // std::make_shared

#include "net_ip/component/send_to_all.hpp"

#include "net_ip/queue_stats.hpp"

#include "utility/shared_buffer.hpp"

constexpr int magic1 = 42;
constexpr int magic2 = 100;

struct ioh_mock {
  using endpoint_type = int;
  using socket_type = double;

  bool send_called = false;

  void send(chops::const_shared_buffer) { send_called = true; }

  chops::net::output_queue_stats get_output_queue_stats() const noexcept {
    return chops::net::output_queue_stats { magic1, magic2 };
  }

};

using io_interface_type = chops::net::basic_io_interface<ioh_mock>;

SCENARIO ( "Testing send_to_all class",
           "[send_to_all]" ) {

  chops::net::send_to_all<ioh_mock> sta { };
  REQUIRE (sta.size() == 0);

  GIVEN ("A default constructed send_to_all object") {
    WHEN ("add_io_interface is called") {
      auto ioh = std::make_shared<ioh_mock>();
      sta.add_io_interface(io_interface_type(ioh));
      THEN ("the size increases by 1") {
        REQUIRE (sta.size() == 1);
      }
    }
    AND_WHEN ("remove_io_interface or function call operator is called") {
      auto ioh1 = std::make_shared<ioh_mock>();
      auto ioh2 = std::make_shared<ioh_mock>();
      auto ioh3 = std::make_shared<ioh_mock>();
      sta.add_io_interface(io_interface_type(ioh1));
      sta.add_io_interface(io_interface_type(ioh2));
      sta.add_io_interface(io_interface_type(ioh3));
      REQUIRE (sta.size() == 3);
      THEN ("the size decreases by 1 for each call") {
        sta.remove_io_interface(io_interface_type(ioh2));
        REQUIRE (sta.size() == 2);
        sta(io_interface_type(ioh1), std::error_code(), 0);
        REQUIRE (sta.size() == 1);
        sta(io_interface_type(), std::error_code(), 0);
        REQUIRE (sta.size() == 1);
        sta.remove_io_interface(io_interface_type(ioh3));
        REQUIRE (sta.size() == 0);
      }
    }
    AND_WHEN ("send is called") {
      std::byte b(static_cast<std::byte>(0xFE));
      chops::const_shared_buffer buf(&b, 1);
      auto ioh1 = std::make_shared<ioh_mock>();
      REQUIRE_FALSE(ioh1->send_called);
      auto ioh2 = std::make_shared<ioh_mock>();
      REQUIRE_FALSE(ioh2->send_called);
      sta.add_io_interface(io_interface_type(ioh1));
      sta.add_io_interface(io_interface_type(ioh2));
      REQUIRE (sta.size() == 2);
      sta.send(buf);
      THEN ("the send flag is true") {
        REQUIRE(ioh1->send_called);
        REQUIRE(ioh2->send_called);
      }
    }
    AND_WHEN ("get_total_output_queue_stats is called") {
      auto ioh1 = std::make_shared<ioh_mock>();
      auto ioh2 = std::make_shared<ioh_mock>();
      sta.add_io_interface(io_interface_type(ioh1));
      sta.add_io_interface(io_interface_type(ioh2));
      THEN ("the right results are returned") {
        auto tot = sta.get_total_output_queue_stats();
        REQUIRE(tot.output_queue_size == sta.size() * magic1);
        REQUIRE(tot.bytes_in_output_queue == sta.size() * magic2);
      }
    }
  } // end given
}



