/** @file
 * 
 *  @ingroup example_module
 * 
 *  @brief Example of 2-way TCP peer to peer network chat program.
 *  See usage text for further information.
 * 
 *  @author Thurman Gillespy
 * 
 *  Copyright (c) 2019 Thurman Gillespy
 *  @date 2019-10-14
 * 
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *  
 *  Sample compile script:
g++ -std=c++17  -g -Wall -Werror \
-I ../include \
-I ../../utility-rack/include/ \
-I ../../asio/asio/include/ \
-I ../../expected-lite/include/ \
simple_chat_demo.cpp -lpthread -o chat 
 *
 */ 

#include <iostream>
#include <cstdlib> // EXIT_SUCCESS
#include <cstddef> // std::size_t 
#include <string>
#include <thread>
#include <cassert>

#include "net_ip/net_ip.hpp"
#include "net_ip/net_entity.hpp"
#include "net_ip_component/worker.hpp"
#include "simple_chat_screen.hpp"
#include "queue/wait_queue.hpp"
#include "net_ip/io_type_decls.hpp"

using io_context = asio::io_context;
using io_output = chops::net::tcp_io_output;
using io_interface = chops::net::tcp_io_interface;
using const_buf = asio::const_buffer;
using tcp_io_interface = chops::net::tcp_io_interface;
using endpoint = asio::ip::tcp::endpoint;


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

    // command line arguments
    if (process_args(argc, argv, ip_addr, port, param, print_errors) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }
    
    // create instance of @c simple_chat_screen class
    simple_chat_screen screen(ip_addr, port, param, print_errors);

    /**************************************/
    /********** lambda callbacks **********/
    /**************************************/

    // message handler for @c network_entity
    auto msg_hndlr = [&] (const_buf buf, io_output io_out, endpoint ep)
        {
            // receive network text message, display to user
            // remove deliminator here?
            std::string s(static_cast<const char *>(buf.data()), buf.size());

            screen.insert_scroll_line(s, REMOTE);
            screen.draw_screen();

            return true;
        };

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
                // tcp_iof = iof; // return @c iof to main
            } else {
                // since we are peer to peer, reject >1 connections
                screen.insert_scroll_line("2nd tcp_connector client rejected" + 
                            DELIM, SYSTEM);
                screen.draw_screen();
                iof.start_io(DELIM, msg_hndlr);
                const std::string err = "only one tcp connection allowed";
                auto ret = iof.make_io_output();
                auto io_out = *ret;
                io_out.send(err.data(), err.size());
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                iof.stop_io();
            }
        } else { // flag false
            if (print_errors) {
                screen.insert_scroll_line("io_interface stop" + DELIM, SYSTEM);
                screen.draw_screen();
            }
        }
    };

    // error handler
    auto err_func = [&] (io_interface iof, std::error_code err) 
        { 
            static int count = 0;
            // shudtown if too many error messages
            if (++count > 20) {
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

    /********************************/
    /********** start here **********/
    /********************************/

    // work guard - handles @c std::thread and @c asio::io_context management
    chops::net::worker wk;
    wk.start();
    
    // create @c net_ip instance
    chops::net::net_ip chat(wk.get_io_context());
    // empty(default) @c net_entity
    chops::net::net_entity net_entity;

    if (param == PARAM_CONNECT) {
        // make @c tcp_connector, return @c network_entity
        // TODO: why does this not work with std::string, but acceptor does?
        net_entity = chat.make_tcp_connector(port.c_str(), ip_addr.c_str());
    } else {
        // make @ tcp_acceptor, return @c network_entity
        net_entity = chat.make_tcp_acceptor(port.c_str(), ip_addr.c_str());
    }
    assert(net_entity.is_valid());
     // start network entity, emplace handlers
    net_entity.start(io_state_chng_hndlr, err_func);

    /**************************************/
    /********** user interaction **********/
    /**************************************/

    screen.draw_screen();      
    // get std::string from user, send string data over network, update screen
    while (!shutdown) {
        std::string s;
        std::getline (std::cin, s); // user input
        if (s == "quit") {
            shutdown = true;
        }
        s += DELIM; // needed for deliminator
        screen.insert_scroll_line(s, LOCAL);
        screen.draw_screen();

        // send user text message over network
        // note correct method of sending string data over network
        auto ret = net_entity.visit_io_output([&s] (io_output io_out) {
                io_out.send(s.data(), s.size());
            } // end lambda
        );
        
        // check result
        if (ret && *ret == 0) {
            // no connection or other user quit
            screen.insert_scroll_line(NO_CONNECTION, SYSTEM);
            screen.draw_screen();
        } // end if
    } // end while

    /******************************/
    /********** shutdown **********/
    /******************************/

    net_entity.stop();
    wk.reset();

    return EXIT_SUCCESS;
}
