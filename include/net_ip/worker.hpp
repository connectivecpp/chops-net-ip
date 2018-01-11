/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Convenience executor and work guard class.
 *
 *  @author Cliff Green
 *  @date 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef WORKER_HPP_INCLUDED
#define WORKER_HPP_INCLUDED

#include <thread>

#include <experimental/io_context>
#include <experimental/executor>

namespace chops {
namespace net {

/**
 *  @brief Convenience class that combines an executor, work guard, and a thread,
 *  invoking asynchronous operations as per the C++ Networking TS.
 */
class worker {
private:
  std::experimental::net::io_context  m_ioc;
  std::experimental::net::executor_work_guard<typename std::experimental::net::io_context::executor_type> 
                                      m_wg;
  std::thread                         m_run_thr;

public:
  worker() : m_ioc(), m_wg(make_work_guard(m_ioc)), m_run_thr() { }

/**
 *  @brief Provide access to the @c io_context.
 *
 *  @return Reference to a @c std::experimental::net::io_context.
 */
  std::experimental::net::io_context& get_io_context() { return m_ioc; }

/**
 *  @brief Start the thread that invokes the underlying asynchronous
 *  operations.
 */
  void start() {
    m_run_thr = std::thread([this] () { m_ioc.run(); } );
  }

/**
 *  @brief Shutdown the executor and join the thread.
 */
  void stop() {
    m_ioc.stop();
    m_run_thr.join();
  }

};


}  // end net namespace
}  // end chops namespace

#endif

