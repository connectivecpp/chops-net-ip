/** @file
 * 
 *  @ingroup example_module
 * 
 *  @brief Example of TCP send/recieve text string over network.
 * 
 *  @author Thurman Gillespy
 * 
 *  Copyright (c) 2019 Thurman Gillespy
 *  4/2/19
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
    // local variables
    io_context ioc;
    chops::wait_queue<std::string> queue_in;
    chops::wait_queue<std::string> queue_out;
    queue_out.push("Hello, world");

    /* lambda callbacks */
    // message handlers
    auto msg_hndlr_out = [&queue_out] (const_buf buf, io_interface iof, endpoint ep)
        {
            return true;
        };
    
    auto msg_hndlr_in = [&queue_in] (const_buf buf, io_interface iof, endpoint ep)
        {
            return true;
        };

    // io state change handlers
    auto io_state_chng_out = [n, flag, &queue_out, &msg_hndlr_out] 
        (io_interface iof, std::size_t n, bool flag)
        {
            iof.start_io("\n", msg_hndlr_out);
        };

    auto io_state_chng_in = [n, flag, &queue_in, &msg_hndlr_in]
        (io_interface iof, std::size_t n, bool flag)
        {

        };

    // error handler
    auto err_func = [] (io_interface iof, std::error_code err) 
        { std::cerr << "err_func: " << err << std::endl; };

    // work guard
    chops::net::worker wk;
    wk.start();

    // create input and output tcp connections
    chops::net::net_ip chat_out(ioc);
    auto tane = chat_out.make_tcp_connector("5000", "127.0.0.1");

    chops::net::net_ip chat_in(ioc);
    auto tcne = chat_in.make_tcp_acceptor("5000", "127.0.0.1");

    // start tcp_connector_network_entity
    std::size_t n = 1;
    bool flag = true;
    tane.start(io_state_chng_out, err_func);

    // start tcp_acceptor network entity
    tcne.start(io_state_chng_in, err_func);

    // process user input here
    
    wk.stop();

    return EXIT_SUCCESS;
}