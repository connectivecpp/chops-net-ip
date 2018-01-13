/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c make_endpoint and @c resolve_endpoint functions.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#include "catch.hpp"

#include <experimental/internet>
#include <experimental/socket>
#include <experimental/io_context>

#include <system_error> // std::error_code
#include <utility> // std::pair
#include <future>
#include <string_view>

#include "net_ip/make_endpoint.hpp"
#include "net_ip/worker.hpp"

#include <iostream>

using namespace std::experimental::net;

template <typename Protocol>
void make_endpoint_test (bool local, std::string_view host, std::string_view port) {

  using res_results = ip::basic_resolver_results<Protocol>;
  using endpoints = std::vector<ip::basic_endpoint<Protocol> >;
  using prom_ret = std::pair<std::error_code, endpoints>;

  chops::net::worker wk;
  wk.start();

  GIVEN ("An executor work guard, host, and port strings") {
    WHEN ("make_endpoint is called") {
      THEN ("a sequence of endpoints is returned through a function object callback") {

        std::promise<prom_ret> res_prom;
        auto fut = res_prom.get_future();
        chops::net::make_endpoint<Protocol>(wk.get_io_context(),
          [&res_prom] (const std::error_code& err, res_results res) {
std::cerr << "In lambda, ready to copy results" << std::endl;
              endpoints ends { };
              for (auto i : res) {
                ends.push_back(i.endpoint());
              }
std::cerr << "In lambda, size of ends: " << ends.size() << std::endl;
              // res_prom.set_value(prom_ret(err, res));
              res_prom.set_value(prom_ret(err, ends));
            }, local, host, port);
        auto a = fut.get();

        if (a.first) {
          INFO ("Error val: " << a.first);
        }
        REQUIRE_FALSE (a.second.empty());
        for (auto i : a.second) {
          INFO ("-- Endpoint: " << i);
        }
      }
    }
  } // end given
  wk.stop();

}


SCENARIO ( "Make endpoint test, TCP  1", "[tcp_make_endpoint_1]" ) {

  make_endpoint_test<ip::tcp> (false, "www.cnn.com", "80");

}

SCENARIO ( "Make endpoint test, TCP 2", "[tcp_make_endpoint_2]" ) {

  make_endpoint_test<ip::tcp> (false, "www.seattletimes.com", "80");

}

