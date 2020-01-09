/** @file
 * 
 *  @ingroup example_module
 * 
 *  @brief Example of TCP multichat server network program.
 * 
 *  @author Thurman Gillespy
 * 
 *  Copyright (c) 2019 Thurman Gillespy
 *  2019-10-28
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
chat_server_demo.cpp -lpthread -o chat_server
 *
 */

#include <iostream>
#include <cstdlib> // EXIT_SUCCESS
#include <cstddef> // std::size_t
#include <string>
#include <chrono>
#include <thread>
#include <utility> // std::ref
#include <cassert>

#include "net_ip/net_ip.hpp"
#include "net_ip/net_entity.hpp"
#include "net_ip_component/worker.hpp"
#include "net_ip_component/send_to_all.hpp"
#include "marshall/extract_append.hpp"
#include "net_ip/io_type_decls.hpp"

using io_context = asio::io_context;
using io_output = chops::net::tcp_io_output;
using io_interface = chops::net::tcp_io_interface;
using const_buf = asio::const_buffer;
using endpoint = asio::ip::tcp::endpoint;

// process command line args
// set ip_addr, port, param, print_errors as needed
bool process_args(int argc,  char* const argv[], std::string& ip_addr,
                  std::string& port, bool& print_errors) {
   const std::string PORT = "5001";
   const std::string LOCAL_LOOP = "127.0.0.1";
   const std::string USAGE =
       "usage:\n"
       "  ./chat_server [-h] [-e] [port]\n"
       "      -h   print usage\n"
       "      -e   print all error messages to console\n"
       "      default port = " + PORT + "\n"
       "      server IP address (fixed) = " + LOCAL_LOOP + " (local loop)";

   const std::string HELP_PRM = "-h";
   const std::string ERR_PRM = "-e";

   // set default values
   ip_addr = LOCAL_LOOP;
   port = PORT;

   if (argc > 3 || (argc == 2 && argv[1] == HELP_PRM)) {
      std::cout << USAGE << std::endl;
      return EXIT_FAILURE;
   }

   if (argc == 2) {
      if (argv[1] == ERR_PRM) {
         print_errors = true;
      } else {
         port = argv[1];
      }
   }

   if (argc == 3) {
      if (argv[1] == ERR_PRM) {
         print_errors = true;
         port = argv[2];
      } else {
         std::cout << USAGE << std::endl;
         return EXIT_FAILURE;
      }
   }

   return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
   const std::string LOCAL = "[local]  ";
   const std::string SYSTEM = "[system] ";
   const std::string SERVER = "[server] ";
   const std::string DELIM = "\a"; // alert (bell)
   std::string ip_addr;
   std::string port;
   bool print_errors = false;
   bool finished = false;

   if (process_args(argc, argv, ip_addr, port, print_errors) == EXIT_FAILURE) {
      return EXIT_FAILURE;
   }

   // work guard - handles @c std::thread and @c asio::io_context management
   chops::net::worker wk;
   wk.start();

   // handles all @c io_interfaces
   // REVIEW WITH CLIFF
   chops::net::send_to_all<chops::net::tcp_io> sta;

   /* lamda handlers */
   // receive text from client, send out to others
   const auto msg_hndlr = [finished, &sta, &DELIM](const_buf buf, io_output io_out, endpoint ep) {
      if (finished) {
         return false;
      }
      
      // retransmit message
      const std::string s (static_cast<const char*> (buf.data()), buf.size());
      if (s == "quit" + DELIM) {
         // send 'quit' to originator
         // originator needs message for io_state_change halt
         io_out.send(buf.data(), buf.size()); 
      } else {
         // not 'quit' (normal message)
         // so send message to all but originator
         // REVIEW WITH CLIFF - send to all?
         sta.send(buf.data(), buf.size(), io_out);
      }

      return true;
   };

   const auto io_state_chng_hndlr = [&sta, &msg_hndlr, &DELIM]
                              (io_interface iof, std::size_t n, bool flag) {
      // add to or remove @c io_interface from list in sta
      sta(iof, n, flag);
      if (flag) {
         iof.start_io(DELIM, msg_hndlr);
      }
   };

   const auto err_func = [&print_errors](io_interface iof, std::error_code err) {
      if (print_errors) {
         std::cerr << err << ", " << err.message() << std::endl;
      }
   };

   // create @c net_ip instance
   chops::net::net_ip server(wk.get_io_context());
   // make @c tcp_acceptor, ruitn @c network_entity
   auto net_entity = server.make_tcp_acceptor(port.c_str());
   assert(net_entity.is_valid());
   // start network entity, emplace handlers
   net_entity.start(io_state_chng_hndlr, err_func);

   std::cout << "chops-net-ip chat server demo" << std::endl;
   std::cout << "  port: " << port << std::endl;
   if (print_errors) {
      std::cout << "  all error messages printed to console" << std::endl;
   }
   
   while (!finished) {
      std::string s;
      std::cout << "press return to exit" << std::endl;
      getline(std::cin, s);
      // ignore any user input
      // notify clients of server shutdown
      s = "server shutting down..." + DELIM;
      sta.send(s.data(), s.size());
      finished = true;
   }
   // delay so message gets sent
   std::this_thread::sleep_for(std::chrono::milliseconds(1000));
   // shutdown
   std::cerr << "shutdown...\n";
   net_entity.stop();
   
   std::this_thread::sleep_for(std::chrono::milliseconds(200));

   wk.stop();


   return EXIT_SUCCESS;
}