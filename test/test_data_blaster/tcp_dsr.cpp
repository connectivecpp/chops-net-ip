/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test application that sends and receives data as well as sends progress messages to
 *  a monitor application.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2019 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include <system_error> // std::error_code
#include <cstddef> // std::size_t
#include <utility> // std::move
#include <thread>
#include <future>
#include <chrono>
#include <functional> // std::ref, std::cref
#include <string_view>
#include <vector>
#include <cassert>

#include <iostream> // std::cerr for error sink

#include "net_ip/net_ip.hpp"
#include "net_ip/net_entity.hpp"

#include "net_ip_component/worker.hpp"
#include "net_ip_component/io_output_delivery.hpp"
#include "net_ip_component/output_queue_stats.hpp"
#include "net_ip_component/error_delivery.hpp"

#include "shared_test/msg_handling.hpp"
#include "marshall/shared_buffer.hpp"

#include "test_data_blaster/tcp_dsr_args.hpp"

int main (int argc, char* argv[]) {

  auto parms = chops::test::tcp_dsr_args_parse_command_line(argc, argv);
  chops::test::vec_buf msgs{};
  int delay{0};
  if (parms.m_sending_parms) {
    const auto& t = *parms.m_sending_parms;
    delay = t.m_delay;
    msgs = chops::test::make_msg_vec (chops::test::make_variable_len_msg, t.m_prefix, t.m_body_char, t.m_send_count);
  }

  chops::net::worker wk;
  wk.start();
  chops::net::net_ip nip(wk.get_io_context());

  chops::net::err_wait_q err_wq;
  auto err_fut = std::async(std::launch::async, chops::net::ostream_error_sink_with_wait_queue,
                            std::ref(err_wq), std::ref(std::cerr));

  std::promise<void> shutdown_prom;
  auto shutdown_fut = shutdown_prom.get_future();

  for (const auto& s : parms.m_acceptors) {

  }

  shutdown_fut.get();
  err_wq.close();
  auto err_cnt = err_fut.get();
  wk.reset();

}


void perform_test (const vec_buf& var_msg_vec, const vec_buf& fixed_msg_vec,
                   bool reply, int interval, 
                   int num_entities, std::string_view delim, chops::const_shared_buffer empty_msg) {

  chops::net::worker wk;
  wk.start();
  auto& ioc = wk.get_io_context();

  chops::net::err_wait_q err_wq;
  auto err_fut = std::async(std::launch::async, chops::net::ostream_error_sink_with_wait_queue,
                            std::ref(err_wq), std::ref(std::cerr));

  {
    std::size_t total_msgs = num_entities * var_msg_vec.size();
    auto cnt1 = acc_conn_var_test(ioc, err_wq, var_msg_vec, reply, num_entities, delim, empty_msg);
    REQUIRE (cnt1 == total_msgs);
    auto cnt2 = udp_test(ioc, err_wq, var_msg_vec, interval, num_entities);
    CHECK (cnt2 == total_msgs);
  }

  {
    std::size_t total_msgs = num_entities * fixed_msg_vec.size();
    auto cnt1 = acc_conn_fixed_test(ioc, err_wq, fixed_msg_vec, num_entities);
    REQUIRE (cnt1 == total_msgs);
    auto cnt2 = udp_test(ioc, err_wq, fixed_msg_vec, interval, num_entities);
    CHECK (cnt2 == total_msgs);
  }

  while (!err_wq.empty()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  err_wq.close();
  auto err_cnt = err_fut.get();
  INFO ("Num err messages in sink: " << err_cnt);

  wk.reset();
}

TEST_CASE ( "Net IP test, var len msgs, one-way, interval 50, 1 connector or pair", 
            "[netip_acc_conn] [var_len_msg] [one_way] [interval_50] [connectors_1]" ) {

  perform_test(make_msg_vec (make_variable_len_msg, "Heehaw!", 'Q', num_msgs),
               make_fixed_size_msg_vec(num_msgs),
               false, 50, 1,
               std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Net IP test, var len msgs, one-way, interval 0, 1 connector or pair", 
            "[netip_acc_conn] [var_len_msg] [one_way] [interval_0] [connectors_1]" ) {

  perform_test(make_msg_vec (make_variable_len_msg, "Haw!", 'R', 2*num_msgs),
               make_fixed_size_msg_vec(2*num_msgs),
               false, 0, 1,
               std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Net IP test, var len msgs, two-way, interval 50, 1 connector or pair", 
            "[net_ip] [var_len_msg] [two_way] [interval_50] [connectors_1]" ) {

  perform_test(make_msg_vec (make_variable_len_msg, "Yowser!", 'X', num_msgs),
               make_fixed_size_msg_vec(num_msgs),
               true, 50, 1,
               std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Net IP test, var len msgs, two-way, interval 0, 10 connectors or pairs, many msgs", 
            "[net_ip] [var_len_msg] [two_way] [interval_0] [connectors_10] [many]" ) {

  perform_test(make_msg_vec (make_variable_len_msg, "Whoah, fast!", 'X', 30*num_msgs),
               make_fixed_size_msg_vec(30*num_msgs),
               true, 0, 10,
               std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Net IP test, var len msgs, two-way, interval 0, 40 connectors or pairs", 
            "[net_ip] [var_len_msg] [two_way] [interval_0] [connectors_40] [many]" ) {

  perform_test(make_msg_vec (make_variable_len_msg, "Many, many, fast!", 'G', 30*num_msgs),
               make_fixed_size_msg_vec(30*num_msgs),
               true, 0, 40,
               std::string_view(), make_empty_variable_len_msg() );

}

TEST_CASE ( "Net IP test, CR / LF msgs, one-way, interval 50, 1 connector or pair", 
            "[net_ip] [cr_lf_msg] [one_way] [interval_50] [connectors_1]" ) {

  perform_test(make_msg_vec (make_cr_lf_text_msg, "Pretty easy, eh?", 'C', num_msgs),
               make_fixed_size_msg_vec(num_msgs),
               false, 50, 1,
               std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Net IP test, CR / LF msgs, one-way, interval 50, 10 connectors or pairs", 
            "[net_ip] [cr_lf_msg] [one_way] [interval_50] [connectors_10]" ) {

  perform_test(make_msg_vec (make_cr_lf_text_msg, "Hohoho!", 'Q', num_msgs),
               make_fixed_size_msg_vec(num_msgs),
               false, 50, 10,
               std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Net IP test, CR / LF msgs, one-way, interval 0, 20 connectors or pairs", 
            "[net_ip] [cr_lf_msg] [one_way] [interval_0] [connectors_20]" ) {

  perform_test(make_msg_vec (make_cr_lf_text_msg, "HawHeeHaw!", 'N', 4*num_msgs),
               make_fixed_size_msg_vec(4*num_msgs),
               false, 0, 20,
               std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Net IP test, CR / LF msgs, two-way, interval 30, 10 connectors or pairs", 
            "[net_ip] [cr_lf_msg] [two_way] [interval_30] [connectors_10]" ) {

  perform_test(make_msg_vec (make_cr_lf_text_msg, "Yowzah!", 'G', 5*num_msgs),
               make_fixed_size_msg_vec(4*num_msgs),
               true, 30, 10,
               std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Net IP test, CR / LF msgs, two-way, interval 0, 10 connectors or pairs, many msgs", 
            "[net_ip] [cr_lf_msg] [two_way] [interval_0] [connectors_10] [many]" ) {

  perform_test(make_msg_vec (make_cr_lf_text_msg, "Yes, yes, very fast!", 'F', 50*num_msgs),
               make_fixed_size_msg_vec(50*num_msgs),
               true, 0, 10,
               std::string_view("\r\n"), make_empty_cr_lf_text_msg() );

}

TEST_CASE ( "Net IP test,  LF msgs, one-way, interval 50, 1 connector or pair", 
            "[net_ip] [lf_msg] [one_way] [interval_50] [connectors_1]" ) {

  perform_test(make_msg_vec (make_lf_text_msg, "Excited!", 'E', num_msgs),
               make_fixed_size_msg_vec(num_msgs),
               false, 50, 1,
               std::string_view("\n"), make_empty_lf_text_msg() );

}

TEST_CASE ( "Net IP test,  LF msgs, one-way, interval 0, 25 connectors or pairs", 
            "[net_ip] [lf_msg] [one_way] [interval_0] [connectors_25]" ) {

  perform_test(make_msg_vec (make_lf_text_msg, "Excited fast!", 'F', 6*num_msgs),
               make_fixed_size_msg_vec(6*num_msgs),
               false, 0, 25,
               std::string_view("\n"), make_empty_lf_text_msg() );

}

TEST_CASE ( "Net IP test,  LF msgs, two-way, interval 20, 15 connectors or pairs", 
            "[net_ip] [lf_msg] [two_way] [interval_20] [connectors_15]" ) {

  perform_test(make_msg_vec (make_lf_text_msg, "Whup whup!", 'T', 2*num_msgs),
               make_fixed_size_msg_vec(2*num_msgs),
               true, 20, 15,
               std::string_view("\n"), make_empty_lf_text_msg() );

}

TEST_CASE ( "Net IP test,  LF msgs, two-way, interval 0, 15 connectors or pairs, many msgs", 
            "[net_ip] [lf_msg] [two_way] [interval_0] [connectors_15] [many]" ) {

  perform_test(make_msg_vec (make_lf_text_msg, "Super fast!", 'S', 30*num_msgs),
               make_fixed_size_msg_vec(30*num_msgs),
               true, 0, 15,
               std::string_view("\n"), make_empty_lf_text_msg() );

}
