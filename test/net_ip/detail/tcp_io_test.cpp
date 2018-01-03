/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c tcp_io detail class.
 *
 *  The test strategy is to create mock net entity classes to test the interface
 *  and functionality of the @c tcp_io class. In particular, the notification hooks
 *  between the two levels is tricky and needs to be well tested.
 *
 *  The data flow is tested by: ( fill in details )
 *
 *  
 *  @author Cliff Green
 *  @date 2017, 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#include "catch.hpp"

#include <experimental/internet> // endpoint declarations
#include <experimental/socket>
#include <experimental/io_context>

#include <system_error> // std::error_code
#include <string_view>
#include <string>
#include <cstddef> // std::size_t

#include "net_ip/detail/tcp_io.hpp"

#include "utility/repeat.hpp"
#include "utility/shared_buffer.hpp"

#include "../test/net_ip/detail/shared_utility_test.hpp"





  } // end given
}

struct tcp_entity_mock {
  bool notify_called = false;

  void notify_me(const std::error_code& err, std::shared_ptr<chops::net::detail::tcp_io> p) {
    notify_called = true;
  }

};



SCENARIO ( "Tcp IO handler test, one-way", "[tcp_io_one_way]" ) {
  using chops::net::detail::tcp_io;

  auto ioh = std::make_shared<tcp_io>(std::move(sock), 
}

/*
SCENARIO ( "Io common test, tcp", "[io_common_tcp]" ) {
}
*/

