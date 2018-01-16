/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief TCP connector class, for internal use.
 *
 *  @note For internal use only.
 *
 *  @author Cliff Green
 *  @date 2018
 *
 */

#ifndef TCP_CONNECTOR_HPP_INCLUDED
#define TCP_CONNECTOR_HPP_INCLUDED

#include <experimental/internet>
#include <experimental/socket>

#include <system_error>
#include <memory>

#include <cstddef> // for std::size_t

#include "net_ip/detail/tcp_io.hpp"
#include "net_ip/detail/net_entity_base.hpp"
#include "timer/periodic_timer.hpp"

namespace chops {
namespace net {
namespace detail {

class tcp_connector : public std::enable_shared_from_this<tcp_connector> {
private:
  net_entity_base<tcp_io>                 m_entity_base;
  std::experiment::net::ip::tcp::socket   m_socket;
  chops::periodic_timer<>                 m_timer;
  std::size_t                             m_reconn_time;

public:
  template <typename Iter>
  tcp_connector(std::experimental::net::io_context& ioc, 
                Iter beg, Iter end, std::size_t reconn_time) :
      m_entity_base(ioc, beg, end);
      m_socket(ioc),
      m_timer(ioc),
      m_reconn_time(reconn_time)
    { }

  tcp_connector(std::experimental::net::io_context& ioc,
                std::string_view remote_port, std::string_view remote_host, 
                std::size_t reconn_time_millis) :
      m_entity_base(ioc, remote_port, remote_host);
      m_socket(ioc),
      m_timer(ioc),
      m_reconn_time(reconn_time)
    { }


public:
  // methods called by SockLibResource variant
  template <class MsgFrame>
  void startHandler(const ChannelChangeCb& ccCb, const MsgFrame& mf, 
                    const typename IncomingMsgCb<MsgFrame>::Callback& inCb) {
    mIoService.post(boost::bind(&tcp_connector::startProcessing<MsgFrame>, shared_from_this(), ccCb, mf, inCb));
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

};

template <class MsgFrame>
void tcp_connector::startProcessing(ChannelChangeCb ccCb, MsgFrame mf, typename IncomingMsgCb<MsgFrame>::Callback inCb) {
  mStarted = true;
  mCcCb = ccCb;
  mCcCb(OutputChannel(), boost::system::error_code(), 0); // seed any app state machines
  startConnect(mf, inCb);
}

template <class MsgFrame>
void tcp_connector::startConnect(const MsgFrame& mf, 
                                const typename IncomingMsgCb<MsgFrame>::Callback& inCb) {
  if (!mStarted) {
    return;
  }
  mIoHandler.reset(new TcpIo(mIoService, boost::bind(&tcp_connector::errorCb, shared_from_this(), _1, _2)));
  mIoHandler->getSock().async_connect(mEndpoint, boost::bind(&tcp_connector::handleConnect<MsgFrame>, shared_from_this(),
                                      boost::asio::placeholders::error, mf, inCb));
  if (mNoDelay) {
    boost::asio::ip::tcp::no_delay option(true);
    mIoHandler->getSock().set_option(option);
  }

}

// tcp_connector method implementations - could go into an ipp file such as
// #include TcpResource.ipp

template <class MsgFrame>
void tcp_connector::handleConnect(const boost::system::error_code& err, MsgFrame mf, 
                                 typename IncomingMsgCb<MsgFrame>::Callback inCb) {
  if (!mStarted || (err && err.value() == boost::asio::error::operation_aborted)) {
    return;
  }
  if (err) {
    // connect failed, notify app through cb, start timer
    mCcCb(OutputChannel(), err, 0);
    if (mReconnTimeMillis != 0) {
      mReconnTimer.expires_from_now(boost::posix_time::time_duration(boost::posix_time::milliseconds(mReconnTimeMillis)));
      mReconnTimer.async_wait(boost::bind(&tcp_connector::handleTimeout<MsgFrame>, shared_from_this(),
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
void tcp_connector::handleTimeout(const boost::system::error_code& err, MsgFrame mf, 
                                 typename IncomingMsgCb<MsgFrame>::Callback inCb) {
  if (err) {
    return;
  }
  startConnect(mf, inCb);
}

// next two methods are inline since not template member functions

inline void tcp_connector::errorCb (const boost::system::error_code& err, TcpIoPtr ioh) {
  // assert(ioh == mIoHandler);
  // if(ioh == mIoHandler){
    mIoService.post(boost::bind(&tcp_connector::stopIoHandler, shared_from_this(), err, mIoHandler));
  // }
}

inline void tcp_connector::stopIoHandler(boost::system::error_code err, TcpIoPtr ioh) {
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

