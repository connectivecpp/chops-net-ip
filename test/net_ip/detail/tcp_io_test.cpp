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

#include <experimental/internet>
#include <experimental/socket>
#include <experimental/io_context>
#include <experimental/executor>

#include <system_error> // std::error_code
#include <cstddef> // std::size_t
#include <memory> // std::make_shared
#include <utility> // std::move
#include <thread>
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
constexpr int num_msgs = 50;

struct entity_notifier {

  std::error_code err;
  std::shared_ptr<detail::tcp_io> ptr;

  void notify_me()(const std::error_code& e, std::shared_ptr<detail::tcp_io> p) {
    err = e;
    ptr = p;
    wq.close();
  }

};

void acceptor_func (io_context& ioc, bool reply) {

  ip::tcp::acceptor acc(ioc, ip::tcp::endpoint(ip::tcp::v4(), test_port));
  auto ioh = std::make_shared<tcp_io>(std::move(acc.accept()), notifier);
  msg_hdlr<detail::tcp_io> mh (reply);

  ioh->start_io(mh, variable_len_msg_frame, 2);

}

void connector_func (io_context& ioc, entity_notifier& en, int cnt) {
  using namespace std::placeholders;

  ip::tcp::socket sock(ioc);
  sock.connect(ip::tcp::endpoint(ip::address::make_address(test_addr), test_port));
  chops::wait_queue<int> wq;
  entity_notifier en(wq);
  notifier_cb cb(std::bind(&entity_notifier::notify_me, &en, _1, _2));

  auto iohp = std::make_shared<tcp_io>(std::move(sock), cb);
  msg_hdlr<detail::tcp_io> mh (false);

  ioh->start_io(mh, variable_len_msg_frame, 2);

  auto ms = make_msg_set (make_variable_len_msg, "Heehaw!", 'Q', cnt);

  for (auto buf : ms) {
    iohp->send(buf);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  ioh->send(make_variable_len_msg(chops::mutable_shared_buffer())); // send empty body, shutdown signal
  auto a = wq.wait_and_pop();

}




SCENARIO ( "Tcp IO handler test, one-way", "[tcp_io_one_way]" ) {

  GIVEN ("An executor work guard") {
    io_context ioc;
    wk_guard wg { make_work_guard(ioc) };

  } // end given
}

/*
SCENARIO ( "Io common test, tcp", "[io_common_tcp]" ) {
}
*/

