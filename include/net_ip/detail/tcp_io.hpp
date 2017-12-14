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

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/placeholders.hpp>

#include <boost/bind.hpp>

#include <boost/function.hpp>

#include <boost/system/error_code.hpp>

#include <boost_adjunct/shared_buffer.hpp> // in sandbox

#include <cstddef> // std::size_t





#include <experimental/io_context>
#include <experimental/internet>

#include <memory> // std::enable_shared_from_this
#include <atomic>

#include "net_ip/queue_stats.hpp"

namespace chops {
namespace net {
namespace detail {

class tcp_io : std::enable_shared_from_this<tcp_io> {
private:

  std::experimental::net::ip::tcp::socket   m_socket;
  std::experimental::net::ip::tcp::endpoint m_remote_endp;
  std::atomic_bool                          m_started;
  bool                                      m_write_in_progress; // internal only, doesn't need to be atomic
  chops::net::detail::output_queue          m_outq;

  TcpIoErrorCb                   mErrorCb;

public:

  tcp_io(std::experimental::net::io_context& ioc) : 
    m_socket(ioc), m_remote_endp(), m_started(false), m_write_in_progress(false), m_outq() { }

  std::experimental::net::ip::tcp::socket& get_socket() noexcept { return m_socket; }

  chops::net::output_queue_stats get_output_queue_stats() const noexcept { return m_outq.get_queue_stats(); }

  bool is_started() const noexcept { return m_started; }

  template <class MsgFrame>
  void start(const MsgFrame&, const typename IncomingMsgCb<MsgFrame>::Callback&);

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

  // original signature (see notes in OutputChannelResource file):
  // boost::asio::ip::tcp::endpoint remoteTcpEndpoint() const { return m_remote_endp; }
  virtual void remoteTcpEndpoint(boost::asio::ip::tcp::endpoint& endp) const { endp = m_remote_endp; }

private:

  void notifyHandler() {
    mErrorCb(boost::system::errc::make_error_code(boost::system::errc::operation_canceled), shared_from_this());
  }

  void startWrite(boost_adjunct::shared_const_buffer);

  template <class MsgFrame>
  void handleRead(const boost::system::error_code&, std::size_t, boost::shared_ptr<MsgFrame>,
                  boost::shared_ptr<typename IncomingMsgCb<MsgFrame>::Callback>);

  void handleWrite(const boost::system::error_code&);

};

typedef boost::shared_ptr<tcp_io> TcpIoPtr;

// method implementations - could move to an ipp file
// #include TcpIo.ipp

template <class MsgFrame>
void tcp_io::startRead(const MsgFrame& mf, const typename IncomingMsgCb<MsgFrame>::Callback& inCb) {
  mStarted = true;
  m_remote_endp = m_socket.remote_endpoint();
  // create reference counted MsgFrame and IncomingMsgCb objects, so that copies are no longer
  // made of these objects
  boost::shared_ptr<MsgFrame> mfPtr = boost::shared_ptr<MsgFrame>(new MsgFrame(mf));
  boost::shared_ptr<typename IncomingMsgCb<MsgFrame>::Callback> inCbPtr = 
      boost::shared_ptr<typename IncomingMsgCb<MsgFrame>::Callback>(new typename IncomingMsgCb<MsgFrame>::Callback(inCb));
  socklib::MsgFrameReturnType ret = (*mfPtr)();
  boost::asio::async_read(m_socket, boost::asio::buffer(ret.mBuf),
                          boost::bind(&tcp_io::handleRead<MsgFrame>, shared_from_this(),
                                      boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred,
                                      mfPtr, inCbPtr));
}

template <class MsgFrame>
void tcp_io::handleRead(const boost::system::error_code& err, std::size_t nb,
                       boost::shared_ptr<MsgFrame> mfPtr, 
                       boost::shared_ptr<typename IncomingMsgCb<MsgFrame>::Callback> inCbPtr) {
  if (err) { // can get here from socket cancel
    if (err.value() != boost::asio::error::operation_aborted) {
      mErrorCb(err, shared_from_this()); // notify protocol handler
    }
    return;
  }
  MsgFrameReturnType ret = (*mfPtr)();
  if (ret.mDone) { // message ready, invoke user supplied incoming msg cb
    if (!(*inCbPtr)(*mfPtr, OutputChannel(shared_from_this()))) { // IncomingMsgCb not happy, tear connection down
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

