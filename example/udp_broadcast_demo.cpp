/**
 *  @fille
 * 
 *  @ingroup example_module
 * 
 *  @brief UDP broadcast demo. Text messages are sent to the local network
 *  UDP broadcast address.
 * 
 *  @author Thurman Gillespy
 * 
 *  @copyright (c) Thurman Gillespy
 *  8/19/19
 * 
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *  
 *  Sample make file. Assumes all repositories are in some directory.

g++ -std=c++17 -Wall -Werror \
-I ../include \
-I ../../utility-rack/include/ \
-I ../../asio/asio/include/ \
-I ../../boost* \
udp_broadcast_demo.cpp -lpthread -o udp_broad

 */

#include <iostream>
#include <cstdlib> // EXIT_SUCCESS
#include <string>
#include <thread>
#include <exception>
#include <cassert>

#include "net_ip/net_ip.hpp"
#include "net_ip/net_entity.hpp"
#include "net_ip/component/worker.hpp"
#include "net_ip/io_type_decls.hpp"

const std::string HELP_PRM = "-h";
const std::string ERR_PRM = "-e";
const std::string BROAD_PRM = "-b";

auto print_useage = [] () {
    const std::string USEAGE = \
    "usage:\n"
    "  ./udp_broad [-h]  Print useage\n"
    "  ./udp_broad [-e] <IP address> [subnet mask] [port]\n"
    "     -e             Print errors and system messages\n"
    "     IP address     IP address of this machine\n"
    "     subnet mask    Default: 255.255.255.0\n"
    "     port           Default: 5005\n"
    "  ./udp_broad [-e] -b <broadcast address> [port]\n"
    "     -e             Print errors and system messages\n"
    "     -b broadcast address\n"
    "        known broadcast address for this machine\n"
    "        ex: 192.168.1.255, 172.145.255.255, \n"
    "     port           Default: 5005";

    std::cout << USEAGE << std::endl;
};

// process comannand line process_args
bool process_args(int argc, char* argv[], bool& print_errors, std::string& ip_address, 
                std::string& net_mask, int& port, std::string& broadcast_addr) {
                
    int offset = 0;

    using addr4 = asio::ip::address_v4;

    if (argc == 1 || argv[1] == HELP_PRM) {
        print_useage();
        return EXIT_FAILURE;
    }

    if (argv[1] == ERR_PRM) {
        print_errors = true;
        offset = 1;
    }

    if (argc <= 1 + offset) {
        print_useage();
        return EXIT_FAILURE;
    }

    try {

    if (argv[1 + offset] == BROAD_PRM) {
        if (argc <= 2 + offset) {
            print_useage();
            return EXIT_FAILURE;
        }
        broadcast_addr = argv[2 + offset];
        // check address not malformed or empty
        try {
            addr4 t = asio::ip::make_address_v4(broadcast_addr);
            assert( t.to_string() != "");
        }

        catch (...) { throw; }

        if (argc  == 4 + offset) {
            port = std::stoi(argv[3 + offset]);
        }
    } else {
        // get ip address
        ip_address = argv[1 + offset];
        // subnet mask
        if (argc >= 3 + offset) {
            net_mask = argv[2 + offset];
        }
        // port
        if (argc >= 4 + offset) {
            port = std::stoi(argv[3 + offset]);
        }
        // too many params?
        if (argc >= 5 + offset) {
            std::cout << "too many parameters" << std::endl;
            print_useage();
            return EXIT_FAILURE;
        }
        // create broadcast address
        try {
            // DOES THIS WORK WHEN NETMASK IS NOT 255.255.255.0??
            addr4 as_addr  = asio::ip::make_address_v4(ip_address);
            addr4 as_netm  = asio::ip::make_address_v4(net_mask);
            addr4 as_broad = addr4::broadcast(as_addr, as_netm);
            
            broadcast_addr = asbroad.to_string();
        }

        catch (...) { throw; }
    } // end else
    } // end try

    catch (std::exception& e) {
        std::cout << "malformed ipv4 address or network mask" << std::endl;
        std::cout << "  what: " << e.what() << std::endl << std::endl;
        print_useage();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {
    std::string ip_address = "";
    std::string broadcast_addr = "";
    std::string net_mask = "255.255.255.0";
    const int PORT = 5005;
    bool print_errors = false;
    int port = PORT;
    // declare the net_entity here so accessable from the lambda
    chops::net::net_entity udp_ne;

    // chops::net::udp_io_interface udp_iof; NO LONGER NEEDED

    if (process_args(argc, argv, print_errors, ip_address, net_mask,  port, 
            broadcast_addr) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }
    assert(broadcast_addr != "");
    
    /**************************************/
    /********** lambda callbacks **********/
    /**************************************/

    // io state change handler
    auto io_state_chng_hndlr = [&udp_ne, &port, &broadcast_addr, print_errors] 
        (chops::net::udp_io_interface iof, std::size_t n, bool flag) {
        
        if (flag) {
            if (print_errors) {
                std::cout << "io state change: start_io" << std::endl;
            }

            // set socket flag for UPD broadcast
            // TODO - use visit_socket for next 3 lines
            udp_ne.visit_socket(); // ADD PARAMS
            auto& sock = iof.get_socket();
            asio::socket_base::broadcast opt(true);
            sock.set_option(opt);
            // set default endpoint broadcast address for this subnet
            asio::ip::udp::endpoint ep;
            // set the ip address and port
            ep.address(asio::ip::make_address_v4(broadcast_addr));
            ep.port(port);
            // start the io_interface
            iof.start_io(ep);
            udp_iof = iof; // return iof to main, used later to send text
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

    /********************************/
    /********** start here **********/
    /********************************/

    std::cout << "chops-net-ip UDP broadcast demo" << std::endl;
    if (ip_address != "") {
        std::cout << "  IP address:net mask = " << ip_address << ":" << net_mask << std::endl;
    }
    std::cout << "  broadcast address:port = " << broadcast_addr << ":" << port << std::endl;
    std::cout << "  print errors and system messages: ";
    std::cout << (print_errors ? "ON" : "OFF") << std::endl;
    std::cout << std::endl;
    std::cout << "Enter text for UDP broadcast on this subnet" << std::endl;
    std::cout << "Enter \'quit\' or empty string to exit proggram" << std::endl;

    // work guard - handles @c std::thread and @c asio::io_context management
    chops::net::worker wk;
    wk.start();
    
    // create @c net_ip instance
    chops::net::net_ip udp_broad(wk.get_io_context());

    // create a @c network_entitiy
    // chops::net::udp_net_entity udp_ne;
    // udp_ne declared above before lambdas
    // UDP unicast and multicast senders are the same
    udp_ne = udp_broad.make_udp_sender(); // send only, no reads
    assert(udp_ne.is_valid());
    // start it, emplace handlers
    udp_ne.start(io_state_chng_hndlr, err_func);
    
    // get text from user, send to UDP broadcast address
    bool finished = false;
    while (!finished) {
        std::string s;
        getline(std::cin, s);
        if (s == "quit" || s == "") {
            finished = true;
            continue;
        }
        // assert(udp_iof.is_valid());
        // udp_iof.send(s.data(), s.size());
        // send text, check result
        auto ret = upd_ne.visit_io_output([&s] (io_ouput io_out) {
                io_out.send(s.data(), s.size());
            }
        );

        if (ret && *ret == 0) {
            std::cout << "send failed\n";
        }
    } // end while

     /******************************/
    /********** shutdown **********/
    /******************************/

    udp_ne.stop();
    // wk.stop();
    wk.reset();

    return EXIT_SUCCESS;
}