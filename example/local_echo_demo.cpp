/** @file
 * 
 *  @defgroup example_module Example code for Chops Net IP library.
 *
 *  @ingroup example_module
 * 
 *  @brief Example of TCP send/receive text string over local loop network
 *  connection.
 * 
 *  @author Thurman Gillespy
 * 
 *  Copyright (c) 2019 Thurman Gillespy
 *  2019-10-14
 * 
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *  
 *  Sample make file :
g++ -std=c++17 -Wall -Werror \
-I ../include \
-I ../../utility-rack/include/ \
-I ../../asio/asio/include/ \
-I ../../expected-lite/include/ \
 local_echo_demo.cpp -lpthread -o local_echo
 *
 */ 

#include <iostream>
#include <cstdlib> // EXIT_SUCCESS
#include <cstddef> // std::size_t 
#include <string>
#include <string_view>
#include <chrono>
#include <thread>
#include <algorithm> // std::for_each
#include <cassert>

#include "net_ip/net_ip.hpp"
#include "net_ip/net_entity.hpp"
#include "net_ip_component/worker.hpp"
#include "queue/wait_queue.hpp"
#include "net_ip/io_type_decls.hpp"

using io_context = asio::io_context;
using io_output = chops::net::tcp_io_output;
using const_buf = asio::const_buffer;
using tcp_io_interface = chops::net::tcp_io_interface;
using endpoint = asio::ip::tcp::endpoint;

const std::string PORT = "5001";

int main() {
    
    /* lambda callbacks */
    // message handler for @c tcp_connector @c network_entity
    auto msg_hndlr_connect = [] (const_buf buf, io_output io_out, endpoint ep)
        {
            // receive data from acceptor, display to user
            std::string s (static_cast<const char*> (buf.data()), buf.size());
            std::cout << s; // s aleady has terminating '\n'

            return true;
        };
    
    // message handler for @c tcp_acceptor @c message_handler
    // receive data from connector, send back to acceptor
    auto msg_hndlr_accept = [] (const_buf buf, io_output io_out, endpoint ep)
        {
            // copy buffer contents into string, convert to uppercase
            std::string s(static_cast<const char*> (buf.data()), buf.size());
            auto to_upper = [] (char& c) { c = ::toupper(c); };
            std::for_each(s.begin(), s.end(), to_upper);
            // send uppercase string data back over network connection
            io_out.send(s.data(), s.size());
            
            return true;
        };

    // handler for @c tcp_connector
    auto io_state_chng_connect = [msg_hndlr_connect] 
        (tcp_io_interface iof, std::size_t n, bool flag) {
            if (flag && n == 1) {
                iof.start_io("\n", msg_hndlr_connect);
            }
            
        };

    // handler for @c tcp_acceptor
    auto io_state_chng_accept = [msg_hndlr_accept]
        (tcp_io_interface iof, std::size_t n, bool flag)
        {
            if (flag && n == 1) {
                iof.start_io("\n", msg_hndlr_accept);
            }
        };

    // error handler
    auto err_func = [] (tcp_io_interface iof, std::error_code err) 
        { 
            std::cerr << "err_func: " << err << " ";
            std::cerr << err.message() << std::endl; 
        };

    // work guard - handles @c std::thread and @c asio::io_context management
    chops::net::worker wk;
    wk.start();

    // create input (acceptor) and output (connector) tcp connections
    chops::net::net_ip chat(wk.get_io_context());
    // make @ tcp_acceptor, receive @c tcp_acceptor_network_entity
    auto tane = chat.make_tcp_acceptor(PORT, "127.0.0.1");
    assert(tane.is_valid());

    // make @c tcp_connector, receive @c tcp_connector_network_entity
    auto tcne = chat.make_tcp_connector(PORT, "127.0.0.1");
    assert(tcne.is_valid());

    // start @c tcp_acceptor network entity, emplace handlers
    auto ret_tcpa = tane.start(io_state_chng_accept, err_func);
    // check for failure
    assert(ret_tcpa);
    // start @c tcp_connector network entity, emplace handlers
    auto ret_tcpe = tcne.start(io_state_chng_connect, err_func);
    assert(ret_tcpe);

    // pause to let things settle down
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    std::cout << "network echo demo over local loop" << std::endl;
    std::cout << "enter a string at the prompt" << std::endl;
    std::cout << "the string will be returned in uppercase" << std::endl;
    std::cout << "enter \'quit\' to exit" << std::endl << std::endl;

    // get std::string from user
    // send string data over network connection
    std::string s;
    while (s != "quit\n") {
        std::cout << "> ";
        std::getline (std::cin, s);
        s += "\n"; // needed for deliminator
        // send string from @c tcp_connector to @c tcp_acceptor
        auto visit_out = [&s] (io_output io_out) { io_out.send(s.data(), s.size());};
        tcne.visit_io_output(visit_out);
        // pause so returned string is displayed before next prompt
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // must shutdown the net entities
    tcne.stop();
    tane.stop();
    wk.stop();

    return EXIT_SUCCESS;
}
