/** @file
 * 
 *  @ingroup example_module
 * 
 *  @brief Example of TCP send/receive text string over local loop network
 *  connection.
 * 
 *  @author Thurman Gillespy
 * 
 *  Copyright (c) 2019 Thurman Gillespy
 *  4/11/19
 * 
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *  
 *  Sample make file :
g++ -std=c++17 -Wall -Werror \
-I ../include \
-I ~/Projects/utility-rack/include/ \
-I ~/Projects/asio/asio/include \
-I ~/Projects/boost_1_69_0/ \
 local_echo_demo.cpp -lpthread -o local
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
#include "net_ip/component/worker.hpp"
#include "queue/wait_queue.hpp"

using io_context = asio::io_context;
using io_interface = chops::net::tcp_io_interface;
using const_buf = asio::const_buffer;
using tcp_io_interface = chops::net::tcp_io_interface;
using endpoint = asio::ip::tcp::endpoint;

/** How to use @c chops-net-ip for a simple text send/receive network connection
 *  over a local loop that echos the text (in uppercase) back to the user.
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

int main() {
    
    /* lambda callbacks */
    // message handler for @c tcp_connector @c network_entity
    auto msg_hndlr_connect = [] (const_buf buf, io_interface iof, endpoint ep)
        {
            // receive data from acceptor, display to user
            std::string s (static_cast<const char*> (buf.data()), buf.size());
            std::cout << s; // s aleady has terminating '\n'

            // return false if user entered 'quit', otherwise true
            return s == "QUIT\n" ? false: true;
        };
    
    // message handler for @c tcp_acceptor @c message_handler
    // receive data from connector, send back to acceptor
    auto msg_hndlr_accept = [] (const_buf buf, io_interface iof, endpoint ep)
        {
            // copy buffer contents into string, convert to uppercase
            std::string s(static_cast<const char*> (buf.data()), buf.size());
            auto to_upper = [] (char& c) { c = ::toupper(c); };
            std::for_each(s.begin(), s.end(), to_upper);
            // send uppercase string data back over network connection
            iof.send(s.data(), s.size());
            
            // return false if user entered 'quit', otherwise true
            return s == "QUIT\n" ? false:  true;
        };

    // io state change handlers
    tcp_io_interface tcp_connect_iof; // used to send text data
    // handler for @c tcp_connector
    auto io_state_chng_connect = [&tcp_connect_iof, msg_hndlr_connect] 
        (io_interface iof, std::size_t n, bool flag) {
            if (flag && n == 1) {
                iof.start_io("\n", msg_hndlr_connect);
                // return iof to main
                tcp_connect_iof = iof;
            }
            
        };

    // handler for @c tcp_acceptor
    auto io_state_chng_accept = [msg_hndlr_accept]
        (io_interface iof, std::size_t n, bool flag)
        {
            if (flag && n == 1) {
                iof.start_io("\n", msg_hndlr_accept);
            }
        };

    // error handler
    auto err_func = [] (io_interface iof, std::error_code err) 
        { 
            // std::cerr << "err_func: " << err << " ";
            // std::cerr << err.message() << std::endl; 
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
    tane.start(io_state_chng_accept, err_func);
    // start @c tcp_connector network entity, emplace handlers
    tcne.start(io_state_chng_connect, err_func);

    // pause to let things settle down
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    assert(tcp_connect_iof.is_valid()); // fails without a pause

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
        tcp_connect_iof.send(s.data(), s.size());
        // pause so returned string is displayed before next prompt
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    wk.stop();

    return EXIT_SUCCESS;
}
