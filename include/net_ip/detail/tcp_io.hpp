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

namespace chops {
namespace detail {

class tcp_io : std::enable_shared_from_this<tcp_io> {
private:

  std::experimental::net::ip::tcp::socket   m_socket;
  TcpIoErrorCb                   mErrorCb;
  bool                           mStarted;
  bool                           mWriteInProgress;
  boost::asio::ip::tcp::endpoint mRemoteEndp;
public:

  typedef boost::function<void (const boost::system::error_code&, boost::shared_ptr<tcp_io>)> TcpIoErrorCb;

  tcp_io(std::experimental::net::io_context& ioc, TcpIoErrorCb errCb) : 
    OutputChannelResource(),
    mSocket(ios), mErrorCb(errCb), mStarted(false), mWriteInProgress(false), mRemoteEndp() { }

  // started, startRead, getSock only called by protocol handlers, not directly through 
  // Embankment, meaning they are only called while within a posted handler context

  bool started() const { return mStarted; }

  // this method can be called directly from protocol handlers
  void stopIo() {
    mStarted = false;
    mSocket.close();
  }

  boost::asio::ip::tcp::socket& getSock() { return mSocket; }

  // startRead called through TcpConnector and TcpAcceptor objects
  template <class MsgFrame>
  void startRead(const MsgFrame&, const typename IncomingMsgCb<MsgFrame>::Callback&);

public:

  // overrides of OutputChannelResource virtuals

  virtual void send(boost_adjunct::shared_const_buffer buf) {
    mSocket.get_io_service().post(boost::bind(&tcp_io::startWrite, shared_from_this(), buf));
  }

  // this stop can be called through OutputChannel
  virtual void stop() {
    mSocket.get_io_service().post(boost::bind(&tcp_io::notifyHandler, shared_from_this()));
  }

  virtual void outputQueueSize(OutputQueueSizeCb qSzCb, std::size_t maxEntries, std::size_t maxBytes) {
    mSocket.get_io_service().post(boost::bind(&tcp_io::setQueueSize, shared_from_this(),
                                              qSzCb, maxEntries, maxBytes));
  }

  // the TCP remote endpoint is set by the startRead method, which should be called by
  // the protocol handlers before the OutputChannel is made available to the app; this 
  // accessor method should therefore not need concurrency protection
  // original signature (see notes in OutputChannelResource file):
  // boost::asio::ip::tcp::endpoint remoteTcpEndpoint() const { return mRemoteEndp; }
  virtual void remoteTcpEndpoint(boost::asio::ip::tcp::endpoint& endp) const { endp = mRemoteEndp; }

private:

  void notifyHandler() {
    mErrorCb(boost::system::errc::make_error_code(boost::system::errc::operation_canceled), shared_from_this());
  }

  void startWrite(boost_adjunct::shared_const_buffer);

  template <class MsgFrame>
  void handleRead(const boost::system::error_code&, std::size_t, boost::shared_ptr<MsgFrame>,
                  boost::shared_ptr<typename IncomingMsgCb<MsgFrame>::Callback>);

  void handleWrite(const boost::system::error_code&);

  void setQueueSize(OutputQueueSizeCb qSzCb, std::size_t maxEntries, std::size_t maxBytes) {
    setOutputQueueSize(qSzCb, maxEntries, maxBytes);
  }

};

typedef boost::shared_ptr<tcp_io> TcpIoPtr;

// method implementations - could move to an ipp file
// #include TcpIo.ipp

template <class MsgFrame>
void tcp_io::startRead(const MsgFrame& mf, const typename IncomingMsgCb<MsgFrame>::Callback& inCb) {
  mStarted = true;
  mRemoteEndp = mSocket.remote_endpoint();
  // create reference counted MsgFrame and IncomingMsgCb objects, so that copies are no longer
  // made of these objects
  boost::shared_ptr<MsgFrame> mfPtr = boost::shared_ptr<MsgFrame>(new MsgFrame(mf));
  boost::shared_ptr<typename IncomingMsgCb<MsgFrame>::Callback> inCbPtr = 
      boost::shared_ptr<typename IncomingMsgCb<MsgFrame>::Callback>(new typename IncomingMsgCb<MsgFrame>::Callback(inCb));
  socklib::MsgFrameReturnType ret = (*mfPtr)();
  boost::asio::async_read(mSocket, boost::asio::buffer(ret.mBuf),
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
  boost::asio::async_read(mSocket, boost::asio::buffer(ret.mBuf),
                          boost::bind(&tcp_io::handleRead<MsgFrame>, shared_from_this(),
                                      boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred,
                                      mfPtr, inCbPtr));
}

// following method implementations must be inline since they are not
// template member functions

inline void tcp_io::startWrite(boost_adjunct::shared_const_buffer buf) {
  // checkQueueSize();
  if (!mStarted) {
    return; // shutdown happening, most likely, so don't do anything
  }
  if (mWriteInProgress) { // queue buffer
    queueEntry(buf);
    return;
  }
  mWriteInProgress = true;
  boost::asio::async_write(mSocket, buf, boost::bind(&tcp_io::handleWrite, shared_from_this(),
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
  boost::asio::async_write(mSocket, e->first, boost::bind(&tcp_io::handleWrite, shared_from_this(),
                                                         boost::asio::placeholders::error));
}

} // end detail namespace
} // end namespace

#endif

