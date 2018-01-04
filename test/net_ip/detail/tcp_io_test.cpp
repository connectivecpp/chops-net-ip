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
 *  The data flow is tested by sending a group of messages (variable len, text). 
 *  optionally the message handler will send them back to the originator.
 *
 *  The data flow is terminated by a message with an empty body.
 *  
 *  @author Cliff Green
 *  @date 2017, 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#include "catch.hpp"

#include <experimental/internet>
#include <experimental/socket>
#include <experimental/io_context>
#include <experimental/executor>

#include <system_error> // std::error_code
#include <cstddef> // std::size_t
#include <memory> // std::make_shared
#include <utility> // std::move
#include <tuple>
#include <thread>
#include <future>
#include <chrono>
#include <functional>

#include "net_ip/detail/tcp_io.hpp"

#include "../test/net_ip/detail/shared_utility_test.hpp"

using namespace std::experimental::net;
using namespace chops::test;
using namespace chops::net;

using wk_guard = executor_work_guard<io_context::executor_type>;

using notifier_cb = typename detail::io_common::entity_notifier_cb;

constexpr int test_port = 3434;
const char*   test_addr = "127.0.0.1";
constexpr int NumMsgs = 50;


using notifier_data = std::tuple<std::error_code, std::shared_ptr<detail::tcp_io> >;

struct entity_notifier {

  std::promise<notifier_data> prom;

  void notify_me()(const std::error_code& e, std::shared_ptr<detail::tcp_io> p) {
    prom.set_value(notifier_data(e, p));
  }

};

// Catch is not thread-safe, therefore all REQUIRE clauses must be in a single thread; meaning:
// 1. Error code 2. Does the ioh ptr match? 3. Do the msg sets match?
using thread_data = std::tuple<std::error_code, bool, bool>; 
using thread_promise = std::promise<thread_data>;

void acceptor_func (thread_promise&& thr_prom, const vec_buf& in_msg_set, io_context& ioc, 
                    bool reply) {
  using namespace std::placeholders;

  entity_notifier en { };
  auto fut = en.prom.get_future();
  notifier_cb cb(std::bind(&entity_notifier::notify_me, &en, _1, _2));

  ip::tcp::acceptor acc(ioc, ip::tcp::endpoint(ip::tcp::v4(), test_port));
  auto iohp = std::make_shared<tcp_io>(std::move(acc.accept()), cb);

  msg_hdlr<detail::tcp_io> mh (reply);

  iohp->start_io(mh, variable_len_msg_frame, 2);
  auto ret = fut.get(); // wait for termination callback

  thr_prom.set_value(thread_data(std::get<0>(ret), std::get<1>(ret) == iohp, in_msg_set == mh.msgs));

}

void connector_func (thread_promise&& thr_prom, const vec_buf& in_msg_set, io_context& ioc, 
                     std::chrono::milliseconds interval) {
  using namespace std::placeholders;

  entity_notifier en { };
  auto fut = en.prom.get_future();
  notifier_cb cb(std::bind(&entity_notifier::notify_me, &en, _1, _2));

  ip::tcp::socket sock(ioc);
  sock.connect(ip::tcp::endpoint(ip::address::make_address(test_addr), test_port));
  auto iohp = std::make_shared<tcp_io>(std::move(sock), cb);

  msg_hdlr<detail::tcp_io> mh (false);

  iohp->start_io(mh, variable_len_msg_frame, 2);

  for (auto buf : in_msg_set) {
    iohp->send(buf);
    std::this_thread::sleep_for(interval);
  }
  iohp->send(make_variable_len_msg(chops::mutable_shared_buffer())); // send empty body, shutdown signal
  auto ret = fut.get(); // wait for termination callback

  thr_prom.set_value(thread_data(std::get<0>(ret), std::get<1>(ret) == iohp, in_msg_set == mh.msgs));

}




SCENARIO ( "Tcp IO handler test, one-way", "[tcp_io_one_way]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Heehaw!", 'Q', NumMsgs);

  io_context ioc;
  wk_guard wg { make_work_guard(ioc) };

  GIVEN ("An executor work guard and a message set") {
    WHEN ("an acceptor and connector are created") {
      THEN ("dlkjfkl") {
      }
    }
  } // end given
}

/*
SCENARIO ( "Io common test, tcp", "[io_common_tcp]" ) {
}
*/

