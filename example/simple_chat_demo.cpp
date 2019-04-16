/** @file
 * 
 *  @ingroup example_module
 * 
 *  @brief Example of TCP 2-way network chat program.
 * 
 *  @author Thurman Gillespy
 * 
 *  Copyright (c) 2019 Thurman Gillespy
 *  4/15/19
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
#include <future> // std::promise, std::future
#include <cassert>

#include "net_ip/net_ip.hpp"
#include "net_ip/basic_net_entity.hpp"
#include "net_ip/component/worker.hpp"
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
bool process_args(int argc, char* const argv[], std::string& ip_addr, 
        std::string& port, std::string& param, bool& print_errors) {
    const std::string PORT = "5001";
    const std::string LOCAL_LOOP = "127.0.0.1";
    const std::string USAGE =
        "usage: ./chat [-h] [-e] -connect | -accept [ip address] [port]\n"
        "  -h  print usage info\n"
        "  -e  print error messages\n"
        "  -connect  tcp_connector\n"
        "  -accept   tcp_acceptor\n"
        "  default ip address: " + LOCAL_LOOP + " (local loop)\n"
        "  default port: " + PORT + "\n"
        "  if connection type = accept, IP address becomes \"\"";
    const std::string HELP = "-h";
    const std::string ERR = "-e";
    const std::string EMPTY = "";
    
    // set default values
    ip_addr = LOCAL_LOOP;
    port = PORT;

    if (argc < 2 || argc > 5) {
        std::cout << "incorrect parameter count\n";
        std::cout << USAGE << std::endl;
        return EXIT_FAILURE;
    };

    if (argv[1] == HELP) {
        std::cout << USAGE << std::endl;
        return EXIT_FAILURE;
    } else if (argv[1] == ERR) {
        print_errors = true;
    }
    
    if (print_errors) {
        if (argc == 2) {
            std::cout << USAGE << std::endl;
            return EXIT_FAILURE;
        }
        if (argv[2] == PARAM_CONNECT) {
            param = PARAM_CONNECT;
        } else if (argv[2] == PARAM_ACCEPT) {
            param = PARAM_ACCEPT;
        } else {
            std::cout << "incorrect second parameter: ";
            std::cout << "must be [-connect | -accept]" << std::endl;
            std::cout << USAGE << std::endl;
            return EXIT_FAILURE;
        }
    } else {
        if (argv[1] == PARAM_CONNECT) {
            param = PARAM_CONNECT;
        } else if (argv[1] == PARAM_ACCEPT) {
            param = PARAM_ACCEPT;
        } else {
            std::cout << "incorrect first parameter: ";
            std::cout << "must be [-h | -e | -connect | -accept]" << std::endl;
            std::cout << USAGE << std::endl;
            return EXIT_FAILURE;
        }
    }

    if (param == PARAM_ACCEPT) {
        ip_addr = "";
    }

    if (print_errors) {
        if (argc == 4 || argc == 5) {
            if (param == PARAM_CONNECT) {
                ip_addr = argv[3];
            }
        }

        if (argc == 5) {
            port = argv[4];
        }
    } else {
        if (argc == 3 || argc == 4) {
            if (param == PARAM_CONNECT) {
                ip_addr = argv[2];
            }
        }

        if (argc == 4) {
            port = argv[3];
        }
    }

    assert(param != "");

    return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {
    const std::string LOCAL = "[local]  ";
    const std::string SYSTEM = "[system] ";
    const std::string ERROR = "[error]  ";
    const std::string DELIM = "\a"; // alert (bell)
    const std::string NO_CONNECTION = "error: no connection" + DELIM;
    const std::string ABORT = "abort: too many errors";
    const std::string WAIT_CONNECT = "waiting for connection..." + DELIM;
    std::string ip_addr;
    std::string port;
    std::string param;
    std::string last_msg;
    bool print_errors = false;
    bool shutdown = false;
    std::promise<io_interface> promise_obj;
    std::future<io_interface> future_obj = promise_obj.get_future();

    if (process_args(argc, argv, ip_addr, port, param, print_errors) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }
    
    // create instance of @c simple_chat_screen class
    simple_chat_screen screen(ip_addr, port, param, print_errors);

    /* lambda callbacks */
    // message handler for @c network_entity
    auto msg_hndlr = [&] (const_buf buf, io_interface iof, endpoint ep)
        {
            if (shutdown) {
                screen.insert_scroll_line("msg_hndlr shutdown" + DELIM,
                    SYSTEM);
                screen.draw_screen();

                return false;
            }

            // receive data from acceptor, display to user
            // remove deliminator here?
            std::string s (static_cast<const char*> (buf.data()), buf.size());
            
            if (s != last_msg) {
                screen.insert_scroll_line(s, REMOTE);
                screen.draw_screen();

                // if user sent quit, echo back to allow
                // io_state_change handler to exit
                if (s == "quit" + DELIM) {
                    iof.send(s.data(), s.size());
                }
            }
            
            return true;
        };

    // handler for @c tcp_connector
    auto io_state_chng_hndlr = [&promise_obj, msg_hndlr, DELIM, SYSTEM, &screen]
                                (io_interface iof, std::size_t n, bool flag) {
        // only start the iof if flag is true (startup) and only 1
        if (flag && n == 1) {
            iof.start_io(DELIM, msg_hndlr);
            // return iof via promise/future handoff
            promise_obj.set_value(iof);
        }
        if (!flag) {
            screen.insert_scroll_line("io_state_chng shutdown" + DELIM, SYSTEM);
            screen.draw_screen();
            assert(iof.is_valid());
            iof.stop_io();
        }
    };

    // error handler
    auto err_func = [&] (io_interface iof, std::error_code err) 
        { 
            static int count = 0;

            if (++count > 15) {
                std::cerr << ABORT << std::endl;
                exit(0);
            }

            if (err.value() == 111 || err.value() == 113) {
                screen.insert_scroll_line(WAIT_CONNECT, SYSTEM);
                screen.draw_screen();
            }

            if (print_errors) {
                std::string err_text = err.category().name();
                err_text += ": " + std::to_string(err.value()) + ", " +
                err.message() + DELIM;
                screen.insert_scroll_line(err_text, ERROR);
                screen.draw_screen();
            }
        };

    // work guard - handles @c std::thread and @c asio::io_context management
    chops::net::worker wk;
    wk.start();
    { 
    // create @c net_ip instance
    chops::net::net_ip chat(wk.get_io_context());
    

    chops::net::tcp_connector_net_entity net_entity_connect;
    chops::net::tcp_acceptor_net_entity net_entity_accept;
    chops::net::tcp_acceptor_net_entity* net_entity_ptr = nullptr;

    if (param == PARAM_CONNECT) {
        // make @c tcp_connector, return @c network_entity
        // TODO: why does this not work with std::string, but acceptor does?
        net_entity_connect = chat.make_tcp_connector(port.c_str(), ip_addr.c_str(),
                    std::chrono::milliseconds(5000));
        assert(net_entity_connect.is_valid());
        // start network entity, emplace handlers
        net_entity_connect.start(io_state_chng_hndlr, err_func);
        net_entity_ptr = 
            reinterpret_cast<chops::net::tcp_acceptor_net_entity*> (&net_entity_connect);
    } else {
        // make @ tcp_acceptor, return @c network_entity
        net_entity_accept = chat.make_tcp_acceptor(port.c_str(), ip_addr.c_str());
        assert(net_entity_accept.is_valid());
        // start network entity, emplace handlers
        net_entity_accept.start(io_state_chng_hndlr, err_func);
        net_entity_ptr = &net_entity_accept;
    }

    screen.draw_screen();

     // io state change handler
    tcp_io_interface tcp_iof; // used to send text data
    tcp_iof = future_obj.get(); // wait for value set in io_state_chng_hndlr
    
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
        last_msg = s;
        if (!tcp_iof.is_valid()) {
            screen.insert_scroll_line(NO_CONNECTION, SYSTEM);
            screen.draw_screen();
            continue;
        }
        // note correct method of sending string data over network
        tcp_iof.send(s.data(), s.size());
       
    }
    // allow last message to be sent before shutting down connection
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // shutdown
    net_entity_ptr->stop();
    
    // if (param == PARAM_CONNECT) {
    //     screen.insert_scroll_line("connect exit" + DELIM, SYSTEM);
    //     screen.draw_screen();

    //     if (net_entity_connect.is_valid() && net_entity_connect.is_started()) {
    //         net_entity_connect.stop();
    //     }

    //     // chat.stop_all();
    //     // chat.remove_all();
    // } else {
    //     screen.insert_scroll_line("accept exit" + DELIM, SYSTEM);
    //     screen.draw_screen();
    //     assert(net_entity_accept.is_valid());
    //     net_entity_accept.stop(); // still leaks
    //     // chat.stop_all();
    //     // chat.remove_all();
    // }

    } // ::net_ip chat goes out of scope
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    wk.stop();

    return EXIT_SUCCESS;
}