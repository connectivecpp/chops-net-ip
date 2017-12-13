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

#ifndef TCP_ACCEPTOR_HPP_INCLUDED
#define TCP_ACCEPTOR_HPP_INCLUDED

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

class TcpAcceptor : public SockLibResource, public boost::enable_shared_from_this<TcpAcceptor> {
public:
  TcpAcceptor(boost::asio::io_service& io_service, const boost::asio::ip::tcp::endpoint& endp, bool noDelay = false) :
      SockLibResource(), mIoService(io_service), mEndp(endp), mAcceptor(), 
      mCcCb(), mHandlers(), mNoDelay(noDelay), mStarted(false)
      { SockLibResource::setDerived(this); }

public:
  // overrides of SockLibResource virtuals
  void stopHandler() {
    mIoService.post(boost::bind(&TcpAcceptor::stopProcessing, shared_from_this()));
  }

public:
  template <class MsgFrame>
  void startHandler(const ChannelChangeCb& ccCb, const MsgFrame& mf,
                    const typename IncomingMsgCb<MsgFrame>::Callback& inCb) {
    mIoService.post(boost::bind(&TcpAcceptor::startProcessing<MsgFrame>, shared_from_this(), ccCb, mf, inCb));
  }

private:

  template <class MsgFrame>
  void startProcessing(ChannelChangeCb, MsgFrame, typename IncomingMsgCb<MsgFrame>::Callback);

  void stopProcessing() {
    mStarted = false;
    removeAllIoHandlers();
  }

  std::size_t channelCount() const { return mHandlers.size(); }

  void removeAllIoHandlers();

  void removeIoHandler(boost::system::error_code, TcpIoPtr);

  void errorCb(const boost::system::error_code&, TcpIoPtr);

  template <class MsgFrame>
  void handleAccept(const boost::system::error_code&, MsgFrame, 
                    typename IncomingMsgCb<MsgFrame>::Callback, TcpIoPtr);


private:
  boost::asio::io_service&       mIoService;
  boost::asio::ip::tcp::endpoint mEndp;
  boost::scoped_ptr<boost::asio::ip::tcp::acceptor> mAcceptor;
  ChannelChangeCb               mCcCb;
  std::list<TcpIoPtr>           mHandlers;
  bool                          mNoDelay;
  bool                          mStarted;

};

// TcpAcceptor method implementations - see note about TcpConnector method implementations

template <class MsgFrame>
void TcpAcceptor::startProcessing(ChannelChangeCb ccCb, MsgFrame mf, typename IncomingMsgCb<MsgFrame>::Callback inCb) {
  mStarted = true;
  mAcceptor.reset(new boost::asio::ip::tcp::acceptor(mIoService, mEndp));
  mCcCb = ccCb;
  mCcCb(OutputChannel(), boost::system::error_code(), 0); // seed any app state machines
  // initiate listens
  TcpIoPtr ioh = TcpIoPtr(new TcpIo(mIoService, boost::bind(&TcpAcceptor::errorCb, shared_from_this(), _1, _2)));
  mAcceptor->async_accept(ioh->getSock(), boost::bind(&TcpAcceptor::handleAccept<MsgFrame>, shared_from_this(),
                        boost::asio::placeholders::error, mf, inCb, ioh));
}

template <class MsgFrame>
void TcpAcceptor::handleAccept(const boost::system::error_code& err, MsgFrame mf, 
                  typename IncomingMsgCb<MsgFrame>::Callback inCb, TcpIoPtr ioh) {

  if (!mStarted || (err && err.value() == boost::asio::error::operation_aborted)) {
    return;
  }
  if (err) {
    // notify app, let io handler go out of scope, don't start another accept with assumption
    // something really bad has happened
    mCcCb(OutputChannel(), err, channelCount());
    return;
  }
  mHandlers.push_back(ioh);
  ioh->startRead(mf, inCb); // start read processing on the socket
  if (!mCcCb(OutputChannel(ioh), boost::system::error_code(), channelCount())) {
    // app didn't like something, cancel read processing on ioh, remove ioh from container
    removeIoHandler(boost::system::errc::make_error_code(boost::system::errc::operation_canceled), ioh);
  }
  if (mNoDelay) {
    boost::asio::ip::tcp::no_delay option(true);
    ioh->getSock().set_option(option);
  }
  TcpIoPtr newIoh = TcpIoPtr(new TcpIo(mIoService, boost::bind(&TcpAcceptor::errorCb, shared_from_this(), _1, _2)));
  mAcceptor->async_accept(newIoh->getSock(), boost::bind(&TcpAcceptor::handleAccept<MsgFrame>, shared_from_this(),
                         boost::asio::placeholders::error, mf, inCb, newIoh));
}

inline void TcpAcceptor::errorCb(const boost::system::error_code& err, TcpIoPtr ioh) {
  removeIoHandler(err, ioh);
}

inline void TcpAcceptor::removeAllIoHandlers() {
  while (channelCount() > 0) {
    removeIoHandler(boost::system::errc::make_error_code(boost::system::errc::operation_canceled), mHandlers.front());
  }
  mAcceptor->close();
  mAcceptor.reset(); // delete ptr
  mCcCb(OutputChannel(), boost::system::errc::make_error_code(boost::system::errc::operation_canceled), 0);
}

inline void TcpAcceptor::removeIoHandler(boost::system::error_code err, TcpIoPtr ioh) {
  ioh->stopIo(); // stop Io handler processing - eventually object will be destructed
  mHandlers.remove(ioh); // remove TcpIo handler from container
  mCcCb(OutputChannel(ioh), err, channelCount()); // notify app
}

} // end detail namespace
} // end net namespace
} // end chops namespace


#endif

