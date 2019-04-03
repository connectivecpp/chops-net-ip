/** @file
 * 
 *  @ingroup example_module
 * 
 *  @brief Example of TCP send/recieve text string over network.
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
    auto msg_hndlr_out = [] (const_buf buf, io_interface iof, endpoint ep)
        {
            // only data back from acceptor
            // display to user
            std::string s (static_cast<const char*> (buf.data()), buf.size());
            std::cout << s; // s aleady has terminating '\n'

            return true;
        };
    
    auto msg_hndlr_in = [] (const_buf buf, io_interface iof, endpoint ep)
        {
            std::string s(static_cast<const char*> (buf.data()), buf.size());
            //std::for_each(s.begin(), s.end(), std::mem_fun(&std::toupper));
            for (auto &ch : s) {
                ch = toupper(ch);
            }
            iof.send(s.c_str(), s.size() + 1);

            return true;
        };

    // io state change handlers
    tcp_io_interface tcp_iof;
    auto io_state_chng_out = [&tcp_iof, msg_hndlr_out] 
        (io_interface iof, std::size_t n, bool flag)
        {
            iof.start_io("\n", msg_hndlr_out);
            // send iof to main
            tcp_iof = iof;
        };

    auto io_state_chng_in = [msg_hndlr_in]
        (io_interface iof, std::size_t n, bool flag)
        {
            iof.start_io("\n", msg_hndlr_in);
        };

    // error handler
    auto err_func = [] (io_interface iof, std::error_code err) 
        { std::cerr << "err_func: " << err << std::endl; };

    // work guard
    chops::net::worker wk;
    wk.start();

    // create input and output tcp connections
    chops::net::net_ip chat_in(wk.get_io_context());
    auto tane = chat_in.make_tcp_acceptor("12370", "127.0.0.1");
    assert(tane.is_valid());

    auto tcne = chat_in.make_tcp_connector("12370", "127.0.0.1");
    assert(tcne.is_valid());

    // start tcp_acceptor network entity
    tane.start(io_state_chng_in, err_func);
    // start tcp_connector_network_entity
    tcne.start(io_state_chng_out, err_func);

    // pause to let things settle down
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    assert(tcp_iof.is_valid()); // fails without a pause

    std::cout << "network demo over local loop" << std::endl;
    std::cout << "enter a string at the prompt" << std::endl;
    std::cout << "the string will be returned in uppercase" << std::endl;
    std::cout << "enter \'quit\' to exit" << std::endl << std::endl;

    // get string from user
    // send as c-string over network connection
    std::string s;
    while (s != "quit\n") {
        std::cout << "> ";
        std::getline (std::cin, s);
        s += "\n"; // needed for deliminator
        tcp_iof.send(s.c_str(), s.size() + 1);
        // pause so returned string displayed before next prompt
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    wk.stop();

    return EXIT_SUCCESS;
}