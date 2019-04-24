/**
 *  @file
 * 
 *  @ingroup example_module
 *  
 *  @brief TCP connector (client) that sends binary text message to
 *  server, receives message back converted to upper case.
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
echo_binary_text_client_demo.cpp -lpthread -o echo_client
 * 
 */

#include <iostream>
#include <cstdlib> // EXIT_SUCCESS
#include <cstddef> // std::size_t 
#include <string>
#include <thread>
#include <chrono>
#include <cassert>

#include "net_ip/net_ip.hpp"
#include "net_ip/basic_net_entity.hpp"
#include "net_ip/component/worker.hpp"
#include "asio.hpp"

using io_context = asio::io_context;
using io_interface = chops::net::tcp_io_interface;
using const_buf = asio::const_buffer;
using endpoint = asio::ip::tcp::endpoint;

const std::size_t HDR_SIZE = 2; // 1st 2 bytes of header is message size
const std::string PORT = "5002";
const std::string IP_ADDR = "127.0.0.1";

io_interface tcp_iof; // use this to send text messages

int main(int argc, char* argv[]) {

    bool print_errors = true;

    /* lambda handlers */
    // message handler
    // receive text, convert to uppercase, send back to client
    auto msg_hndlr = [&] (const_buf buf, io_interface iof, endpoint ep) {
        std::cerr << "msg_hndlr: buf.size() = " << buf.size() << std::endl;
        return true;
    };

    auto msg_frame = [&] (asio::mutable_buffer buf) -> std::size_t {
        bool hdr_processed = false;
        std::cerr << "msg_frame: buf.size() = " << buf.size();
        std::cerr << ", hdr_processed: " << (hdr_processed ? "true" : "false") << std::endl;

        if (hdr_processed) {
            hdr_processed = false;
            return 0;
        } else {
            hdr_processed = true;
            return buf.size();
        }
    };

    auto io_state_chng_hndlr = [&] (io_interface iof, std::size_t n, bool flag) {
        std::cerr << "io_state_chng_hndlr: flag = " << (flag ? "true" : "false") << std::endl;

        if (flag) {
            iof.start_io(HDR_SIZE, msg_hndlr, msg_frame);
            tcp_iof = iof;
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
    chops::net::tcp_connector_net_entity net_entity_connect;
    net_entity_connect = echo_server.make_tcp_connector(PORT.c_str(), IP_ADDR.c_str(),
                        std::chrono::milliseconds(5000));
    assert(net_entity_connect.is_valid());
    // start network entity, emplace handlers
    net_entity_connect.start(io_state_chng_hndlr, err_func);

    std::cout << "binary text demo - client" << std::endl;
    std::cout << "enter text to send, or \'quit\' to exit" << std::endl;

    bool shutdown = false;
    std::string s;
    while (!shutdown) {
        std::getline (std::cin, s); // user input
        if (s == "quit") {
            shutdown = true;
            continue;
        }
        // tcp.iof is not valid when there is no network connection
        if (!tcp_iof.is_valid()) {
            std::cout << "no connextion" << std::endl;
            continue; // back to top of loop
        }
        
        chops::mutable_shared_buffer buf;
        
        // 1st 2 bytes size of string
        uint16_t size_val = (uint16_t)HDR_SIZE;
        buf.append(&size_val, sizeof(size_val));
        buf.append(s.data(), s.size());
        std::cout << "buf.size() = " << buf.size() << std::endl;
        tcp_iof.send(buf.data(), buf.size());
    }

    net_entity_connect.stop();

    wk.stop();

    return EXIT_SUCCESS;
}