/** @file
 * 
 *  @ingroup example_module
 * 
 *  @brief Example of TCP 2-way network chat program.
 * 
 *  @author Thurman Gillespy
 * 
 *  Copyright (c) 2019 Thurman Gillespy
 *  4/9/19
 * 
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *  
 *  Sample make file:
 *  g++ -std=c++17 -Wall -Werror \
 *  -I ../include -I ../include/net_ip/ \
 *  -I ~/Projects/utility-rack/include/ \
 *  -I ~/Projects/asio-1.12.2/include \
 *  -I ~/Projects/boost_1_69_0/ \
 *  simple_chat_deo.cpp -lpthread -o chat
 * 
 *  BUGS:
 *   - leaks memory like a sieve. Under investigation.
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
#include "component/worker.hpp"
#include "simple_chat_screen.hpp"

using io_context = asio::io_context;
using io_interface = chops::net::tcp_io_interface;
using const_buf = asio::const_buffer;
using tcp_io_interface = chops::net::tcp_io_interface;
using endpoint = asio::ip::tcp::endpoint;

/** How to use @c chops-net-ip for a simple 2-way chat program over a
 *  network connection
 * 
 *  1. Write message handler function.
 * 
 *  The message handler receives text after @c ::send is invoked. Both
 *  the local and remote remote clients obtain the sent text through the
 *  handler. In this demo, the handler inserts the recevied text into the
 *  history scroll text, and then updates the screen output.
 * 
 *  Both message handlers return @c false when the user enters @c 'quit',
 *  otherwise @c true. @c 'quit' shuts down the handler and exits the program.
 * 
 *  2. Write @ io_state_change handler.
 * 
 *  The @c io_state_change handler calls @c start_io on the provided
 *  @c chops::net::tcp_io_interface. The handler needs to return a copy
 *  of the @c io_interface to main. The main copy will use the @c io_interface
 *  to @c send the text message from one client to another.
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
 *  5. Call @c ::make_tcp_connector on the @c tcp_connector @ net_ip instance
 *  (@c '-connect' connection type), which returns a copy of a
 *  @c tcp_connector_network_entity.
 *  
 *  6. Call @c ::make_tcp_acceptor on the @c tcp_acceptor @ net_ip instance
 *  (@c '-accept' connection type), which returns a copy of a
 *  @c tcp_acceptor_network_entity.
 * 
 *  7. Call @c ::start() on both both instances of @c network_entity, which
 *  emplaces the message handler and @c io_state change handlers.
 * 
 *  8. Call @c ::send() on the @c chops::net::tcp_io_interface instance to send
 *  a text string over the network connection.
 * 
 *  9. See the example code and the header files for the signatures of the
 *  handlers.
 * 
 */


// process command line args, set ip_addr, port, param as needed
bool process_args(int argc, char* argv[], std::string& ip_addr, 
        std::string& port, std::string& param) {
    const std::string PORT = "5001";
    const std::string LOCAL_LOOP = "127.0.0.1";
    const std::string USAGE =
        "usage: ./chat [-h | -connect | -accept ] [ip address] [port]\n"
        "  default ip address: " + LOCAL_LOOP + " (local loop)\n"
        "  default port: " + PORT + "\n"
        "  if connection type = accept, IP address becomes \"\"";
    const std::string HELP = "-h";
    const std::string EMPTY = "";

    // set default values
    ip_addr = LOCAL_LOOP;
    port = PORT;

    if (argc < 2 || argc > 4) {
        std::cout << "incorrect parameter count\n";
        std::cout << USAGE << std::endl;
        return EXIT_FAILURE;
    };

    if (argv[1] == HELP) {
        std::cout << USAGE << std::endl;
        return EXIT_FAILURE;
    } else if (argv[1] == PARAM_CONNECT) {
        param = PARAM_CONNECT;
    } else if (argv[1] == PARAM_ACCEPT) {
        param = PARAM_ACCEPT;
    } else {
        std::cout << "incorrect first parameter: ";
        std::cout << "must be [-h | -connect | -accept]" << std::endl;
        std::cout << USAGE << std::endl;
        return EXIT_FAILURE;
    }

    if (param == PARAM_ACCEPT) {
        ip_addr = "";
    }

    if (argc == 3 || argc == 4) {
        if (param == PARAM_CONNECT) {
            ip_addr = argv[2];
        }
    }

    if (argc == 4) {
        port = argv[3];
    }

    assert(param != "");

    return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {
    const std::string LOCAL = "[local]  ";
    std::string ip_addr;
    std::string port;
    std::string param;

    if (process_args(argc, argv, ip_addr, port, param) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }
    
    // create instance of @c simple_chat_screen class
    simple_chat_screen screen(ip_addr, port, param);

    /* lambda callbacks */
    // message handler for @c network_entity
    auto msg_hndlr = [&] (const_buf buf, io_interface iof, endpoint ep)
        {
            // receive data from acceptor, display to user
            std::string s (static_cast<const char*> (buf.data()), buf.size());
            // BUG: remove errant '\0' at front of string, if present
            if (s.at(0) == '\0') {
                s = s.substr(1, std::string::npos);
            }
            screen.insert_scroll_line(s, REMOTE);
            screen.draw_screen();

            // return false if user entered 'quit', otherwise true
            return s == "quit\n" ? false : true;
        };

    // io state change handler
    tcp_io_interface tcp_iof; // used to send text data
    // handler for @c tcp_connector
    auto io_state_chng_hndlr = [&tcp_iof, msg_hndlr] 
        (io_interface iof, std::size_t n, bool flag)
        {
            iof.start_io("\n", msg_hndlr);
            // return iof to main
            tcp_iof = iof;
        };

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

    // create @c net_ip instance
    chops::net::net_ip chat(wk.get_io_context());

    if (param == PARAM_CONNECT) {
        // make @c tcp_connector, return @c network_entity
        auto net_entity = chat.make_tcp_connector(port.c_str(), ip_addr.c_str(),
                    std::chrono::milliseconds(5000));
        assert(net_entity.is_valid());
        // start network entity, emplace handlers
        net_entity.start(io_state_chng_hndlr, err_func);
    } else {
        // make @ tcp_acceptor, return @c network_entity
        auto net_entity = chat.make_tcp_acceptor(port.c_str(), ip_addr.c_str());
        assert(net_entity.is_valid());
        // start network entity, emplace handlers
        net_entity.start(io_state_chng_hndlr, err_func);
    }

    screen.draw_screen();

    // get std::string from user
    // send as c-string over network connection, update screen
    std::string s;
    while (s != "quit\n") {
        std::getline (std::cin, s);
        s += "\n"; // needed for deliminator
        screen.insert_scroll_line(s, LOCAL);
        screen.draw_screen();
        // send c-string from @c tcp_connector to @c tcp_acceptor
        assert(tcp_iof.is_valid());
        tcp_iof.send(s.c_str(), s.size() + 1);
    }
    
    wk.stop();

    return EXIT_SUCCESS;
}