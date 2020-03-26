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
 *  2019-10-21
 * 
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *  
 *  Sample make file:
g++ -std=c++17 -Wall -Werror \
-I ../include \
-I ../../utility-rack/include/ \
-I ../../utility-rack/third_party/ \
-I ../../asio/asio/include/ \
echo_binary_text_server_demo.cpp -lpthread -o echo_server
 * 
 */

#include <iostream>
#include <cstdlib> // EXIT_SUCCESS
#include <cstddef> // std::size_t 
#include <string>
#include <thread>
#include <algorithm> // std::for_each
#include <cassert>

#include "net_ip/net_ip.hpp"
#include "net_ip/net_entity.hpp"
#include "net_ip_component/worker.hpp"
#include "marshall/extract_append.hpp"
#include "net_ip/io_type_decls.hpp"

using io_context = asio::io_context;
using io_output = chops::net::tcp_io_output;
using io_interface = chops::net::tcp_io_interface;
using const_buf = asio::const_buffer;
using endpoint = asio::ip::tcp::endpoint;

// process command line args (if any)
bool processArgs(int argc, char* argv[], bool& print_errors, std::string& port) {
    const std::string HELP_PRM = "-h";
    const std::string PRINT_ERRS = "-e";
    const std::string USEAGE = \
    "useage: ./echo_server [-h | -e] [port]\n"
    "  -h    Print useage\n"
    "  -e    Print error messages\n"
    "  port  Default: 5002";
    int offset = 0;

    if (argc > 3 || (argc > 1 && argv[1] == HELP_PRM)) {
        std::cout << USEAGE << std::endl;
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

constexpr std::size_t HDR_SIZE{2}; // 1st 2 bytes of data is message size

int main(int argc, char* argv[]) {
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
    auto msg_hndlr = [] (const_buf buf, io_output io_out, endpoint ep) {
        // create string from buf, omit 1st 2 bytes (header)
        std::string s (static_cast<const char*> (buf.data()) + 2, buf.size() - 2);
        
        // print info about client
        std::cout << "received request from " << ep.address() << ":" << ep.port() << std::endl;
        std::cout << "  text: " << s << std::endl;
        // convert received text to uppercase
        auto to_upper = [] (char& c) { c = ::toupper(c); };
        std::for_each(s.begin(), s.end(), to_upper);

        // create buffer to send altered text back to client
        chops::mutable_shared_buffer buf_out;
        // 1st 2 bytes are the size of the message
        uint16_t size_val = s.size();
        // endian correct data marshalling
        std::byte tbuf[HDR_SIZE]; // temp buffer to hold the header
        // write those 2 bytes to the temp buffer
        std::size_t result = chops::append_val<uint16_t>(tbuf, size_val);
        assert(result == HDR_SIZE);
        // now append our header and string data to the output buffer
        buf_out.append(tbuf, sizeof(tbuf)); // write the header
        buf_out.append(s.data(), s.size()); // now add the text data
        // send message back to the client
        io_out.send(buf_out.data(), buf_out.size());
        

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
            // endian correct data marshalling
            uint16_t size = chops::extract_val<uint16_t> 
                                (static_cast<std::byte*> (buf.data()));
            
            return size; // return the size of the text data (obtained from header)
        }
    };

    // io state change handler
    auto io_state_chng_hndlr = [&msg_hndlr, &msg_frame] 
        (io_interface iof, std::size_t n, bool flag) {
        
        if (flag) {
            iof.start_io(HDR_SIZE, msg_hndlr, msg_frame);
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
    chops::net::net_entity net_entity_accept;

    // make @ tcp_acceptor, return @c network_entity
    net_entity_accept = echo_server.make_tcp_acceptor(port.c_str());
    assert(net_entity_accept.is_valid());
    // start network entity, emplace handlers
    net_entity_accept.start(io_state_chng_hndlr, err_func);

    // begin
    std::cout << "chops-net-ip binary text echo demo - server" << std::endl;
    std::cout << "  IP address:port = 127.0.0.1:" << port << std::endl;
    std::cout << "  print error messages: " << (print_errors ? "ON" : "OFF") << std::endl; 
    std::cout << "Press return to exit" << std::endl;

    std::string s;
    std::getline(std::cin, s); // pause until return
    
    // cleanup
    net_entity_accept.stop();
    wk.reset();

    return EXIT_SUCCESS;
}
