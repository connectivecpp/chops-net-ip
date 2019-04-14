/** @file
 * 
 *  @ingroup example_module
 * 
 *  @brief Example of TCP multichat server network program.
 * 
 *  @author Thurman Gillespy
 * 
 *  Copyright (c) 2019 Thurman Gillespy
 *  4/12/19
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
multichat_server_demo.cpp -lpthread -o server
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
#include "net_ip/basic_net_entity.hpp"
#include "net_ip/component/worker.hpp"
#include "net_ip/component/send_to_all.hpp"
#include "simple_chat_screen.hpp"

using io_context = asio::io_context;
using io_interface = chops::net::tcp_io_interface;
using const_buf = asio::const_buffer;
using endpoint = asio::ip::tcp::endpoint;

// process command line args, set ip_addr, port, param as needed
bool process_args(int argc, char *argv[], std::string &ip_addr,
                  std::string &port, std::string &param) {
   const std::string PORT = "5001";
   const std::string LOCAL_LOOP = "127.0.0.1";
   const std::string USAGE =
       "usage:\n"
       "  ./chat_server [-h]  [port]\n"
       "     -h   print usage\n"
       "      default port = " + PORT + "\n"
       "      server IP address = " + LOCAL_LOOP + " (local loop)";

   const std::string HELP = "-h";

   // set default values
   ip_addr = LOCAL_LOOP;
   port = PORT;

   if (argc > 2 || (argc == 2 && argv[1] == HELP)) {
      std::cout << USAGE << std::endl;
      return EXIT_FAILURE;
   }

   if (argc == 2) {
      port = argv[1];
   }

   return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
   const std::string LOCAL = "[local]  ";
   const std::string SYSTEM = "[system] ";
   const std::string SERVER = "[server] ";
   const std::string DELIM = "\a"; // alert (bell)
   const std::string NO_CONNECTION = "error: no connection" + DELIM;
   const std::string ABORT = "abort: too many errors";
   const std::string WAIT_CONNECT = "waiting for connection..." + DELIM;
   std::string ip_addr;
   std::string port;
   std::string param;
   bool finished = false;

   if (process_args(argc, argv, ip_addr, port, param) == EXIT_FAILURE) {
      return EXIT_FAILURE;
   }

   // work guard - handles @c std::thread and @c asio::io_context management
   chops::net::worker wk;
   wk.start();

   // handles all @c io_interfaces
   chops::net::send_to_all<chops::net::tcp_io> sta;

   /* lamda handlers */
   // receive text from client, send out to others
   auto msg_hndlr = [&](const_buf buf, io_interface iof, endpoint ep) {
      sta.send(buf.data(), buf.size(), iof);
      return true;
   };

   auto io_state_chng_hndlr = [&](io_interface iof, std::size_t n, bool flag) {
      // add to or remove from list
      sta(iof, n, flag);
      if (flag) {
         iof.start_io(DELIM, msg_hndlr);
      }
   };

   auto err_func = [&](io_interface iof, std::error_code err) {
      std::cerr << err << " " << err.message() << std::endl;
   };

   // create @c net_ip instance
   chops::net::net_ip server(wk.get_io_context());
   // make @c tcp_acceptor, ruitn @c network_entity
   auto net_entity = server.make_tcp_acceptor(port.c_str(), ip_addr.c_str());
   // start network entity, emplace handlers
   net_entity.start(io_state_chng_hndlr, err_func);

   std::cout << "chops-net-ip chat sever demo" << std::endl;
   
   while (!finished) {
      std::string s;
      std::cout << "press any key to exit" << std::endl;
      getline(std::cin, s);
      s = "server shutting down" + DELIM;
      sta.send(s.data(), s.size());
      finished = true;
   }
   
   std::this_thread::sleep_for(std::chrono::milliseconds(500));

   wk.stop();


   return EXIT_SUCCESS;
}