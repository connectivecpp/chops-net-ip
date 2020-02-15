/** @file
 *
 *  @ingroup test_module
 *
 *  @brief Test application that receives progress messages from data sender / receiver (DSR)
 *  processes, both TCP and UDP versions.
 *
 *  @author (fill in)
 *
 *  Copyright (c) 2019 by Cliff Green, (fill in)
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include "net_ip/net_ip.hpp"
#include "monitor_msg_handling.hpp"
// function to create a server

class Monitor{
  // work guard - handles @c std::thread and @c asio::io_context management
   chops::net::worker worker;
   worker.start();
  // server is an acceptor
  // it recieves messages from the connected dsrs
  // create @c net_ip instance
   chops::net::net_ip monitor(worker.get_io_context());
   const std::string port = "5001";
   auto net_entity = monitor.make_tcp_acceptor(port.c_str());
   assert(net_entity.is_valid());
   // start network entity, emplace handlers
   net_entity.start(io_state_chng_hndlr, err_func);
   std::cout << "Monitor started..." << std::endl;

   // use this for shutdown
   chops::net::send_to_all<chops::net::tcp_io> sta;

   monitor_acceptor monitor_acceptor();
   std::promise<void> shutdown_prom;
   auto shutdown_fut = shutdown_prom.get_future();

//______---------------------------------
// monitor message is null terminated stream of bytes with /n as end
// 's' will initiate shutdown

// bool unnecessary. put in another place.
   bool finished = false;
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

   const auto msg_hndlr = [finished, &sta, &DELIM](const_buf buf, io_output io_out, endpoint ep) {
      if (finished) {
         return false;
      }
      const std::string s (static_cast<const char*> (buf.data()), buf.size());
      if (s == "shutdown" + DELIM) {
         start_shutdown(io_out, buf);
      } else {
         std::cout << " type 'shutdown' to initiate shutdown \n";
      }
  };
     const auto io_state_chng_hndlr = [&sta, &msg_hndlr, &DELIM]
                              (io_interface iof, std::size_t n, bool flag) {
      // add to or remove @c io_interface from list in sta
      sta(iof, n, flag);
      if (flag) {
         iof.start_io(DELIM, msg_hndlr);
      }
   };

  // output all of the things that are coming in

  void start_shutdown(io_output io_out, const_buf buf){
    // implement this function
    // receive shutdown from client, send out to others
    io_out.send(buf.data(), buf.size()); 

}

void create_monitor(int argc, char* argv[]){
  auto parms = temp_parse_cmd (argc, argv);
  // make the monitor use the params...... eventually......
  Monitor monitor;
  
  // connector sends shutdown messagge
  monitor

}

// temporary command line parsing while production version is being developed
// 1 - monitor name
// 2 - R means reply is true, anything else not
// 3 - mod
// 4 - send count
// 5 - delay
// 6 - A means acceptor, C means connector
// 7 - port
// 8 - host, if connector
auto temp_parse_cmd (int argc, char* argv[]) {
// skip this step for now
  // if (argc < 8) {
  //   std::cerr << "Parameters (temporary):" << '\n' <<
  //     "  name R/n mod cnt delay A/C port (host)" << '\n' <<
  //     "  where:" << '\n' <<
  //     "    name = DSR name" << '\n' <<
  //     "    R = reply (anything else means no reply)" << '\n' <<
  //     "    mod = display modulo number" << '\n' <<
  //     "    cnt = send count (0 for receive only)" << '\n' <<
  //     "    delay = delay in milliseconds between sends (ignored if send count is 0)" << '\n' <<
  //     "    A/C = A for acceptor, C for connector" << '\n' <<
  //     "    port = port number (for either acceptor or connector)" << '\n' <<
  //     "    host = if connector, host name or IP number" << '\n' << std::endl;
  //   std::exit(0);
  // }'
// 1 - monitor name
// 2 - R for reply
// 3 - mod
// 4 - send count
// 5 - delay
// 6 - A for acceptor, C for connector
// 7 - port
// 8 - host, if connector

  chops::test::tcp_monitor_args parms;
  parms.m_monitor_name = std::string(argv[1]);
  //parms.m_reply = (*(argv[2]) == 'R');
  // parms.m_modulus = std::stoi(std::string(argv[3]));
  // parms.m_send_count = std::stoi(std::string(argv[4]));
  // parms.m_delay = std::chrono::milliseconds(std::stoi(std::string(argv[5])));
  std::string port { argv[2] };
  // one of the things that defines the monitor is that it is an acceptor
  parms.m_acceptors.push_back(port);
  // Where the 'stop' command will be sent from
  std::string host { argv[3] };
    parms.m_connectors.push_back( chops::test::connector_info { port, host } );

  return parms;
}


int main (int argc, char* argv[]) {
  // The monitoring app is a server (acceptor)
  // monitor gets info from sender data server reciever
  // each node contacts monitor with status
  // monitor can initiate shutdown
  // We need the ability to display the data
  // start with cout/cerrors
  // look at curses library


  // parse commands
  auto params = temp_parse_cmd(argc, argv[]);


  // instantiate monitor
  Monitor monitor;

  // look at tmp_parse_cmd in tcp_dsr
  // use --fsanitize=address
  // initialize a server

  return 0;
}

