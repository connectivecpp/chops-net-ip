/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Internal handler class for TCP stream input and output.
 *
 *  @note For internal use only.
 *
 *  @author Cliff Green
 *  @date 2017, 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef TCP_IO_HPP_INCLUDED
#define TCP_IO_HPP_INCLUDED

#include <experimental/io_context>
#include <experimental/internet>

#include <memory> // std::shared_ptr, std::enable_shared_from_this
#include <atomic>
#include <system_error>

#include <cstddef> // std::size_t
#include <utility> // std::forward

#include "net_ip/queue_stats.hpp"
#include "utility/shared_buffer.hpp"

namespace chops {
namespace net {
namespace detail {

template <typename ET>
class tcp_io : std::enable_shared_from_this<tcp_io> {
private:
  using entity_ptr = std::shared_ptr<ET>;

private:

  std::experimental::net::ip::tcp::socket   m_socket;
  entity_ptr                                m_entity_ptr; // back reference ptr for notification to creator
  std::atomic_bool                          m_started;
  bool                                      m_write_in_progress; // internal only, doesn't need to be atomic
  chops::net::detail::output_queue          m_outq;

public:

  tcp_io(std::experimental::net::io_context& ioc, entity_ptr ep) noexcept : 
    m_socket(ioc), m_entity_ptr(ep), m_started(false), m_write_in_progress(false), m_outq() { }

  std::experimental::net::ip::tcp::socket& get_socket() noexcept { return m_socket; }

  chops::net::output_queue_stats get_output_queue_stats() const noexcept { return m_outq.get_queue_stats(); }

  bool is_started() const noexcept { return m_started; }

  // an is_started check is performed in the io interface object
  template <typename MF, typename MH>
  void start_io(MF && msg_frame, MH&& msg_handler, std::size_t initial_size);

  void stop_io();
// fix this, not correct
  void stop() {
    m_started = false;
    m_socket.close();
  }

  void send(boost_adjunct::shared_const_buffer buf) {
    m_socket.get_io_service().post(boost::bind(&tcp_io::startWrite, shared_from_this(), buf));
  }

  // this stop can be called through OutputChannel
  virtual void stop() {
    m_socket.get_io_service().post(boost::bind(&tcp_io::notifyHandler, shared_from_this()));
  }

private:

  void notifyHandler() {
    mErrorCb(boost::system::errc::make_error_code(boost::system::errc::operation_canceled), shared_from_this());
  }

  void startWrite(boost_adjunct::shared_const_buffer);

  template <class MsgFrame>
  void handleRead(const boost::system::error_code&, std::size_t, std::shared_ptr<MsgFrame>,
                  std::shared_ptr<typename IncomingMsgCb<MsgFrame>::Callback>);

  void handleWrite(const boost::system::error_code&);

};

// method implementations, just to make the header a little more readable

template <typename MF, typename MH>
void start_io(MF&& msg_frame, MH&& msg_hdlr, std::size_t initial_size) {
  m_started = true;
  chops::utility::mutable_shared_buffer buf(initial_size);
  boost::asio::async_read(m_socket, buf, 
    [this, buf, mf = std::forward(msg_frame), mh = std::forward(msg_hdlr)] (const std::error_code& err, std::size_t nb) {
      handle_read(mf, mh, buf, err, nb);
    }
  );
}

template <typename MF, typename MH>
void handle_read(MF&& msg_frame, MH&& msg_hdlr, chops::utility::mutable_shared_buffer buf, 
                 const std::error_code& err, std::size_t num_bytes) {
  if (err) { // socket cancels as well as other errors
    m_entity_ptr->io_handler_notification(err, std::shared_from_this());
    return; // done, no more reads hanging
  }
  // assert nb == buf.size()
  std::size_t next_num_bytes = (*mf_ptr)(buf);
  if (next_num_bytes == 0) { // msg fully received, now invoke message handler
    if (!(*mh_tr)(*mfPtr, OutputChannel(shared_from_this()))) { // IncomingMsgCb not happy, tear connection down
      mErrorCb(boost::system::errc::make_error_code(boost::system::errc::operation_canceled), shared_from_this());
      return;
    }
  }
  // start another read, using the buffer supplied in the MsgFrame return val
  boost::asio::async_read(m_socket, boost::asio::buffer(ret.mBuf),
                          boost::bind(&tcp_io::handleRead<MsgFrame>, shared_from_this(),
                                      boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred,
                                      mfPtr, inCbPtr));
}

// following method implementations must be inline since they are not
// template member functions

inline void tcp_io::startWrite(boost_adjunct::shared_const_buffer buf) {
  if (!mStarted) {
    return; // shutdown happening, most likely, so don't do anything
  }
  if (mWriteInProgress) { // queue buffer
    queueEntry(buf);
    return;
  }
  mWriteInProgress = true;
  boost::asio::async_write(m_socket, buf, boost::bind(&tcp_io::handleWrite, shared_from_this(),
                                                     boost::asio::placeholders::error));
}

inline void tcp_io::handleWrite(const boost::system::error_code& err) {
  if (err || !mStarted) { // err or shutting down
    // invoke errorCb only through read errors - a write error should result in a corresponding
    // read error in the handleRead method
    return;
  }
  OutputChannelResource::OptEntry e = nextEntry();
  if (!e) { // queue empty, no more writes needed for now
    mWriteInProgress = false;
    return;
  }
  mWriteInProgress = true;
  boost::asio::async_write(m_socket, e->first, boost::bind(&tcp_io::handleWrite, shared_from_this(),
                                                         boost::asio::placeholders::error));
}

} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

