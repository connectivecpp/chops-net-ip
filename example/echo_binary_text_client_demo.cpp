/**
 *  @file
 * 
 *  @ingroup example_module
 *  
 *  @brief TCP connector (client) that sends binary text message to a
 *  server, receives message back converted to upper case.
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
-I ../../asio/asio/include/ \
-I ../../expected-lite/include/ \
echo_binary_text_client_demo.cpp -lpthread -o echo_client
 * 
 */

#include <iostream>
#include <cstdlib> // EXIT_SUCCESS
#include <cstddef> // std::size_t 
#include <string>
#include <thread>
#include <chrono>
#include <cassert>

#include "net_ip/net_ip.hpp"
#include "net_ip/net_entity.hpp"
#include "net_ip_component/worker.hpp"
#include "marshall/extract_append.hpp"
#include "net_ip/io_type_decls.hpp"

using io_context = asio::io_context;
using io_output = chops::net::tcp_io_output;
using const_buf = asio::const_buffer;
using endpoint = asio::ip::tcp::endpoint;

// process command line args (if any)
bool process_args(int argc, char* argv[], bool& print_errors, std::string& ip_address, 
    std::string& port) {
    const std::string HELP_PRM = "-h";
    const std::string PRINT_ERRS = "-e";
    const std::string USEAGE = \
    "useage: ./echo_client [-h | -e] [ip address/hostname] [port]\n"
    "  -h           Print useage\n"
    "  -e           Print error messages\n"
    "  ip address   Default: 127.0.0.1 (LOCAL LOOP)\n"
    "  port         Default: 5002\n"
    "  change port and use local loop:\n"
    "    ./echo_client [-e] \"\" port";

    int offset = 0;

    if (argc > 4 || (argc > 1 && argv[1] == HELP_PRM)) {
        std::cout << USEAGE << std::endl;
        return EXIT_FAILURE;
    }

    if (argc > 1 && argv[1] == PRINT_ERRS) {
        print_errors = true;
        offset = 1;
    }

    if (argc == 2 + offset) {
        ip_address = argv[1 + offset];
    } else if (argc == 3 + offset) {
        ip_address = argv[1 + offset];
        port = argv[2 + offset];
    } else if (argc > 3 + offset) {
        std::cout << USEAGE << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}


int main(int argc, char* argv[]) {
    const std::size_t HDR_SIZE = 2; // 1st 2 bytes of data is message size
    const std::string PORT = "5002";
    const std::string LOCAL_LOOP = "127.0.0.1";

    std::string ip_address = LOCAL_LOOP;
    std::string port = PORT;
    bool hdr_processed = false;
    bool print_errors = false;

    // io_interface tcp_iof; // use this to send text messages

    if (process_args(argc, argv, print_errors, ip_address, port) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }

    /**** lambda handlers ****/

    // message handler
    // receive text, display to console
    auto msg_hndlr = [] (const_buf buf, io_interface iof, endpoint ep) {
        // create string from buf, omit 1st 2 bytes (header)
        std::string s (static_cast<const char*> (buf.data()) + 2, buf.size() - 2);
        std::cout << s << std::endl;
    
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
            
            return size;
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
    chops::net::net_ip echo_client(wk.get_io_context());

    // create a @c tcp_connector network entity
    chops::net::tcp_connector_net_entity net_entity_connect;
    net_entity_connect = echo_client.make_tcp_connector(port.c_str(), ip_address.c_str(),
                        std::chrono::milliseconds(5000));
    assert(net_entity_connect.is_valid());
    // start @c network_entity, emplace handlers
    net_entity_connect.start(io_state_chng_hndlr, err_func);

    // begin
    std::cout << "chops-net-ip binary text echo demo - client" << std::endl;
    std::cout << "  IP address:port = " << (ip_address == "" ? LOCAL_LOOP : ip_address);
    std::cout << ":" << port << std::endl;
    std::cout << "  print error messages: " << (print_errors ? "ON" : "OFF") << std::endl; 
    std::cout << "Enter text to send, or \'quit\' to exit" << std::endl;


    // get text to send from user, exit on 'quit'
    std::string s;
    bool shutdown = false;
    while (!shutdown) {
        std::getline (std::cin, s); // user input
        if (s == "quit") {
            shutdown = true;
            continue;
        }
        // @c tcp.iof is not valid when there is no network connection
        if (!tcp_iof.is_valid()) {
            std::cout << "no connection..." << std::endl;
            continue; // back to top of loop
        }
        
        // buffer to send entered message from user
        chops::mutable_shared_buffer buf_out;
        
        // 1st 2 bytes size of message (header) are the string length
        uint16_t size_val = s.size();
        // endian correct data marshalling
        std::byte tbuf[HDR_SIZE]; // temp buffer to hold the header
        // write those 2 bytes to the temp buffer
        std::size_t result = chops::append_val<uint16_t>(tbuf, size_val);
        assert(result == HDR_SIZE);
        // now append our header and string data to the output buffer
        buf_out.append(tbuf, sizeof(tbuf)); // write the header
        buf_out.append(s.data(), s.size()); // now add the text data
        // send message to server (TCP_acceptor)
        // tcp_iof.send(buf_out.data(), buf_out.size());
        net_entity_connect.visit_io_output([&buf_out] (io_output io_out) {
                io_out.send(buf_out);
            } // end lambda
        );
    } // end while
 
    // cleanup
    net_entity_connect.stop();
    wk.reset();

    return EXIT_SUCCESS;
}