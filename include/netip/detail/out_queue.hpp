/** @file 
 *
 *  @ingroup net_ip_module
 *
 *  @brief Utility class to manage output data queueing.
 *
 *  @note For internal use only.
 *
 *  @author Cliff Green
 *  @date 2017, 2018
 *  @copyright Cliff Green, MIT License
 *
 */

#ifndef OUT_QUEUE_HPP_INCLUDED
#define OUT_QUEUE_HPP_INCLUDED

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/utility.hpp> // noncopyable
#include <boost/optional.hpp>

#include <boost/asio/ip/udp.hpp> // udp::endpoint
#include <boost/asio/ip/tcp.hpp> // tcp::endpoint

#include <boost_adjunct/shared_buffer.hpp> // in sandbox, not official Boost

#include <string>
#include <queue>
#include <cstddef> // std::size_t
#include <limits>  // std::numeric_limits
#include <utility> // std::pair

namespace chops {
namespace detail {

inline void simpleQueueCb(std::size_t, std::size_t, bool) { }

class OutputChannelResource : boost::noncopyable {
public:

  OutputChannelResource() : 
    mSendQueue(),
    mMaxEntries(std::numeric_limits<std::size_t>::max()),
    mMaxBytes(std::numeric_limits<std::size_t>::max()),
    mCurrentBytes(0), mQueueSzCb(simpleQueueCb), mExceeded(false) { }

  virtual ~OutputChannelResource() { }

public:

  // methods overridden by specific IO protocol handlers

  virtual void send (boost_adjunct::shared_const_buffer) = 0;
  // default implementation of the following method for TCP IO handler
  virtual void send (boost_adjunct::shared_const_buffer buf, const boost::asio::ip::udp::endpoint&) {
    send(buf);
  }

  virtual void stop() = 0;

  // all modifications to OutputChannelResource objects are posted in the derived class, since this
  // base class does no event thread posting; protected methods are invoked to do any actual work
  // inside the base class
  virtual void outputQueueSize(OutputQueueSizeCb, std::size_t, std::size_t) = 0;

  // some strangeness with g++ 3.1.2 with the following virtual declarations, requiring workaround;
  // it's probably template name lookup related, since endpoint is a nested typedef in the tcp/udp
  // classes; the API is as desired in OutputChannel, just not as desired in this private ABC
  // virtual boost::asio::ip:tcp::endpoint remoteTcpEndpoint() const { return boost::asio::ip::tcp::endpoint(); }
  // virtual boost::asio::ip:udp::endpoint remoteUdpEndpoint() const { return boost::asio::ip::udp::endpoint(); }
  virtual void remoteTcpEndpoint(boost::asio::ip::tcp::endpoint&) const { } // assumes already empty, so if
  virtual void remoteUdpEndpoint(boost::asio::ip::udp::endpoint&) const { } // not overridden, return empty

protected:

  // assumption: any of these protected methods are invoked within event thread context in the
  // derived output channel classes
  void setOutputQueueSize(OutputQueueSizeCb outQueueSzCb, std::size_t maxEntries, std::size_t maxBytes) {
    mMaxEntries = maxEntries;
    mMaxBytes = maxBytes;
    mQueueSzCb = outQueueSzCb;
  }

  typedef std::pair<boost_adjunct::shared_const_buffer, boost::asio::ip::udp::endpoint> QueueEntry;
  typedef boost::optional<QueueEntry> OptEntry;

  // method called by IO handlers to get another buf of data
  OptEntry nextEntry() {
    if (mSendQueue.empty()) {
      return OptEntry();
    }
    QueueEntry e = mSendQueue.front();
    mSendQueue.pop();
    mCurrentBytes -= e.first.size();
    if (!mExceeded) {
      return OptEntry(e);
    }
    if (mSendQueue.size() <= mMaxEntries && mCurrentBytes <= mMaxBytes) {
      mExceeded = false;
      mQueueSzCb(mSendQueue.size(), mCurrentBytes, mExceeded);
    }
    return OptEntry(e);
  }

  void queueEntry(boost_adjunct::shared_const_buffer buf, const boost::asio::ip::udp::endpoint& endp) {
    mSendQueue.push(std::make_pair(buf, endp));
    mCurrentBytes += buf.size(); // note - possible rollover of num will blow logic
    if (mExceeded) {
      return;
    }
    if (mSendQueue.size() > mMaxEntries || mCurrentBytes > mMaxBytes) {
      mExceeded = true;
      mQueueSzCb(mSendQueue.size(), mCurrentBytes, mExceeded);
    }
  }

  void queueEntry(boost_adjunct::shared_const_buffer buf) {
    queueEntry(buf, boost::asio::ip::udp::endpoint());
  }

private:

  typedef std::queue<QueueEntry> SendQueue;
  SendQueue                      mSendQueue;
  std::size_t                    mMaxEntries;
  std::size_t                    mMaxBytes;
  std::size_t                    mCurrentBytes;
  OutputQueueSizeCb              mQueueSzCb;
  bool                           mExceeded;

};

typedef boost::shared_ptr<OutputChannelResource> OutputChannelResourcePtr;
typedef boost::weak_ptr<OutputChannelResource> OutputChannelResourceWeakPtr;

} // end detail namespace
} // end namespace

#endif

