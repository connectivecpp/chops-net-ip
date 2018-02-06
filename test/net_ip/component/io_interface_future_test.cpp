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

#include "net_ip/component/io_interface_future.hpp"

SCENARIO ( "Simple variable len message frame test",
           "[msg_frame]" ) {

  auto mf = chops::net::make_simple_variable_len_msg_frame(decoder_func);

  // Protocol: 1 byte header specifying body len; three msgs in the following byte array
  auto msgs = chops::make_byte_array(0x01, 0xBB, 0x03, 0xAA, 0xDD, 0xEE, 0x04, 0xDE, 0xAD, 0xBE, 0xEF);

  GIVEN ("A simple message frame object constructed with a decoder func") {
    auto a = mf; // verify copying the lambda does the right thing
    WHEN ("it is called multiple times") {
      THEN ("the return value toggles between the decoder supplied number and zero") {
        std::size_t idx = 0;
        mutable_buffer mbuf;
        while (idx < msgs.size()) {
          mbuf = mutable_buffer(msgs.data() + idx, 1);
          auto ret = mf(mbuf);
          INFO("Return from msg frame: " << ret);
          idx += ret;
          mbuf = mutable_buffer(msgs.data() + idx, ret);
          ret = mf(mbuf);
          REQUIRE (ret == 0);
          idx += 1;
        }
      }
    }
  } // end given
}



