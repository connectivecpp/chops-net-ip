/** @file
 * 
 *  @ingroup example_module
 * 
 *  @brief Example of TCP 2-way network chat program.
 * 
 *  @author Thurman Gillespy
 * 
 *  Copyright (c) 2019 Thurman Gillespy
 *  4/4/19
 * 
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *  
 *  Sample make file:
 *  g++ -std=c++17 -Wall -Werror \
 *  -I ../include -I ../include/net_ip/ -I ~/Projects/utility-rack/include/ \
 *  -I ~/Projects/asio/asio/include \
 *  simple_chat_deo.cpp -lpthread
 * 
 *  BUGS:
 *   - leaks memory like a sieve. Under investigation.
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

using io_context = asio::io_context;
using io_interface = chops::net::tcp_io_interface;
using const_buf = asio::const_buffer;
using tcp_io_interface = chops::net::tcp_io_interface;
using endpoint = asio::ip::tcp::endpoint;

/** How to use @c chops-net-ip for a simple text send/receive network connection
 *  over a local loop.
 * 
 *  1. Write message handlers for the @c tcp_connector and @c tcp_acceptor.
 * 
 *  The @c tcp_connector_message_handler receives text from a @c tcp_acceptor
 *  (in-bound messages). In this demo, the handler prints the text to stdout,
 * 
 *  The @c tcp_acceptor_message_handler receives the (out-bound) message from
 *  the @c tcp-connector. In this demo, the handler converts the text to
 *  uppercase, then returns the text over the same connection back to the
 *  @c tcp_connector.
 * 
 *  In this demo, both message handlers return @c false when the user enters
 *  @c "quit", otherwise @c true. @c "quit" shuts down both handlers and exits
 *  the program.
 * 
 *  2. Write @ io_state_change handlers for the @c tcp_connector and @c
 *  @c tcp_acceptor.
 * 
 *  The @c tcp_connector @c io_state_change handler calls @c start_io on the
 *  provided @c chops::net::tcp_io_interface. It then needs to return a copy
 *  of the @c io_interface to main. The main copy will use the @c io_interface
 *  to @c send the text message from the @c tcp_connector to the @c tcp_acceptor.
 *  
 *  The @c tcp_acceptor @c io_state_change handler only needs to call @c start_io
 *  on the provided @c io_interface.
 * 
 *  3. Create an instance of the @c chops::net::worker class, which provides
 *  @c std::thread and @c asio::io_context management. Call @c worker::start 
 *  before the frist call to the @chops::net::ip facilities, then call 
 *  @c worker::stop when finished.
 * 
 *  4. Create an instance of the @c chops::net::ip::net_ip class. The constructor
 *  needs an @c asio:io_context, which is provided by the @c get_io_context()
 *  method of the @c chops::net::worker instance.
 *  
 *  5. Call @c ::make_tcp_connector on the @ net_ip instance, which returns a 
 *  copy of a @c tcp_connecvtor_network_entity.
 *  
 *  6. Call @c ::make_tcp_acceptor on the @ net_ip instance, which returns a
 *  copy of a @c tcp_acceptor_network_entity.
 * 
 *  7. Call @c ::start() on both both @c network_entity. Each @c tcp_connector
 *  and @c tcp_acceptor @ network_entity takes its' own @c io_state_change
 *  handler, and each @ io_state_change handler takes its' own @c message_handler. 
 * 
 *  8. Call @c ::send() on the @c chops::net::tcp_io_interface instance to send
 *  a text string over the local loop network connection.
 * 
 *  9. See the example code and the header files for the signatures of the
 *  handlers.
 * 
 */

const std::string PORT = "5001";
const std::string LOCAL_LOOP = "127.0.0.1";
const std::string usage = "usage: ./chat [-h | -connect | -accept ] [ip address] [port]\n"\
    "  default ip address: " + LOCAL_LOOP + " (local loop)\n" \
    "  default port: " + PORT;
const std::string help = "-h";
const std::string param_connect = "-connect";
const std::string param_accept = "-accept";

int main(int argc, char* argv[]) {
    const char* ip_addr = LOCAL_LOOP.c_str();
    const char* port = PORT.c_str();
    std::string param;

    //std::cout << "argc: " << argc << std::endl;

    if (argc < 2 || argc > 4) {
        std::cout << "incorrect parameter count\n";
        std::cout << usage << std::endl;
        return EXIT_FAILURE;
    };

    if (argv[1] == help) {
        std::cout << usage << std::endl;
        return EXIT_SUCCESS;
    } else if (param_connect == argv[1]) {
        param = param_connect;
    } else if (argv[1] == param_accept) {
        param = param_accept;
    } else {
        std::cout << "incorrect first parameter: must be [-h | -connect | -accept]" << std::endl;
        std::cout << usage << std::endl;
        return EXIT_FAILURE;
    }

    if (argc == 3) {
        ip_addr = argv[2];
    } else if (argc == 4) {
        ip_addr = argv[2];
        port = argv[3];
    }

    std::cout << "2-way chat demo" << std::endl;
    std::cout << "IP address: " << ip_addr << "; port: " << port << std::endl;
    std::cout << "connection type: " << param << std::endl;
    std::cout << "enter -h for usage" << std::endl;
    std::cout << "enter text message at the prompt" << std::endl;
    std::cout << "enter \'quit\' to exit" << std::endl << std::endl;


    /* lambda callbacks */
    // message handler for @c tcp_connector @c network_entity
    auto msg_hndlr_connect = [] (const_buf buf, io_interface iof, endpoint ep)
        {
            // receive data from acceptor, display to user
            std::string s (static_cast<const char*> (buf.data()), buf.size());
            std::cout << "> " << s; // s aleady has terminating '\n'

            // return false if user entered 'quit', otherwise true
            return s == "QUIT\n" ? false : true;
        };

    // io state change handlers
    tcp_io_interface tcp_connect_iof; // used to send text data
    // handler for @c tcp_connector
    auto io_state_chng_connect = [&tcp_connect_iof, msg_hndlr_connect] 
        (io_interface iof, std::size_t n, bool flag)
        {
            iof.start_io("\n", msg_hndlr_connect);
            // return iof to main
            tcp_connect_iof = iof;
        };

    // // handler for @c tcp_acceptor
    // auto io_state_chng_accept = [msg_hndlr_accept]
    //     (io_interface iof, std::size_t n, bool flag)
    //     {
    //         iof.start_io("\n", msg_hndlr_accept);
    //     };

    // error handler
    auto err_func = [] (io_interface iof, std::error_code err) 
        { 
            static int count = 0;
            std::cerr << "err_func: " << err << std::endl;
            if (++count > 10) {
                std::cerr << "aborting: > 10 error messages" << std::endl;
                exit(0);
            } };

    // work guard - handles @c std::thread and @c asio::io_context management
    chops::net::worker wk;
    wk.start();

    assert(param == param_connect || param == param_accept);

    // create @c net_ip instance
    chops::net::net_ip chat(wk.get_io_context());
    if (param == param_connect) {
        // make @c tcp_connector, receive @c tcp_connector_network_entity
        auto net_entity = chat.make_tcp_connector(port, ip_addr);
        assert(net_entity.is_valid());
    } else if (param == param_accept){
        // make @ tcp_acceptor, receive @c tcp_acceptor_network_entity
        auto net_entity = chat.make_tcp_acceptor(port, ip_addr);
        assert(net_entity.is_valid());
    }
    
    // start network entity, emplace handlers
    net_entity.start(io_state_chng_connect, err_func);

    // pause to let both sides connect
    std::this_thread::sleep_for(std::chrono::milliseconds(4000));

    assert(tcp_connect_iof.is_valid()); // fails without a pause

    // get std::string from user
    // send as c-string over network connection
    std::string s;
    while (s != "quit\n") {
        std::cout << "> ";
        std::getline (std::cin, s);
        s += "\n"; // needed for deliminator
        // send c-string from @c tcp_connector to @c tcp_acceptor
        tcp_connect_iof.send(s.c_str(), s.size() + 1);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    wk.stop();

    return EXIT_SUCCESS;
}