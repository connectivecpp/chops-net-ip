// Chris Kohlhoff's Networking TS implementation appears to be buggy when 
// using basic_resolver::async_resolve, on g++ 7.2.
//
// This is test code planned to submitted to Chris' issue tracker on Github.
//
// Update 1/14/2018 - cannot reproduce problem, turned out to be issue with my own code.
// Synopsis: resolver object must stay in scope while async_resolve handler is alive.
//

#include <experimental/internet>
#include <experimental/socket>
#include <experimental/io_context>

#include <system_error>
#include <thread>

#include <iostream>

int main() {

using namespace std::experimental::net;
using namespace std::literals::chrono_literals;

  using Protocol = ip::tcp;
  using results_t = ip::basic_resolver<Protocol>::results_type;

  io_context    ioc;
  auto          wg = make_work_guard(ioc);
  std::thread   run_thr( [&ioc] { ioc.run(); } );

  ip::basic_resolver<Protocol> res(ioc);
  res.async_resolve("cnn.com", "80",
                    [] (const std::error_code& err, results_t res) {
                      std::cerr << "In lambda, ready to copy results, results size: " << res.size() << std::endl;
                      for ( const auto& i : res) {
                        std::cerr << "In lambda, endpoint entry: " << i.endpoint() << std::endl;
                      }
                    }
  );
  std::this_thread::sleep_for(5s); // sleep for 5 seconds
  ioc.stop();
  run_thr.join();
}


