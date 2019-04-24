/**
 *  @file
 * 
 *  @ingroup example_module
 *  
 *  @brief TCP acceptor (server) that receives binary text messages, converts
 *  to upper case, then echos back to TCP connector (client).
 * 
 *  @author Thurman Gillespy
 * 
 *  Copyright (c) Thurman Gillespy
 *  4/23/19
 * 
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *  
 *  Sample make file:
g++ -std=c++17 -Wall -Werror \
-I ../include \
-I ~/Projects/utility-rack/include/ \
-I ~/Projects/asio/asio/include/ \
-I ~/Projects/boost_1_69_0/ \
echo_binary_text_server_demo.cpp -lpthread -o echo_server
 * 
 */

#include <iostream>
#include <cstdlib> // EXIT_SUCCESS
#include <cstddef> // std::size_t 
#include <string>
#include <thread>
#include <cassert>

#include "net_ip/net_ip.hpp"
#include "net_ip/basic_net_entity.hpp"
#include "net_ip/component/worker.hpp"
#include "asio.hpp"

using io_context = asio::io_context;
using io_interface = chops::net::tcp_io_interface;
using const_buf = asio::const_buffer;
using endpoint = asio::ip::tcp::endpoint;

const std::size_t HDR_SIZE = 4; // 1st 4 bytes of header is message size
const std::string PORT = "5002";

int main(int argc, char* argv[]) {

    bool print_errors = true;
    
    /* lambda handlers */
    // message handler
    // receive text, convert to uppercase, send back to client
    auto msg_hndlr = [&] (const_buf buf, io_interface iof, endpoint ep) {
        return true;
    };

    auto msg_frame = [&] (asio::mutable_buffer buf) -> std::size_t {
        return buf.size();
    };

    auto io_state_chng_hndlr = [&] (io_interface iof, std::size_t n, bool flag) {

        if (flag) {
            iof.start_io(HDR_SIZE, msg_hndlr, msg_frame);
        } else {
            iof.stop_io();
        }
    
    };

    // error handler
    auto err_func = [&] (io_interface iof, std::error_code err) {
        if (print_errors) {
            std::string err_text = err.category().name();
            err_text += ": " + std::to_string(err.value()) + ", " +
            err.message();
            std::cerr << err_text << std::endl;
        }
    };

    // work guard - handles @c std::thread and @c asio::io_context management
    chops::net::worker wk;
    wk.start();
    
    // create @c net_ip instance
    chops::net::net_ip echo_server(wk.get_io_context());
    chops::net::tcp_acceptor_net_entity net_entity_accept;

    // make @ tcp_acceptor, return @c network_entity
    net_entity_accept = echo_server.make_tcp_acceptor(PORT.c_str());
    assert(net_entity_accept.is_valid());
    // start network entity, emplace handlers
    net_entity_accept.start(io_state_chng_hndlr, err_func);

    std::cout << "Press return to exit" << std::endl;

    std::string s;
    std::getline(std::cin, s); // pause until return

    net_entity_accept.stop();

    wk.stop();
}