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

using io_context = asio::io_context;
using io_interface = chops::net::tcp_io_interface;
using const_buf = asio::const_buffer;
using endpoint = asio::ip::tcp::endpoint;

const std::size_t HDR_SIZE = 2; // 1st 2 bytes of header is message size
const std::string PORT = "5002";
const std::string IP_ADDR = "127.0.0.1";

int main(int argc, char* argv[]) {
    io_interface tcp_iof; // use this to send text messages
    bool hdr_processed = false;
    bool print_errors = true;

    /* lambda handlers */

    // message handler
    // receive text, display to console
    auto msg_hndlr = [&] (const_buf buf, io_interface iof, endpoint ep) {
        // convert buf to string, but omit 1st 2 bytes (header)
        std::string s (static_cast<const char*> (buf.data()) + 2, buf.size() - 2);
        std::cout << s << std::endl;
    
        return true;
    };

    auto msg_frame = [&] (asio::mutable_buffer buf) -> std::size_t {
        
        if (hdr_processed) {
            hdr_processed = false;
            return 0;
        } else {
            hdr_processed = true;
            // 1st 2 bytes is message size
            uint16_t size = *(static_cast<uint16_t*> (buf.data()));
            
            return size;
        }
    };

    auto io_state_chng_hndlr = [&] (io_interface iof, std::size_t n, bool flag) {
        
        if (flag) {
            iof.start_io(HDR_SIZE, msg_hndlr, msg_frame);
            tcp_iof = iof; // used later to send text
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

    std::cout << "chops-net-ip binary text echo demo - client" << std::endl;
    std::cout << "Enter text to send, or \'quit\' to exit" << std::endl;

    std::string s;
    bool shutdown = false;
    while (!shutdown) {
        std::getline (std::cin, s); // user input
        if (s == "quit") {
            shutdown = true;
            continue;
        }
        // tcp.iof is not valid when there is no network connection
        if (!tcp_iof.is_valid()) {
            std::cout << "no connection..." << std::endl;
            continue; // back to top of loop
        }
        
        // create buffer to send entered message from user
        chops::mutable_shared_buffer buf;
        
        // 1st 2 bytes size of message are the string length
        uint16_t size_val = s.size();
        buf.append(&size_val, sizeof(size_val)); // put 2 bytes into buffer
        buf.append(s.data(), s.size()); // now add the string
        // send message to server (TCP_acceptor)
        tcp_iof.send(buf.data(), buf.size());
    }

    // cleanup
    net_entity_connect.stop();

    wk.stop();

    return EXIT_SUCCESS;
}