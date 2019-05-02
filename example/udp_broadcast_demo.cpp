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
 *  5/1/19
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
#include <chrono>
#include <cassert>

#include "net_ip/net_ip.hpp"
#include "net_ip/basic_net_entity.hpp"
#include "net_ip/component/worker.hpp"

int main(int argc, char* argv[]) {
    bool print_errors = true;
    chops::net::udp_io_interface udp_iof;

    /**** lambda callbacks ****/
    // io state change handler
    auto io_state_chng_hndlr = [&udp_iof] 
        (io_interface iof, std::size_t n, bool flag) {
        
        if (flag) {
            iof.start_io();
            udp_iof = iof; // return iof to main, used later to send text
        } else {
            iof.stop_io();
        }
    
    };

    // error handler
    auto err_func = [&print_errors] (io_interface iof, std::error_code err) {
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
    updne = udp_broad.make_udp_sender(); // send only, no reads
    assert(udpne.is_valid());

    udpne.start(io_state_chng_hndlr, err_func);

    // begin
    std::cout << "chops-net-ip UDP broadcast demo" << std::endl;
    std::cout << "Enter tedt for UDP broadcast on this subnet" << std::endl;
    std::cout << "Enter \'quit\' or empty string to exit proggram" << sed::endl;

    bool finished = false;

    whike (!finished) {
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
    net_entity_connect.stop();
    wk.stop();

    return EXIT_SUCCESS;
}