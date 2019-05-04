/**
 *  @fille
 * 
 *  @ingroup example_module
 * 
 *  @brief UDP reciever demo
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
#include <chrono>
#include <cassert>

#include "net_ip/net_ip.hpp"
#include "net_ip/basic_net_entity.hpp"
#include "net_ip/component/worker.hpp"

const std::string PORT = "5005";

int main(int argc, char* argv[]) {
    std::string port = PORT;
    std::size_t max_buf = 1024;
    bool print_errors = true;
    chops::net::udp_io_interface udp_iof;

    /**** lambda callbacks ****/
    // message handler
    // receive text, display to console
    auto msg_hndlr = [] (asio::const_buffer buf, chops::net::udp_io_interface iof,
        asio::ip::udp::endpoint ep) {
        // create string from buf
        std::string s (static_cast<const char*> (buf.data()), buf.size());
        std::cout << "UPD message received from " << ep.address() << std::endl;
        std::cout << "  " << s << std::endl;
    
        return true;
    };

    // io state change handler
    auto io_state_chng_hndlr = [&] 
        (chops::net::udp_io_interface iof, std::size_t n, bool flag) {
        
        if (flag) {
            iof.start_io(max_buf, msg_hndlr);
            udp_iof = iof; // return iof to main, used later to send text
        } else {
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
    chops::net::net_ip udp_receive(wk.get_io_context());

    // create a @c network_entitiy
    chops::net::udp_net_entity udpne;
    udpne = udp_receive.make_udp_unicast(port.c_str()); // send only, no reads
    assert(udpne.is_valid());

    udpne.start(io_state_chng_hndlr, err_func);

    // begin
    std::cout << "chops-net-ip UDP receiver demo" << std::endl;
    std::cout << "Press Enter to exit program" << std::endl;

    std::string s;
    getline(std::cin, s); // wait for any input

     // cleanup
    udpne.stop();
    wk.stop();

    return EXIT_SUCCESS;
}