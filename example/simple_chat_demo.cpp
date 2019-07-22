/** @file
 * 
 *  @ingroup example_module
 * 
 *  @brief Example of TCP peer to peer network chat program.
 * 
 *  @author Thurman Gillespy
 * 
 *  Copyright (c) 2019 Thurman Gillespy
 *  4/30/19
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
simple_chat_demo.cpp -lpthread -o chat
 * 
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
#include "net_ip/net_entity.hpp"
#include "net_ip_component/worker.hpp"
#include "simple_chat_screen.hpp"
#include "queue/wait_queue.hpp"
#include "net_ip/io_type_decls.hpp"

using io_context = asio::io_context;
using io_interface = chops::net::tcp_io_interface;
using const_buf = asio::const_buffer;
using tcp_io_interface = chops::net::tcp_io_interface;
using endpoint = asio::ip::tcp::endpoint;

/** How to use @c chops-net-ip for a simple peer to peer chat program over
 *  a network connection
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
 *  The message handler must be shut down to preven memory leaks.
 * 
 *  2. Write @ io_state_change handler.
 * 
 *  The @c io_state_change handler calls @c ::start_io on the provided
 *  @c chops::net::tcp_io_interface. The handler needs to return a copy
 *  of the @c io_interface to main. The main copy will use the @c io_interface
 *  to @c send the text message from one client to another. On program
 *  shutdown, @c ::stop_io must be called on the @c io_interface to prevent
 *  memory leaks.
 * 
 *  3. Create an instance of the @c chops::net::worker class, which provides
 *  @c std::thread and @c asio::io_context management. Call @c worker::start 
 *  before the frist call to the @chops::net::ip facilities, then call 
 *  @c worker::stop when finished.
 * 
 *  4. Create an instance of the @c chops::net::ip::net_ip class. The constructor
 *  needs an @c asio:io_context, which is provided by the @c ::get_io_context
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
 *  7. Call @c ::start on the @c network_entity, which emplaces the
 *  message handler and @c io_state change handler.
 * 
 *  8. Call @c ::send on the @c chops::net::tcp_io_interface instance to send
 *  a text string over the network connection.
 * 
 *  9. See the example code and the header files for the signatures of the
 *  handlers.
 *  
 *  10. Program shutdown
 *  
 *  To prevent memory leaks and crashes, the following steps must be taken.
 * 
 *  Shutdown the @c message_handler and call @c ::stop_io on the 
 *  @c io_interface. This program the @c 'quit' command and the
 *  @c finished flag to shutdown the handlers.
 * 
 *  Call @c ::stop on the @c network_entity.
 *  
 *  Comment: This sequence is too convoluted, and a more gracefull 
 *  automatic shutdown will be implemented for release 1.0.
 * 
 */


// process command line args, set ip_addr, port, param, print_errors
bool process_args(int argc, char* const argv[], std::string& ip_addr, 
        std::string& port, std::string& param, bool& print_errors) {
    const std::string PORT = "5001";
    const std::string LOCAL_LOOP = "127.0.0.1";
    const std::string USAGE =
        "usage: ./chat [-h] [-e] -connect | -accept [ip address] [port]\n"
        "  -h  print usage info\n"
        "  -e  print error and diagnostic messages\n"
        "  -connect  tcp_connector\n"
        "  -accept   tcp_acceptor\n"
        "  default ip address: " + LOCAL_LOOP + " (local loop)\n"
        "  default port: " + PORT + "\n"
        "  if connection type = accept, IP address becomes \"\"";
    const std::string HELP_PRM = "-h";
    const std::string PRINT_ERRS = "-e";
    
    int offset = 0; // 1 when -e 1st parameter
    
    // set default values
    ip_addr = LOCAL_LOOP;
    port = PORT;

    if (argc < 2 || argc > 5) {
        std::cout << "incorrect parameter count\n";
        std::cout << USAGE << std::endl;
        return EXIT_FAILURE;
    };

    if (argv[1] == HELP_PRM) {
        std::cout << USAGE << std::endl;
        return EXIT_FAILURE;
    } else if (argv[1] == PRINT_ERRS) {
        print_errors = true;
        offset = 1;
    }
    
    if (argv[1 + offset] == PARAM_CONNECT) {
        param = PARAM_CONNECT;
    } else if (argv[1 + offset] == PARAM_ACCEPT) {
        param = PARAM_ACCEPT;
    } else {
        std::cout << "incorrect first parameter: ";
        std::cout << "must be [-h | -e | -connect | -accept]" << std::endl;
        std::cout << USAGE << std::endl;
        return EXIT_FAILURE;
    }

    if (param == PARAM_ACCEPT) {
        ip_addr = "";
    }

    if (argc == 3 + offset || argc == 4 + offset) {
        if (param == PARAM_CONNECT) {
            ip_addr = argv[2 + offset];
        }
    }

    if (argc == 4 + offset) {
        port = argv[3 + offset];
    }

    assert(param != "");

    return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {
    const std::string LOCAL = "[local]  ";
    const std::string SYSTEM = "[system] ";
    const std::string ERROR_MSG = "[error]  ";
    const std::string DELIM = "\a"; // alert (bell)
    const std::string NO_CONNECTION = "no connection..." + DELIM;
    const std::string ABORT = "abort: too many errors";
    const std::string WAIT_CONNECT = "waiting for connection..." + DELIM;
    std::string ip_addr;
    std::string port;
    std::string param;
    bool print_errors = false;
    bool shutdown = false;

    if (process_args(argc, argv, ip_addr, port, param, print_errors) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }
    
    // create instance of @c simple_chat_screen class
    simple_chat_screen screen(ip_addr, port, param, print_errors);

    /* lambda callbacks */
    // message handler for @c network_entity
    // note: cannot explicitly capture REMOTE
    auto msg_hndlr = [&] (const_buf buf, io_interface iof, endpoint ep)
        {
            if (shutdown) {
                if (print_errors) {
                    screen.insert_scroll_line("msg_hndlr shutdown" + DELIM,
                        SYSTEM);
                    screen.draw_screen();
                }
                // must return false on shutdown
                return false; 
            }

            // receive network text message, display to user
            // remove deliminator here?
            std::string s(static_cast<const char *>(buf.data()), buf.size());

            screen.insert_scroll_line(s, REMOTE);
            screen.draw_screen();

            // if user sent quit, echo back to allow her io_interface to stop
            if (s == "quit" + DELIM) {
                iof.send(s.data(), s.size());
            }

            return true;
        };

    io_interface tcp_iof; // use this to send text messages

    // handler for @c tcp_connector
    auto io_state_chng_hndlr = [&] (io_interface iof, std::size_t n, bool flag) {
        // start iof on flag, and only allow one connection
        if (flag) {
            if (n == 1) {
                if (print_errors) {
                    screen.insert_scroll_line("io_interface start" + DELIM, SYSTEM);
                    screen.draw_screen();
                }
                iof.start_io(DELIM, msg_hndlr);
                tcp_iof = iof; // return @c iof to main
            } else {
                // since we are peer to peer, reject >1 connections
                screen.insert_scroll_line("2nd tcp_connector client rejected" + 
                            DELIM, SYSTEM);
                screen.draw_screen();
                iof.start_io(DELIM, msg_hndlr);
                const std::string err = "only one tcp connection allowed";
                iof.send(err.data(), err.size());
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                iof.stop_io();
            }
        } else { // flag false
            // stop iof when flag false
            if (print_errors) {
                screen.insert_scroll_line("io_interface stop" + DELIM, SYSTEM);
                screen.draw_screen();
            }
            // IMPORTANT: needed to avert memory leaks and crashes
            iof.stop_io();
        }
    };

    // error handler
    auto err_func = [&] (io_interface iof, std::error_code err) 
        { 
            static int count = 0;
            // shudtown if too many error messages
            if (++count > 15) {
                std::cerr << ABORT << std::endl;
                exit(1);
            }

            if (err.value() == 111 || err.value() == 113) {
                screen.insert_scroll_line(WAIT_CONNECT, SYSTEM);
                screen.draw_screen();
            }

            if (print_errors) {
                std::string err_text = err.category().name();
                err_text += ": " + std::to_string(err.value()) + ", " +
                err.message() + DELIM;
                screen.insert_scroll_line(err_text, ERROR_MSG);
                screen.draw_screen();
            }
        };

    // work guard - handles @c std::thread and @c asio::io_context management
    chops::net::worker wk;
    wk.start();
    
    // create @c net_ip instance
    chops::net::net_ip chat(wk.get_io_context());
    

    chops::net::tcp_connector_net_entity net_entity_connect;
    chops::net::tcp_acceptor_net_entity net_entity_accept;

    if (param == PARAM_CONNECT) {
        // make @c tcp_connector, return @c network_entity
        // TODO: why does this not work with std::string, but acceptor does?
        net_entity_connect = chat.make_tcp_connector(port.c_str(), ip_addr.c_str(),
                    std::chrono::milliseconds(5000));
        assert(net_entity_connect.is_valid());
        // start network entity, emplace handlers
        net_entity_connect.start(io_state_chng_hndlr, err_func);
    } else {
        // make @ tcp_acceptor, return @c network_entity
        net_entity_accept = chat.make_tcp_acceptor(port.c_str(), ip_addr.c_str());
        assert(net_entity_accept.is_valid());
        // start network entity, emplace handlers
        net_entity_accept.start(io_state_chng_hndlr, err_func);
    }

    screen.draw_screen();
        
    // get std::string from user, send string data over network, update screen
    std::string s;
    while (!shutdown) {
        std::getline (std::cin, s); // user input
        if (s == "quit") {
            shutdown = true;
        }
        s += DELIM; // needed for deliminator
        screen.insert_scroll_line(s, LOCAL);
        screen.draw_screen();
        // tcp.iof is not valid when there is no network connection
        if (!tcp_iof.is_valid()) {
            screen.insert_scroll_line(NO_CONNECTION, SYSTEM);
            screen.draw_screen();
            continue; // back to top of loop
        }
        // send user text message over network
        // note correct method of sending string data over network
        tcp_iof.send(s.data(), s.size());
    }

    // allow last message to be sent before shutting down connection
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // shutdown
    if (param == PARAM_CONNECT) {
        net_entity_connect.stop();
    } else {
        net_entity_accept.stop();
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    wk.stop();

    return EXIT_SUCCESS;
}