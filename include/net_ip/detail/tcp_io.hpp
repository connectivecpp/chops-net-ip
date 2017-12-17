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
#include <experimental/buffer>

#include <memory> // std::shared_ptr, std::enable_shared_from_this
#include <atomic>
#include <system_error>

#include <cstddef> // std::size_t
#include <utility> // std::forward

#include <string_view>

#include "net_ip/queue_stats.hpp"
#include "net_ip/net_ip_error.hpp"
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
  std::experimental::net::ip::tcp::endpoint m_remote_endp;
  entity_ptr                                m_entity_ptr; // back reference ptr for notification to creator
  std::atomic_bool                          m_started;
  bool                                      m_write_in_progress; // internal only, doesn't need to be atomic
  chops::net::detail::output_queue          m_outq;

public:

  tcp_io(std::experimental::net::io_context& ioc, entity_ptr ep) noexcept : 
    m_socket(ioc), m_remote_endp(), m_entity_ptr(ep), 
    m_started(false), m_write_in_progress(false), m_outq() { }

  std::experimental::net::ip::tcp::socket& get_socket() noexcept { return m_socket; }

  chops::net::output_queue_stats get_output_queue_stats() const noexcept { return m_outq.get_queue_stats(); }

  bool is_started() const noexcept { return m_started; }

  // an is_started check is performed in the @c io_interface object
  template <typename MH, typename MF>
  void start_io(MH && msg_handler, MF&& msg_frame, std::size_t header_read_size);

  template <typename MH>
  void start_io(MH&& msg_handler, std::string_view delimiter);

  void start_io();

  // this method can be called through the @c io_interface object
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

  bool start_io_setup();

  template <typename MH, typename MF>
  void handle_read(MH&&, MF&&, chops::utility::mutable_shared_buffer, std::size_t, const std::error_code& err, std::size_t);

  template <typename MH>
  void handle_read(MH&&, chops::utility::mutable_shared_buffer, std::string_view, const std::error_code& err, std::size_t);

  void startWrite(boost_adjunct::shared_const_buffer);

  template <class MsgFrame>
  void handleRead(const boost::system::error_code&, std::size_t, std::shared_ptr<MsgFrame>,
                  std::shared_ptr<typename IncomingMsgCb<MsgFrame>::Callback>);

  void handleWrite(const boost::system::error_code&);

};

// method implementations, just to make the class declaration a little more readable

template <typename ET>
bool tcp_io<ET>::start_io_setup() {
  m_started = true;
  std::error_code ec;
  m_remote_endp = m_socket.remote_endpoint(ec);
  if (ec) {
    m_entity_ptr->io_handler_notification(ec, std::shared_from_this());
    return false;
  }
  return true;
}

template <typename ET>
template <typename MH, typename MF>
void tcp_io<ET>::start_io(MH&& msg_hdlr, MF&& msg_frame, std::size_t header_read_size) {
  if (!start_io_setup()) {
    return;
  }
  chops::utility::mutable_shared_buffer buf(header_read_size);
  boost::asio::async_read(m_socket, std::experimental::net::mutable_buffer(buf.data(), buf.size()),
    [this, buf, mh = std::forward(msg_hdlr), mf = std::forward(msg_frame), header_read_size]
          (const std::error_code& err, std::size_t nb) {
      handle_read(mh, mf, buf, hrs, err, nb);
    }
  );
}

template <typename ET>
template <typename MH, typename MF>
void tcp_io<ET>::handle_read(MH&& msg_hdlr, MF&& msg_frame, chops::utility::mutable_shared_buffer buf, 
                 std::size_t header_read_size, const std::error_code& err, std::size_t num_bytes) {
  if (err) {
    // socket cancel errors as well as other errors
    m_entity_ptr->io_handler_notification(err, std::shared_from_this());
    return;
  }
  // assert num_bytes == buf.size()
  std::experimental::net::mutable_buffer mbuf;
  std::size_t next_read_size = msg_frame(buf);
  if (next_read_size == 0) { // msg fully received, now invoke message handler
    if (!msg_hdlr(buf, OutputChannel(std::weak_from_this()), m_remote_endp, msg_frame)) {
      // message handler not happy, tear everything down
      m_entity_ptr->io_handler_notification(std::make_error_code(chops::net::message_handler_terminated)
                                            std::shared_from_this());
      return;
    }
    // buf may be empty if moved from
    buf.resize(header_read_size);
    mbuf = std::experimental::net::mutable_buffer(buf.data(), buf.size());
  }
  else {
    std::size_t old_size = buf.size();
    buf.resize(buf.size() + next_read_size);
    mbuf = std::experimental::net::mutable_buffer(buf.data() + old_size, next_read_size);
  }
  // start another read, using the buffer supplied in the MsgFrame return val
  boost::asio::async_read(m_socket, mbuf,
    [this, buf, mh = std::forward(msg_hdlr), mf = std::forward(msg_frame), header_read_size]
          (const std::error_code& err, std::size_t nb) {
      handle_read(mh, mf, buf, header_read_size, err, nb);
    }
  );
}

template <typename ET>
template <typename MH>
void tcp_io<ET>::start_io(MH&& msg_hdlr, std::string_view delimiter) {
  if (!start_io_setup()) {
    return;
  }
  boost::asio::async_read_until(m_socket, chops::utility::mutable_shared_buffer(), delimiter,
    [this, buf, mh = std::forward(msg_hdlr), delimiter] (const std::error_code& err, std::size_t nb) {
      handle_read(mh, buf, delimiter, err, nb);
    }
  );
}

template <typename ET>
template <typename MH>
void tcp_io<ET>::handle_read(MH&& msg_hdlr, chops::utility::mutable_shared_buffer buf, std::string_view delimiter,
                 const std::error_code& err, std::size_t num_bytes) {
  if (err) {
    // socket cancel errors as well as other errors
    m_entity_ptr->io_handler_notification(err, std::shared_from_this());
    return;
  }
  if (!msg_hdlr(buf, OutputChannel(std::weak_from_this()), m_remote_endp)) {
    m_entity_ptr->io_handler_notification(std::make_error_code(chops::net::message_handler_terminated)
                                          std::shared_from_this());
    return;
  }
  // start another read, using the buffer supplied in the MsgFrame return val
  boost::asio::async_read_until(m_socket, something, delimiter,
    [this, buf, mh = std::forward(msg_hdlr), delimiter] (const std::error_code& err, std::size_t nb) {
      handle_read(mh, buf, delimiter, err, nb);
    }
  );
}

template <typename ET>
void tcp_io<ET>::start_io() {
  if (!start_io_setup()) {
    return;
  }
  but here
  boost::asio::async_read(m_socket, std::experimental::net::mutable_buffer(&b, 1),
    [this, buf, mh = std::forward(msg_hdlr), delimiter] (const std::error_code& err, std::size_t nb) {
      handle_read(mh, buf, delimiter, err, nb);
    }
  );
}

template <typename ET>
void tcp_io<ET>::startWrite(boost_adjunct::shared_const_buffer buf) {
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

template <typename ET>
void tcp_io<ET>::handleWrite(const boost::system::error_code& err) {
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

