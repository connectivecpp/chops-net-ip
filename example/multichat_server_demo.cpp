/** @file
 * 
 *  @ingroup example_module
 * 
 *  @brief Example of TCP multichat server network program.
 * 
 *  @author Thurman Gillespy
 * 
 *  Copyright (c) 2019 Thurman Gillespy
 *  4/12/19
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
multichat_server_demo.cpp -lpthread -o server
 *
 */

#include <iostream>
#include <cstdlib> // EXIT_SUCCESS
#include <cstddef> // std::size_t
#include <string>
#include <chrono>
#include <thread>
#include <cassert>

#include "net_ip/net_ip.hpp"
#include "net_ip/basic_net_entity.hpp"
#include "net_ip/component/worker.hpp"
#include "net_ip/component/send_to_all.hpp"
#include "simple_chat_screen.hpp"

using io_context = asio::io_context;
using io_interface = chops::net::tcp_io_interface;
using const_buf = asio::const_buffer;
using endpoint = asio::ip::tcp::endpoint;

// process command line args, set ip_addr, port, param as needed
bool process_args(int argc, char *argv[], std::string &ip_addr,
                  std::string &port, std::string &param)
{
    const std::string PORT = "5001";
    const std::string LOCAL_LOOP = "127.0.0.1";
    const std::string USAGE =
        "usage:\n"
        "  ./mchat [-h]  print usage info\n"
        "  ./mchat -accept [port]    multichat server\n"
        "  ./mchat -connect [IP address/hostname] [port]    multichat client\n";
    "    default port = " + PORT + "\n";
    const std::string HELP = "-h";
    const std::string EMPTY = "";

    // set default values
    ip_addr = LOCAL_LOOP;
    port = PORT;

    std::cout << USAGE << std::endl;

    return EXIT_SUCCESS;
}

struct io_handler_mock
{
    using socket_type = int;
    using endpoint_type = asio::ip::udp::endpoint;

    socket_type sock = 3;
    bool started = false;
    constexpr static std::size_t qs_base = 42;

    bool is_io_started() const { return started; }

    socket_type &get_socket() { return sock; }

    chops::net::output_queue_stats get_output_queue_stats() const
    {
        return chops::net::output_queue_stats{qs_base, qs_base + 1};
    }
    bool send_called = false;

    void send(chops::const_shared_buffer) { send_called = true; }
    void send(chops::const_shared_buffer, const endpoint_type &) { send_called = true; }

    bool mf_sio_called = false;
    bool delim_sio_called = false;
    bool rd_sio_called = false;
    bool rd_endp_sio_called = false;
    bool send_sio_called = false;
    bool send_endp_sio_called = false;

    template <typename MH, typename MF>
    bool start_io(std::size_t, MH &&, MF &&)
    {
        return started ? false : started = true, mf_sio_called = true, true;
    }

    template <typename MH>
    bool start_io(std::string_view, MH &&)
    {
        return started ? false : started = true, delim_sio_called = true, true;
    }

    template <typename MH>
    bool start_io(std::size_t, MH &&)
    {
        return started ? false : started = true, rd_sio_called = true, true;
    }

    template <typename MH>
    bool start_io(const endpoint_type &, std::size_t, MH &&)
    {
        return started ? false : started = true, rd_endp_sio_called = true, true;
    }

    bool start_io()
    {
        return started ? false : started = true, send_sio_called = true, true;
    }

    bool start_io(const endpoint_type &)
    {
        return started ? false : started = true, send_endp_sio_called = true, true;
    }

    bool stop_io()
    {
        return started ? started = false, true : false;
    }
};

int main(int argc, char *argv[])
{
    const std::string LOCAL = "[local]  ";
    const std::string SYSTEM = "[system] ";
    const std::string SERVER = "[server] ";
    const std::string DELIM = "\a"; // alert (bell)
    const std::string NO_CONNECTION = "error: no connection" + DELIM;
    const std::string ABORT = "abort: too many errors";
    const std::string WAIT_CONNECT = "waiting for connection..." + DELIM;
    std::string ip_addr;
    std::string port;
    std::string param;

    // work guard - handles @c std::thread and @c asio::io_context management
    chops::net::worker wk;
    wk.start();

    chops::net::send_to_all<io_handler_mock> sta;

    if (process_args(argc, argv, ip_addr, port, param) == EXIT_FAILURE)
    {
        return EXIT_FAILURE;
    }

    /* lamda handlers */
    // receive text from client, send out to others
    auto msg_hndlr = [&](const_buf buf, io_interface iof, endpoint ep) {

    };

    auto io_state_chng_hndlr = [&](io_interface iof, std::size_t n, bool flag) {
        // call start_io, add to or remove from list
        iof.start_io(DELIM, msg_hndlr);
        sta.add_io_interface(iof);
    };

    auto err_func = [&](io_interface iof, std::error_code err) {

    };


    // create @c net_ip instance
    chops::net::net_ip server(wk.get_io_context());
    // make @c tcp_acceptor, ruitn @c network_entity
    auto net_entity = server.make_tcp_acceptor(port.c_str(), ip_addr.c_str());
    // start network entity, emplace handlers
    net_entity.start(io_state_chng_hndlr, err_func);

    while (!finished) {

    }
    
    return EXIT_SUCCESS;
}