/**
 *  @file
 * 
 *  @ingroup example_module
 *  
 *  @brief TCP acceptor (server) that receives binary text messages, converts
 *  to upper case, then echos back to TCP connector (client).
 * 
 *  @author Thurman Gillespy
 * 
 *  Copyright (c) Thurman Gillespy
 *  4/25/19
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
echo_binary_text_server_demo.cpp -lpthread -o echo_server
 * 
 */

#include <iostream>
#include <cstdlib> // EXIT_SUCCESS
#include <cstddef> // std::size_t 
#include <string>
#include <thread>
#include <cassert>

#include "net_ip/net_ip.hpp"
#include "net_ip/basic_net_entity.hpp"
#include "net_ip/component/worker.hpp"
#include "utility/cast_ptr_to.hpp"
#include "marshall/extract_append.hpp"

using io_context = asio::io_context;
using io_interface = chops::net::tcp_io_interface;
using const_buf = asio::const_buffer;
using endpoint = asio::ip::tcp::endpoint;

bool processArgs(int argc, char* argv[], bool& print_errors, std::string& port) {
    const std::string HELP = "-h";
    const std::string PRINT_ERRS = "-e";
    const std::string usage = \
    "useage: ./echo_server [-h | -e] [port]\n"
    "  -h    Print useage\n"
    "  -e    Print error messages\n"
    "  port  Default port: 5002";
    int offset = 0;

    if (argc > 3 || (argc > 1 && argv[1] == HELP)) {
        std::cout << usage << std::endl;
        return EXIT_FAILURE;
    }

    if (argc > 1 && argv[1] == PRINT_ERRS) {
        print_errors = true;
        offset = 1;
    }

    if (argc > 1 + offset) {
        port = argv[1 + offset];
    }

    return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {
    const std::size_t HDR_SIZE = 2; // 1st 2 bytes of header is message size
    const std::string PORT = "5002";

    std::string port = PORT;
    bool hdr_processed = false;
    bool print_errors = false;

    if (processArgs(argc, argv, print_errors, port) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }
    
    /**** lambda handlers ****/

    // message handler
    // receive text, convert to uppercase, send back to client
    auto msg_hndlr = [] (const_buf buf, io_interface iof, endpoint ep) {
        // create string from buf, omit 1st 2 bytes (header)
        std::string s (static_cast<const char*> (buf.data()) + 2, buf.size() - 2);
        // std::string ss (chops::cast_ptr_to<const char, const std::byte> (buf.data()), buf.size());
        // print info about client
        std::cout << "received request from " << ep.address() << ":" << ep.port() << std::endl;
        std::cout << "  text: " << s << std::endl;
        // convert to uppercase
        auto to_upper = [] (char& c) { c = ::toupper(c); };
        std::for_each(s.begin(), s.end(), to_upper);

        // create buffer to send test data
        chops::mutable_shared_buffer buf_out;
        // 1st 2 bytes are the size of the message
        uint16_t size_val = s.size();
        // endian correct data marshalling
        std::byte tbuf[HDR_SIZE];
        std::size_t result = chops::append_val<uint16_t>(tbuf, size_val);
        assert(result == HDR_SIZE);
        // chops::append_val<uint16_t>(const_cast<std::byte*> 
                            // (static_cast<const std::byte*> (buf.data())), size_val);
        // buf_out.append(&size_val, sizeof(size_val)); // write 2 byte size
        buf_out.append(tbuf, sizeof(tbuf)); // write the header
        buf_out.append(s.data(), s.size()); // now add the text data
        
        iof.send(buf_out.data(), buf_out.size());

        return true;
    };

    // message frame handler
    // 1st call: buffer contains only the header, return message size, toggle flag
    // 2nd call: return 0 to indicate no further prodessing, toggle flag
    auto msg_frame = [&hdr_processed] (asio::mutable_buffer buf) -> std::size_t {
        
        if (hdr_processed) {
            hdr_processed = false;
            return 0;
        } else {
            hdr_processed = true;
            // 1st 2 bytes is message size
            // uint16_t size = *(static_cast<uint16_t*> (buf.data()));
            // endian correct data marshalling
            uint16_t size = chops::extract_val<uint16_t> 
                                (static_cast<std::byte*> (buf.data()));
            // std::cerr << "msg_frame: size = " << size << std::endl;
            
            return size;
        }
    };

    // io state change handler
    auto io_state_chng_hndlr = [&msg_hndlr, &msg_frame] 
        (io_interface iof, std::size_t n, bool flag) {
        
        if (flag) {
            iof.start_io(HDR_SIZE, msg_hndlr, msg_frame);
        } else {
            iof.stop_io();
        }
    
    };

    // error handler
    auto err_func = [&] (io_interface iof, std::error_code err) {
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
    chops::net::net_ip echo_server(wk.get_io_context());
    chops::net::tcp_acceptor_net_entity net_entity_accept;

    // make @ tcp_acceptor, return @c network_entity
    net_entity_accept = echo_server.make_tcp_acceptor(port.c_str());
    assert(net_entity_accept.is_valid());
    // start network entity, emplace handlers
    net_entity_accept.start(io_state_chng_hndlr, err_func);

    std::cout << "chops-net-ip binary text echo demo - server" << std::endl;
    std::cout << "  IP address:port = 127.0.0.1:" << port << std::endl;
    std::cout << "  print error messages: " << (print_errors ? "ON" : "OFF") << std::endl; 
    std::cout << "Press return to exit" << std::endl;

    std::string s;
    std::getline(std::cin, s); // pause until return
    
    // cleanup
    net_entity_accept.stop();
    wk.stop();

    return EXIT_SUCCESS;
}