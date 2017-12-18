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
#include <string>

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

  template <typename MH, typename MF>
  void start_io(MH && msg_handler, MF&& msg_frame, std::size_t header_size);

  template <typename MH>
  void start_io(MH&& msg_handler, const std::string& delimiter);

  void start_io();

  // this method can be called through the @c io_interface object
  void stop_io();

// fix this, not correct
  void stop() {
    m_started = false;
    m_socket.close();
  }

  void send(chops::shared_const_buffer buf) {
    m_socket.get_io_service().post(boost::bind(&tcp_io::start_write, shared_from_this(), buf));
  }

  // this stop can be called through OutputChannel
  virtual void stop() {
    m_socket.get_io_service().post(boost::bind(&tcp_io::notifyHandler, shared_from_this()));
  }

private:

  bool process_err_code(const std::error_code&);

  bool start_io_setup();

  template <typename MH, typename MF>
  void handle_read(MH&&, MF&&, chops::mutable_shared_buffer, std::size_t, const std::error_code& err, std::size_t);

  template <typename MH>
  void handle_read(MH&&, chops::mutable_shared_buffer, const std::string& delimiter, const std::error_code& err, std::size_t);

  void handle_read(chops::mutable_shared_buffer, const std::error_code& err, std::size_t);

  void start_write(chops::shared_const_buffer);

  void handleWrite(const std::error_code&);

};

// method implementations, just to make the class declaration a little more readable

template <typename ET>
bool tcp_io<ET>::process_err_code(const std::error_code& err) {
  if (err) {
    m_started = false;
    m_entity_ptr->io_handler_notification(err, std::shared_from_this());
    return false;
  }
  return true;
}

template <typename ET>
bool tcp_io<ET>::start_io_setup() {
  if (m_started) {
    return false;
  }
  m_started = true;
  std::error_code ec;
  m_remote_endp = m_socket.remote_endpoint(ec);
  return process_err_code(ec);
}

template <typename ET>
template <typename MH, typename MF>
void tcp_io<ET>::start_io(MH&& msg_hdlr, MF&& msg_frame, std::size_t header_size) {
  if (!start_io_setup()) {
    return;
  }
  chops::mutable_shared_buffer buf(header_size);
  std::experimental::net::async_read(m_socket, std::experimental::net::mutable_buffer(buf.data(), buf.size()),
    [this, buf, mh = std::forward(msg_hdlr), mf = std::forward(msg_frame), header_size]
          (const std::error_code& err, std::size_t nb) {
      handle_read(mh, mf, buf, header_size, err, nb);
    }
  );
}

template <typename ET>
template <typename MH, typename MF>
void tcp_io<ET>::handle_read(MH&& msg_hdlr, MF&& msg_frame, chops::mutable_shared_buffer buf, 
                 std::size_t header_size, const std::error_code& err, std::size_t num_bytes) {
  if (!process_err_code(err)) {
    return;
  }
  // assert num_bytes == buf.size()
  std::experimental::net::mutable_buffer mbuf;
  std::size_t next_read_size = msg_frame(buf);
  if (next_read_size == 0) { // msg fully received, now invoke message handler
    if (!msg_hdlr(buf, OutputChannel(std::weak_from_this()), m_remote_endp, msg_frame)) {
      // message handler not happy, tear everything down
      process_err_code(std::make_error_code(chops::net::message_handler_terminated));
      return;
    }
    // buf may be empty if moved from
    buf.resize(header_size);
    mbuf = std::experimental::net::mutable_buffer(buf.data(), buf.size());
  }
  else {
    std::size_t old_size = buf.size();
    buf.resize(buf.size() + next_read_size);
    mbuf = std::experimental::net::mutable_buffer(buf.data() + old_size, next_read_size);
  }
  // start another read, using the buffer supplied in the MsgFrame return val
  std::experimental::net:async_read(m_socket, mbuf,
    [this, buf, mh = std::forward(msg_hdlr), mf = std::forward(msg_frame), header_size]
          (const std::error_code& err, std::size_t nb) {
      handle_read(mh, mf, buf, header_size, err, nb);
    }
  );
}

template <typename ET>
template <typename MH>
void tcp_io<ET>::start_io(MH&& msg_hdlr, const std::string& delimiter) {
  if (!start_io_setup()) {
    return;
  }
  std::experimental::net::async_read_until(m_socket, chops::mutable_shared_buffer(), delimiter,
    [this, buf, mh = std::forward(msg_hdlr), delimiter] (const std::error_code& err, std::size_t nb) {
      handle_read(mh, buf, delimiter, err, nb);
    }
  );
}

template <typename ET>
template <typename MH>
void tcp_io<ET>::handle_read(MH&& msg_hdlr, chops::mutable_shared_buffer buf, std::string_view delimiter,
                 const std::error_code& err, std::size_t num_bytes) {
  if (!process_err_code(err)) {
    return;
  }
  if (!msg_hdlr(buf, OutputChannel(std::weak_from_this()), m_remote_endp)) {
      process_err_code(std::make_error_code(chops::net::message_handler_terminated));
    m_entity_ptr->io_handler_notification(std::make_error_code(chops::net::message_handler_terminated)
                                          std::shared_from_this());
    return;
  }
  // start another read, using the buffer supplied in the MsgFrame return val
  std::experimental::net::async_read_until(m_socket, something, delimiter,
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
  chops::mutable_shared_buffer buf(1);
  std::experimental::net::async_read(m_socket, std::experimental::net::mutable_buffer(buf.data(), buf.size()),
    [this, buf] (const std::error_code& err, std::size_t nb) {
      handle_read(buf, err, nb);
    }
  );
}

template <typename ET>
void tcp_io<ET>::handle_read(chops::mutable_shared_buffer buf, const std::error_code& err, std::size_t nb) {
  if (!process_err_code(err)) {
    return;
  }
  std::experimental::net::async_read(m_socket, std::experimental::net::mutable_buffer(buf.data(), buf.size()),
    [this, buf] (const std::error_code& err, std::size_t nb) {
      if (!process_err_code(err)) {
        return;
      }
      handle_read(buf, err, nb);
    }
  );
}

template <typename ET>
void tcp_io<ET>::start_write(chops::shared_const_buffer buf) {
  if (!mStarted) {
    return; // shutdown happening, most likely, so don't do anything
  }
  if (mWriteInProgress) { // queue buffer
    queueEntry(buf);
    return;
  }
  mWriteInProgress = true;
  std::experimental::net::async_write(m_socket, buf, boost::bind(&tcp_io::handleWrite, shared_from_this(),
                                                     boost::asio::placeholders::error));
}

template <typename ET>
void tcp_io<ET>::handleWrite(const std::system::error_code& err) {
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
  std::experimental::net::async_write(m_socket, e->first, boost::bind(&tcp_io::handleWrite, shared_from_this(),
                                                         boost::asio::placeholders::error));
}

} // end detail namespace
} // end net namespace
} // end chops namespace

#endif

