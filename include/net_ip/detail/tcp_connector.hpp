/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Classes for TCP connectors and acceptors in @c SockLib library. 
 *
 *  These classes implement TCP connector and TCP acceptor functionality, with apps
 *  interacting indirectly through @c Embankment objects. The @c startHandler
 *  methods are called (indirectly) through @c SockLibResource methods.
 *
 *  Object lifetimes, destruction, and graceful shutdown require careful attention.
 *  Note that a @c TcpConnector or @c TcpAcceptor is never directly destructed, only
 *  indirectly destructed when a @c SockLib object is destroyed. However, the @c start
 *  and @c stop methods through an @c Embankment may be called multiple times, and
 *  internal @c TcpIo objects are created and destroyed as appropriate.
 *
 *  Most of the object lifetime and shutdown issues are related to @c TcpConnector
 *  and @c TcpAcceptor management of @c TcpIo objects. @c TcpIo objects can't be
 *  left "free floating" since they may be in a perfectly good state (waiting for
 *  input or writing output), and app code needs to disconnect and / or shut them
 *  down (in other words, the data flow state does not directly determine the lifetime
 *  of a @c TcpIo object).
 *
 *  There are two logic paths for destroying a @c TcpIo object - either internally
 *  as an error or @c false return from an @c IncomingMsgCb, or externally through
 *  a @c stop method call (e.g. from an @c Embankment).
 *  
 *  Both of these logic paths result in a destruction handler being posted to the 
 *  @c io_service object. This simplifies the destruction, particularly when needing
 *  to destroy the @c TcpIo object from inside a callback (which can happen due to
 *  an error, or a @c false return from the @c IncomingMsgCb, or from a call
 *  to an @c Embankment @c stop method from a reference stored inside an 
 *  @c IncomingMsgCb function object).
 *
 *  Another benefit of posting destruction handlers for later execution is that it
 *  simplifies threading issues, where an app thread invokes the destruction, but
 *  events can occur inside the io_service thread. By posting (instead of direct
 *  execution), all changes happen inside the io_service thread.
 *
 *  @c TcpIo errors are reported through error handling callbacks in @c TcpConnector and
 *  @c TcpAcceptor. 
 *
 *  @note For internal use only.
 *
 *  @author Cliff Green
 *  @date 2010
 *
 */

#ifndef TCP_CONNECTOR_HPP_INCLUDED
#define TCP_CONNECTOR_HPP_INCLUDED

#include "Socket/OutputChannel.h"
#include "Socket/detail/SockLibResource.h"

#include "Socket/detail/TcpIo.h"

#include <boost/enable_shared_from_this.hpp>
#include <boost/scoped_ptr.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#include <boost/system/error_code.hpp>

#include <utility> // for std::pair
#include <list>
#include <cstddef> // for std::size_t
// #include <cassert>

namespace chops {
namespace net {
namespace detail {

// classes and handlers for interacting with Asio

// may need to add enable_shared_from_this when destruct capabilities added
class TcpConnector : public SockLibResource, public boost::enable_shared_from_this<TcpConnector> {
public:
  TcpConnector(boost::asio::io_service& io_service, 
               const boost::asio::ip::tcp::endpoint& endp,
               std::size_t reconnTimeMillis,
               bool noDelay = false) :
      SockLibResource(), mIoService(io_service), mEndpoint(endp), mReconnTimer(io_service), 
      mReconnTimeMillis(reconnTimeMillis), mIoHandler(), mCcCb(), mNoDelay(noDelay), mStarted(false)
      { SockLibResource::setDerived(this); }

public:
  // overrides of SockLibResource virtuals
  void stopHandler() {
    mIoService.post(boost::bind(&TcpConnector::stopProcessing, shared_from_this()));
  }

public:
  // methods called by SockLibResource variant
  template <class MsgFrame>
  void startHandler(const ChannelChangeCb& ccCb, const MsgFrame& mf, 
                    const typename IncomingMsgCb<MsgFrame>::Callback& inCb) {
    mIoService.post(boost::bind(&TcpConnector::startProcessing<MsgFrame>, shared_from_this(), ccCb, mf, inCb));
  }

private:

  template <class MsgFrame>
  void startProcessing(ChannelChangeCb, MsgFrame, typename IncomingMsgCb<MsgFrame>::Callback);

  void stopProcessing() {
    mStarted = false;
    stopIoHandler(boost::system::errc::make_error_code(boost::system::errc::operation_canceled), mIoHandler);
  }

  template <class MsgFrame>
  void startConnect(const MsgFrame&, const typename IncomingMsgCb<MsgFrame>::Callback&);

  void stopIoHandler(boost::system::error_code, TcpIoPtr);

  void errorCb(const boost::system::error_code&, TcpIoPtr);

  template <class MsgFrame>
  void handleConnect(const boost::system::error_code&, MsgFrame mf, typename IncomingMsgCb<MsgFrame>::Callback inCb);
  template <class MsgFrame>
  void handleTimeout(const boost::system::error_code&, MsgFrame mf, typename IncomingMsgCb<MsgFrame>::Callback inCb);

private:
  boost::asio::io_service&       mIoService;
  boost::asio::ip::tcp::endpoint mEndpoint;
  boost::asio::deadline_timer    mReconnTimer;
  std::size_t                    mReconnTimeMillis;
  TcpIoPtr                       mIoHandler;
  ChannelChangeCb                mCcCb;
  bool                           mNoDelay;
  bool                           mStarted;

};

template <class MsgFrame>
void TcpConnector::startProcessing(ChannelChangeCb ccCb, MsgFrame mf, typename IncomingMsgCb<MsgFrame>::Callback inCb) {
  mStarted = true;
  mCcCb = ccCb;
  mCcCb(OutputChannel(), boost::system::error_code(), 0); // seed any app state machines
  startConnect(mf, inCb);
}

template <class MsgFrame>
void TcpConnector::startConnect(const MsgFrame& mf, 
                                const typename IncomingMsgCb<MsgFrame>::Callback& inCb) {
  if (!mStarted) {
    return;
  }
  mIoHandler.reset(new TcpIo(mIoService, boost::bind(&TcpConnector::errorCb, shared_from_this(), _1, _2)));
  mIoHandler->getSock().async_connect(mEndpoint, boost::bind(&TcpConnector::handleConnect<MsgFrame>, shared_from_this(),
                                      boost::asio::placeholders::error, mf, inCb));
  if (mNoDelay) {
    boost::asio::ip::tcp::no_delay option(true);
    mIoHandler->getSock().set_option(option);
  }

}

// TcpConnector method implementations - could go into an ipp file such as
// #include TcpResource.ipp

template <class MsgFrame>
void TcpConnector::handleConnect(const boost::system::error_code& err, MsgFrame mf, 
                                 typename IncomingMsgCb<MsgFrame>::Callback inCb) {
  if (!mStarted || (err && err.value() == boost::asio::error::operation_aborted)) {
    return;
  }
  if (err) {
    // connect failed, notify app through cb, start timer
    mCcCb(OutputChannel(), err, 0);
    if (mReconnTimeMillis != 0) {
      mReconnTimer.expires_from_now(boost::posix_time::time_duration(boost::posix_time::milliseconds(mReconnTimeMillis)));
      mReconnTimer.async_wait(boost::bind(&TcpConnector::handleTimeout<MsgFrame>, shared_from_this(),
                              boost::asio::placeholders::error, mf, inCb));
    }
    return;
  }
  mReconnTimer.cancel(); // connect successful, cancel timer
  mIoHandler->startRead(mf, inCb); // start read processing on the socket
  // notify app through cb
  if (!mCcCb(OutputChannel(mIoHandler), boost::system::error_code(), 1)) {
    // app didn't like something, cancel read, don't restart timer
    stopIoHandler(boost::system::errc::make_error_code(boost::system::errc::operation_canceled), mIoHandler);
    return;
  }
}

template <class MsgFrame>
void TcpConnector::handleTimeout(const boost::system::error_code& err, MsgFrame mf, 
                                 typename IncomingMsgCb<MsgFrame>::Callback inCb) {
  if (err) {
    return;
  }
  startConnect(mf, inCb);
}

// next two methods are inline since not template member functions

inline void TcpConnector::errorCb (const boost::system::error_code& err, TcpIoPtr ioh) {
  // assert(ioh == mIoHandler);
  // if(ioh == mIoHandler){
    mIoService.post(boost::bind(&TcpConnector::stopIoHandler, shared_from_this(), err, mIoHandler));
  // }
}

inline void TcpConnector::stopIoHandler(boost::system::error_code err, TcpIoPtr ioh) {
  mCcCb(OutputChannel(ioh), err, 0);
  // stop io handler, closing the socket and canceling operations; io handler will eventually
  // go out of scope and be destructed
  if (ioh) {
    ioh->stopIo();
  }
  mReconnTimer.cancel();
}


} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

