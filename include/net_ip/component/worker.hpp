/** @file 
 *
 *  @defgroup net_ip_component_module Class and functions useful with the Chops Net IP 
 *  library, but not part of the core library dependencies.
 *
 *  @ingroup net_ip_component_module
 *
 *  @brief Convenience executor and work guard class.
 *
 *  @author Cliff Green
 *
 *  Copyright (c) 2018 by Cliff Green
 *
 *  Distributed under the Boost Software License, Version 1.0. 
 *  (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#ifndef WORKER_HPP_INCLUDED
#define WORKER_HPP_INCLUDED

#include <thread>

#include <exception>
#include <iostream>

#include "asio/io_context.hpp"
#include "asio/executor.hpp"
#include "asio/executor_work_guard.hpp"

namespace chops {
namespace net {

/**
 *  @brief Convenience class that combines an executor work guard and a thread,
 *  invoking asynchronous operations as per the Asio documentation.
 *
 *  @note This class is not a necessary dependency of the @c net_ip library, but
 *  is provided for convenience in many use cases.
 */
class worker {
private:
  asio::io_context                                           m_ioc;
  asio::executor_work_guard<asio::io_context::executor_type> m_wg;
  std::thread                                                m_run_thr;

public:
  worker() : m_ioc(), m_wg(asio::make_work_guard(m_ioc)), m_run_thr() { }

/**
 *  @brief Provide access to the @c io_context.
 *
 *  @return Reference to a @c asio::io_context.
 */
  asio::io_context& get_io_context() { return m_ioc; }

/**
 *  @brief Start the thread that invokes the underlying asynchronous
 *  operations.
 */
  void start() {
    m_run_thr = std::thread([this] () {
        try {
          m_ioc.run();
        }
        catch (const std::exception& e) {
          std::cerr << "std::exception caught in worker::start: " << e.what() << std::endl;
        }
        catch (...) {
          std::cerr << "Unknown exception caught in worker::start" << std::endl;
        }
      }
    );
  }

/**
 *  @brief Shutdown the executor and join the thread, abandoning any outstanding operations or handlers.
 */
  void stop() {
    m_ioc.stop();
    m_run_thr.join();
  }

/**
 *  @brief Reset the internal work guard and join the thread, waiting for outstanding operations or 
 *  handlers to complete.
 */
  void reset() {
    m_wg.reset();
    m_run_thr.join();
  }

};


}  // end net namespace
}  // end chops namespace

#endif

