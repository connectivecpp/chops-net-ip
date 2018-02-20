/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenario for error delivery component.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#include "catch.hpp"

#include <cstddef> // std::size_t
#include <system_error>
#include <future>
#include <functional> // std::ref

#include <iostream>

#include "net_ip/net_ip_error.hpp"

#include "net_ip/component/error_delivery.hpp"

#include "queue/wait_queue.hpp"

#include "net_ip/shared_utility_test.hpp"

SCENARIO ( "Testing ostream_error_sink_with_wait_queue function",
           "[error_delivery]" ) {

  using namespace chops::test;

  auto ioh1 = std::make_shared<io_handler_mock>();
  auto ioh2 = std::make_shared<io_handler_mock>();
  auto ioh3 = std::make_shared<io_handler_mock>();

  auto io1 = io_interface_mock(ioh1);
  auto io2 = io_interface_mock(ioh2);
  auto io3 = io_interface_mock(ioh3);

  err_wait_q<io_handler_mock> wq;

  auto sink_fut = std::async(std::launch::async, ostream_error_sink_with_wait_queue,
                             std::ref(wq), std::ref(std::cerr));

  auto err_func = make_error_func_with_wait_queue<io_handler_mock>(wq);

  GIVEN ("A wait_queue, three io interface objects, a source func and a sink thread") {
    WHEN ("the source func is called") {
      err_func(io1, std::make_error_code(chops::net::net_ip_errc::udp_io_handler_stopped));
      err_func(io2, std::make_error_code(chops::net::net_ip_errc::tcp_io_handler_stopped));
      err_func(io3, std::make_error_code(chops::net::net_ip_errc::message_handler_terminated));
      err_func(io2, std::make_error_code(chops::net::net_ip_errc::tcp_connector_stopped));
      err_func(io1, std::make_error_code(chops::net::net_ip_errc::tcp_acceptor_stopped));
      THEN ("the elements are passed through the queue and the count is correct") {
        wq.close();
        auto cnt = sink_fut.get();
        REQUIRE (cnt == 5);
      }
    }
  } // end given
}



