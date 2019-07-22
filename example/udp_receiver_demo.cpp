/**
 *  @fille
 * 
 *  @ingroup example_module
 * 
 *  @brief UDP reciever demo. Receive text messages from UDP broadcast agent.
 * 
 *  @author Thurman Gillespy
 * 
 *  @copyright (c) Thurman Gillespy
 *  5/5/19
 * 
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *  
 *  Sample make file. Assumes all repositories are in same directory

g++ -std=c++17 -Wall -Werror \
-I ../include \
-I ../../utility-rack/include/ \
-I ../../asio/asio/include/ \
udp_receiver_demo.cpp -lpthread -o udp_receive

 *
 */

#include <iostream>
#include <cstdlib> // EXIT_SUCCESS
#include <string>
#include <thread>
#include <chrono>
#include <cassert>

#include "net_ip/net_ip.hpp"
#include "net_ip/net_entity.hpp"
#include "net_ip_component/worker.hpp"
#include "net_ip/io_type_decls.hpp"

const std::string PORT = "5005";
const std::string HELP_PRM = "-h";
const std::string ERRS_PRM = "-e";

auto print_useage = [] () {
    const std::string USEAGE = \
    "./udp_receive [-h] [-e] [port]\n"
    "   -h      Print usage\n"
    "   -e      Print error and system messages\n"
    "   port    Default: " + PORT;

    std::cout << USEAGE << std::endl;
};

bool process_args(int argc, char* argv[], bool& print_errors, std::string& port) {
    
    int offset = 0;

    if (argc >= 2 && argv[1] == HELP_PRM) {
        print_useage();
        return EXIT_FAILURE;
    }

    if (argc >= 2 && argv[1] == ERRS_PRM) {
        print_errors = true;
        offset = 1;
    }

    if (argc >= 2 + offset) {
        // get port
        port = argv[1 + offset];
    }

    // too many args?
    if (argc >= 3 + offset) {
        std::cout << "too many arguements" << std::endl;
        print_useage();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {
    std::string port = PORT;
    const std::size_t MAX_BUF = 256;
    bool print_errors = false;
    bool first_msg = true;
    chops::net::udp_io_interface udp_iof;

    if (process_args(argc, argv, print_errors, port) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }

    /**** lambda callbacks ****/
    // message handler
    // receive text, display to console
    auto msg_hndlr = [&first_msg] (asio::const_buffer buf, chops::net::udp_io_interface iof,
        asio::ip::udp::endpoint ep) {
        // create string from buf
        std::string s (static_cast<const char*> (buf.data()), buf.size());
        if (first_msg) {
            std::cout << "UPD broadcasts from " << ep.address() << ":\n";
            first_msg = false;
        }
        
        std::cout << "> " << s << std::endl;
    
        return true;
    };

    // io state change handler
    auto io_state_chng_hndlr = [&] 
        (chops::net::udp_io_interface iof, std::size_t n, bool flag) {
        
        if (flag) {
            iof.start_io(MAX_BUF, msg_hndlr);
            if (print_errors) {
                std::cout << "io state change: start_io" << std::endl;
            }
        } else {
            if (print_errors) {
                std::cout << "io state change: stop_io" << std::endl;
            }
            iof.stop_io();
        }
    
    };

    // error handler
    auto err_func = [&print_errors] (chops::net::udp_io_interface iof, std::error_code err) {
        if (print_errors) {
            std::string err_text = err.category().name();
            err_text += ": " + std::to_string(err.value()) + ", " +
            err.message();
            std::cerr << err_text << std::endl;
        }
    };

    // begin
    std::cout << "chops-net-ip UDP receiver demo" << std::endl;
    std::cout << "  print errors and system messages: ";
    std::cout << (print_errors ? "ON" : "OFF") << std::endl;
    std::cout << "  port: " << port << std::endl << std::endl;
    std::cout << "Press Enter to exit program" << std::endl;

    // work guard - handles @c std::thread and @c asio::io_context management
    chops::net::worker wk;
    wk.start();
    
    // create @c net_ip instance
    chops::net::net_ip udp_receive(wk.get_io_context());

    // create a @c network_entitiy
    chops::net::udp_net_entity udpne;
    udpne = udp_receive.make_udp_unicast(port.c_str());
    assert(udpne.is_valid());

    udpne.start(io_state_chng_hndlr, err_func);

    // pause for user input, then quit
    std::string s;
    getline(std::cin, s);

     // cleanup
    udpne.stop();
    wk.stop();

    return EXIT_SUCCESS;
}