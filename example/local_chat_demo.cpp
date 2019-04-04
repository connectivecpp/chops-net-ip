/** @file
 * 
 *  @ingroup example_module
 * 
 *  @brief Example of TCP send/recieve text string over local loop network.
 * 
 *  @author Thurman Gillespy
 * 
 *  Copyright (c) 2019 Thurman Gillespy
 *  4/3/19
 * 
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
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
#include "net_ip/basic_net_entity.hpp"
#include "component/worker.hpp"
#include "queue/wait_queue.hpp"
#include <experimental/io_context>
#include <experimental/net>

using io_context = asio::io_context;
using io_interface = chops::net::tcp_io_interface;
using const_buf = asio::const_buffer;
using tcp_io_interface = chops::net::tcp_io_interface;
using endpoint = asio::ip::tcp::endpoint;



int main() {
    
    /* lambda callbacks */
    // message handlers
    auto msg_hndlr_connect = [] (const_buf buf, io_interface iof, endpoint ep)
        {
            // receive data from acceptor, display to user
            std::string s (static_cast<const char*> (buf.data()), buf.size());
            std::cout << s; // s aleady has terminating '\n'

            return true;
        };
    
    // recceive data from connector, send back to acceptor
    auto msg_hndlr_accept = [] (const_buf buf, io_interface iof, endpoint ep)
        {
            // copy buffer contents into string, convert to uppercase
            std::string s(static_cast<const char*> (buf.data()), buf.size());
            auto to_upper = [] (char& c) { c = ::toupper(c); };
            std::for_each(s.begin(), s.end(), to_upper);
            // send c-string back over network connection
            iof.send(s.c_str(), s.size() + 1);

            return true;
        };

    // io state change handlers
    tcp_io_interface tcp_connect_iof;
    auto io_state_chng_connect = [&tcp_connect_iof, msg_hndlr_connect] 
        (io_interface iof, std::size_t n, bool flag)
        {
            iof.start_io("\n", msg_hndlr_connect);
            // return iof to main
            tcp_connect_iof = iof;
        };

    auto io_state_chng_accept = [msg_hndlr_accept]
        (io_interface iof, std::size_t n, bool flag)
        {
            iof.start_io("\n", msg_hndlr_accept);
        };

    // error handler
    auto err_func = [] (io_interface iof, std::error_code err) 
        { std::cerr << "err_func: " << err << std::endl; };

    // work guard
    chops::net::worker wk;
    wk.start();

    // create input (acceptor) and output (connector) tcp connections
    chops::net::net_ip chat(wk.get_io_context());
    auto tane = chat.make_tcp_acceptor("12370", "127.0.0.1");
    assert(tane.is_valid());

    auto tcne = chat.make_tcp_connector("12370", "127.0.0.1");
    assert(tcne.is_valid());

    // start tcp_acceptor network entity
    tane.start(io_state_chng_accept, err_func);
    // start tcp_connector network entity
    tcne.start(io_state_chng_connect, err_func);

    // pause to let things settle down
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    assert(tcp_connect_iof.is_valid()); // fails without a pause

    std::cout << "network demo over local loop" << std::endl;
    std::cout << "enter a string at the prompt" << std::endl;
    std::cout << "the string will be returned in uppercase" << std::endl;
    std::cout << "enter \'quit\' to exit" << std::endl << std::endl;

    // get std::string from user
    // send as c-string over network connection
    std::string s;
    while (s != "quit\n") {
        std::cout << "> ";
        std::getline (std::cin, s);
        s += "\n"; // needed for deliminator
        tcp_connect_iof.send(s.c_str(), s.size() + 1);
        // pause so returned string displayed before next prompt
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    wk.stop();

    return EXIT_SUCCESS;
}