/**
 *  @fille
 * 
 *  @ingroup example_module
 * 
 *  @brief UDP broadcast demo
 * 
 *  @author Thurman Gillespy
 * 
 *  @copyright (c) Thurman Gillespy
 *  5/2/19
 * 
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *  
 * 
 */

#include <iostream>
#include <cstdlib> // EXIT_SUCCESS
#include <string>
#include <thread>
#include <cassert>

#include "net_ip/net_ip.hpp"
#include "net_ip/basic_net_entity.hpp"
#include "net_ip/component/worker.hpp"

int main(int argc, char* argv[]) {
    std::string broadcast_addr = "192.168.1.255";
    const int PORT = 5005;
    bool print_errors = true;
    chops::net::udp_io_interface udp_iof;
    int port = PORT;

    /**** lambda callbacks ****/
    // io state change handler
    auto io_state_chng_hndlr = [&udp_iof, &port, &broadcast_addr ,print_errors] 
        (chops::net::udp_io_interface iof, std::size_t n, bool flag) {
        
        if (flag) {
            if (print_errors) {
                std::cout << "io state change: start_io" << std::endl;
            }
            
            // set socket flag for UPD broadcast
            auto& sock = iof.get_socket();
            asio::socket_base::broadcast opt(true);
            sock.set_option(opt);
            // set default endpoint broadcast address for this subnet
            asio::ip::udp::endpoint ep;
            // set tha ip address and port
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

    // work guard - handles @c std::thread and @c asio::io_context management
    chops::net::worker wk;
    wk.start();
    
    // create @c net_ip instance
    chops::net::net_ip udp_broad(wk.get_io_context());

    // create a @c network_entitiy
    chops::net::udp_net_entity udpne;
    udpne = udp_broad.make_udp_sender(); // send only, no reads
    assert(udpne.is_valid());
    // start it, emplace handlers
    udpne.start(io_state_chng_hndlr, err_func);
    
    // begin
    std::cout << "chops-net-ip UDP broadcast demo" << std::endl;
    std::cout << "Enter text for UDP broadcast on this subnet" << std::endl;
    std::cout << "Enter \'quit\' or empty string to exit proggram" << std::endl;

    bool finished = false;

    while (!finished) {
        std::string s;
        getline(std::cin, s);
        if (s == "quit" || s == "") {
            finished = true;
            continue;
        }
        assert(udp_iof.is_valid());
        udp_iof.send(s.data(), s.size());
    }

     // cleanup
    udpne.stop();
    wk.stop();

    return EXIT_SUCCESS;
}