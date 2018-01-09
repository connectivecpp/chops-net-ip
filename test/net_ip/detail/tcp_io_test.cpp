/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test scenarios for @c tcp_io detail class.
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
#include <string_view>

#include "net_ip/detail/tcp_io.hpp"

#include "../test/net_ip/detail/shared_utility_test.hpp"
#include "utility/shared_buffer.hpp"

using namespace std::experimental::net;
using namespace chops::test;

using wk_guard = executor_work_guard<io_context::executor_type>;

using notifier_cb = 
  typename chops::net::detail::io_common<chops::net::detail::tcp_io>::entity_notifier_cb;

constexpr int test_port = 30434;
const char*   test_addr = "127.0.0.1";
constexpr int NumMsgs = 50;
constexpr int Interval = 50;


using notifier_data = std::tuple<std::error_code, std::shared_ptr<chops::net::detail::tcp_io> >;

struct entity_notifier {
  entity_notifier() = default;

  std::promise<notifier_data> prom;

  void notify_me(const std::error_code& e, std::shared_ptr<chops::net::detail::tcp_io> p) {
    p->close();
    prom.set_value(notifier_data(e, p));
  }

};

// Catch test framework is not thread-safe, therefore all REQUIRE clauses must be in a single 
// thread; return data: 1. Error code 2. Does the ioh ptr match? 3. Do the msg sets match?
using thread_data = std::tuple<std::error_code, bool, bool>; 
using thread_promise = std::promise<thread_data>;

using tcp_ioh_ptr = std::shared_ptr<chops::net::detail::tcp_io>;

void connector_func (thread_promise thr_prom, const vec_buf& in_msg_set, io_context& ioc, 
                     int interval, std::string_view delim, chops::const_shared_buffer empty_msg) {
  using namespace std::placeholders;

  entity_notifier en { };
  auto en_fut = en.prom.get_future();
  notifier_cb cb(std::bind(&entity_notifier::notify_me, &en, _1, _2));

  ip::tcp::socket sock(ioc);
  sock.connect(ip::tcp::endpoint(ip::make_address(test_addr), test_port));

  auto iohp = std::make_shared<chops::net::detail::tcp_io>(std::move(sock), cb);
  vec_buf vb;
  iohp->start_io(msg_hdlr<chops::net::detail::tcp_io>(vb, false), 
                 chops::net::make_simple_variable_len_msg_frame(decode_variable_len_msg_hdr), 2);

  for (auto buf : in_msg_set) {
    iohp->send(chops::const_shared_buffer(std::move(buf)));
    std::this_thread::sleep_for(std::chrono::milliseconds(interval));
  }
  iohp->send(empty_msg);

  auto ret = en_fut.get(); // wait for termination callback
  thr_prom.set_value(thread_data(std::get<0>(ret), std::get<1>(ret) == iohp, vb.empty()));

}

void acc_conn_test (const vec_buf& in_msg_set, bool reply, int interval, std::string_view delim,
                    chops::const_shared_buffer empty_msg) {

  using namespace std::placeholders;

  io_context ioc;
  wk_guard wg { make_work_guard(ioc) };
  std::thread run_thr([&ioc] () { ioc.run(); } );

  GIVEN ("An executor work guard and a message set") {
 
    WHEN ("an acceptor and connector are created") {
      THEN ("the futures provide synchronization and data returns") {

        ip::tcp::acceptor acc(ioc, ip::tcp::endpoint(ip::address_v4::any(), test_port));

        entity_notifier en { };
        auto en_fut = en.prom.get_future();
        notifier_cb cb(std::bind(&entity_notifier::notify_me, &en, _1, _2));

        INFO ("Creating connector thread, msg interval: " << interval);

        thread_promise conn_prom;
        auto conn_fut = conn_prom.get_future();
        std::thread conn_thr(connector_func, std::move(conn_prom), std::cref(in_msg_set), std::ref(ioc), 
                             interval, delim, empty_msg);

        auto iohp = std::make_shared<chops::net::detail::tcp_io>(std::move(acc.accept()), cb);
        vec_buf vb;
        if (delim.empty()) {
          iohp->start_io(msg_hdlr<chops::net::detail::tcp_io>(vb, reply), 
                         chops::net::make_simple_variable_len_msg_frame(decode_variable_len_msg_hdr), 2);
        }
        else {
          iohp->start_io(msg_hdlr<chops::net::detail::tcp_io>(vb, reply), delim);
        }

        auto en_ret = en_fut.get(); // wait for termination callback
        INFO ("Entity future popped");

        auto conn_data = conn_fut.get();
        INFO ("Connector thread future popped, joining thread");
        conn_thr.join();
        std::error_code e = std::get<0>(en_ret);
        INFO ("Acceptor error code and msg: " << e << " " << e.message() );
        e = std::get<0>(conn_data);
        INFO ("Connector error code and msg: " << e << " " << e.message());
        REQUIRE (std::get<1>(en_ret) == iohp);
        REQUIRE (std::get<1>(conn_data));
        REQUIRE (in_msg_set == vb);
        REQUIRE (std::get<2>(conn_data));
      }
    }
  } // end given
  // wg.reset();
  ioc.stop();
  run_thr.join();

}


SCENARIO ( "Tcp IO handler test, variable len msgs, interval 50, one-way", "[tcp_io_one_way_var]" ) {

  auto ms = make_msg_set (make_variable_len_msg, "Heehaw!", 'Q', NumMsgs);
  chops::const_shared_buffer empty_msg(make_empty_body_msg(make_variable_len_msg));
  acc_conn_test ( ms, false, 50, std::string_view(), empty_msg );

}

/*
SCENARIO ( "Io common test, tcp", "[io_common_tcp]" ) {
}
*/

