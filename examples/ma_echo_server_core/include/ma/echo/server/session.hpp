//
// Copyright (c) 2010-2015 Marat Abrarov (abrarov@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef MA_ECHO_SERVER_SESSION_HPP
#define MA_ECHO_SERVER_SESSION_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <cstddef>
#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <boost/noncopyable.hpp>
#include <boost/system/error_code.hpp>
#include <ma/config.hpp>
#include <ma/detail/type_traits.hpp>
#include <ma/cyclic_buffer.hpp>
#include <ma/handler_storage.hpp>
#include <ma/handler_allocator.hpp>
#include <ma/bind_handler.hpp>
#include <ma/context_alloc_handler.hpp>
#include <ma/echo/server/session_config.hpp>
#include <ma/echo/server/session_fwd.hpp>
#include <ma/strand.hpp>
#include <ma/steady_deadline_timer.hpp>
#include <ma/detail/memory.hpp>
#include <ma/detail/functional.hpp>
#include <ma/detail/utility.hpp>

namespace ma {
namespace echo {
namespace server {

class session
  : private boost::noncopyable
  , public  detail::enable_shared_from_this<session>
{
private:
  typedef session this_type;

public:
  typedef boost::asio::ip::tcp protocol_type;

  static session_ptr create(boost::asio::io_service& io_service,
      const session_config& config);

  protocol_type::socket& socket();

  void reset();

  template <typename Handler>
  void async_start(MA_FWD_REF(Handler) handler);

  template <typename Handler>
  void async_stop(MA_FWD_REF(Handler) handler);

  template <typename Handler>
  void async_wait(MA_FWD_REF(Handler) handler);

protected:
  session(boost::asio::io_service&, const session_config&);
  ~session();

private:

#if defined(MA_HAS_RVALUE_REFS) && defined(MA_BIND_HAS_NO_MOVE_CONSTRUCTOR)

  // Home-grown binder to support move semantic
  template <typename Arg>
  class forward_handler_binder;

#endif

  struct extern_state
  {
    enum value_t {ready, work, stop, stopped};
  };

  struct intern_state
  {
    enum value_t {work, shutdown, stop, stopped};
  };

  struct read_state
  {
    enum value_t {wait, in_progress, stopped};
  };

  struct write_state
  {
    enum value_t {wait, in_progress, stopped};
  };

  struct timer_state
  {
    enum value_t {ready, in_progress, stopped};
  };

  typedef boost::optional<boost::system::error_code> optional_error_code;
  typedef steady_deadline_timer          deadline_timer;
  typedef deadline_timer::duration_type  duration_type;
  typedef boost::optional<duration_type> optional_duration;

  template <typename Handler>
  void start_extern_start(Handler&);

  template <typename Handler>
  void start_extern_stop(Handler&);

  template <typename Handler>
  void start_extern_wait(Handler&);

  void handle_read(const boost::system::error_code&, std::size_t);
  void handle_write(const boost::system::error_code&, std::size_t);
  void handle_timer(const boost::system::error_code&);

  boost::system::error_code do_start_extern_start();
  optional_error_code do_start_extern_stop();
  optional_error_code do_start_extern_wait();
  void complete_extern_stop(const boost::system::error_code&);
  void complete_extern_wait(const boost::system::error_code&);

  void handle_read_at_work(const boost::system::error_code&, std::size_t);
  void handle_read_at_shutdown(const boost::system::error_code&, std::size_t);
  void handle_read_at_stop(const boost::system::error_code&, std::size_t);

  void handle_write_at_work(const boost::system::error_code&, std::size_t);
  void handle_write_at_shutdown(const boost::system::error_code&, std::size_t);
  void handle_write_at_stop(const boost::system::error_code&, std::size_t);

  void handle_timer_at_work(const boost::system::error_code&);
  void handle_timer_at_stop(const boost::system::error_code&);

  void continue_work();
  void continue_timer_wait();
  void continue_shutdown(bool need_timer_restart);
  void continue_shutdown_at_read_wait(bool need_timer_restart);
  void continue_shutdown_at_read_in_progress(bool need_timer_restart);
  void continue_shutdown_at_read_stopped(bool need_timer_restart);
  void continue_stop();

  void start_shutdown(const boost::system::error_code& error,
      bool need_timer_restart);
  void start_stop(boost::system::error_code);

  void start_socket_read(const cyclic_buffer::mutable_buffers_type&);
  void start_socket_write(const cyclic_buffer::const_buffers_type&);
  void start_timer_wait();
  boost::system::error_code cancel_timer_wait();
  boost::system::error_code shutdown_socket();
  boost::system::error_code close_socket();
  boost::system::error_code apply_socket_options();

  static optional_duration to_optional_duration(
      const session_config::optional_time_duration& duration);

  const std::size_t                   max_transfer_size_;
  const session_config::optional_int  socket_recv_buffer_size_;
  const session_config::optional_int  socket_send_buffer_size_;
  const session_config::tribool       no_delay_;
  const optional_duration             inactivity_timeout_;

  extern_state::value_t extern_state_;
  intern_state::value_t intern_state_;
  read_state::value_t   read_state_;
  write_state::value_t  write_state_;
  timer_state::value_t  timer_state_;
  bool                  timer_wait_cancelled_;
  bool                  timer_turned_;
  std::size_t           pending_operations_;

  boost::asio::io_service&  io_service_;
  ma::strand                strand_;
  protocol_type::socket     socket_;
  deadline_timer            timer_;
  cyclic_buffer             buffer_;
  boost::system::error_code extern_wait_error_;

  handler_storage<boost::system::error_code> extern_wait_handler_;
  handler_storage<boost::system::error_code> extern_stop_handler_;

  in_place_handler_allocator<640> write_allocator_;
  in_place_handler_allocator<256> read_allocator_;
  in_place_handler_allocator<256> timer_allocator_;
}; // class session

inline session::protocol_type::socket& session::socket()
{
  return socket_;
}

#if defined(MA_HAS_RVALUE_REFS) && defined(MA_BIND_HAS_NO_MOVE_CONSTRUCTOR)

template <typename Arg>
class session::forward_handler_binder
{
private:
  typedef forward_handler_binder this_type;

public:
  typedef void result_type;
  typedef void (session::*func_type)(Arg&);

  template <typename SessionPtr>
  forward_handler_binder(func_type func, SessionPtr&& session);

#if defined(MA_NO_IMPLICIT_MOVE_CONSTRUCTOR) || !defined(NDEBUG)

  forward_handler_binder(this_type&&);
  forward_handler_binder(const this_type&);

#endif // defined(MA_NO_IMPLICIT_MOVE_CONSTRUCTOR) || !defined(NDEBUG)

  void operator()(Arg& arg);

private:
  func_type   func_;
  session_ptr session_;
}; // class session::forward_handler_binder

#endif // defined(MA_HAS_RVALUE_REFS)
       //     && defined(MA_BIND_HAS_NO_MOVE_CONSTRUCTOR)

template <typename Handler>
void session::async_start(MA_FWD_REF(Handler) handler)
{
  typedef typename detail::decay<Handler>::type handler_type;
  typedef void (this_type::*func_type)(handler_type&);
  func_type func = &this_type::start_extern_start<handler_type>;

#if defined(MA_HAS_RVALUE_REFS) && defined(MA_BIND_HAS_NO_MOVE_CONSTRUCTOR)

  strand_.post(make_explicit_context_alloc_handler(
      detail::forward<Handler>(handler),
      forward_handler_binder<handler_type>(func, shared_from_this())));

#else

  strand_.post(make_explicit_context_alloc_handler(
      detail::forward<Handler>(handler),
      detail::bind(func, shared_from_this(), detail::placeholders::_1)));

#endif
}

template <typename Handler>
void session::async_stop(MA_FWD_REF(Handler) handler)
{
  typedef typename detail::decay<Handler>::type handler_type;
  typedef void (this_type::*func_type)(handler_type&);
  func_type func = &this_type::start_extern_stop<handler_type>;

#if defined(MA_HAS_RVALUE_REFS) && defined(MA_BIND_HAS_NO_MOVE_CONSTRUCTOR)

  strand_.post(make_explicit_context_alloc_handler(
      detail::forward<Handler>(handler),
      forward_handler_binder<handler_type>(func, shared_from_this())));

#else

  strand_.post(make_explicit_context_alloc_handler(
      detail::forward<Handler>(handler),
      detail::bind(func, shared_from_this(), detail::placeholders::_1)));

#endif
}

template <typename Handler>
void session::async_wait(MA_FWD_REF(Handler) handler)
{
  typedef typename detail::decay<Handler>::type handler_type;
  typedef void (this_type::*func_type)(handler_type&);
  func_type func = &this_type::start_extern_wait<handler_type>;

#if defined(MA_HAS_RVALUE_REFS) && defined(MA_BIND_HAS_NO_MOVE_CONSTRUCTOR)

  strand_.post(make_explicit_context_alloc_handler(
      detail::forward<Handler>(handler),
      forward_handler_binder<handler_type>(func, shared_from_this())));

#else

  strand_.post(make_explicit_context_alloc_handler(
      detail::forward<Handler>(handler),
      detail::bind(func, shared_from_this(), detail::placeholders::_1)));

#endif
}

inline session::~session()
{
}

template <typename Handler>
void session::start_extern_start(Handler& handler)
{
  boost::system::error_code result = do_start_extern_start();
  io_service_.post(bind_handler(detail::move(handler), result));
}

template <typename Handler>
void session::start_extern_stop(Handler& handler)
{
  if (optional_error_code result = do_start_extern_stop())
  {
    io_service_.post(bind_handler(detail::move(handler), *result));
  }
  else
  {
    extern_stop_handler_.store(detail::move(handler));
  }
}

template <typename Handler>
void session::start_extern_wait(Handler& handler)
{
  if (optional_error_code result = do_start_extern_wait())
  {
    io_service_.post(bind_handler(detail::move(handler), *result));
  }
  else
  {
    extern_wait_handler_.store(detail::move(handler));
  }
}

#if defined(MA_HAS_RVALUE_REFS) && defined(MA_BIND_HAS_NO_MOVE_CONSTRUCTOR)

template <typename Arg>
template <typename SessionPtr>
session::forward_handler_binder<Arg>::forward_handler_binder(
    func_type func, SessionPtr&& session)
  : func_(func)
  , session_(detail::forward<SessionPtr>(session))
{
}

#if defined(MA_NO_IMPLICIT_MOVE_CONSTRUCTOR) || !defined(NDEBUG)

template <typename Arg>
session::forward_handler_binder<Arg>::forward_handler_binder(
    this_type&& other)
  : func_(other.func_)
  , session_(detail::move(other.session_))
{
}

template <typename Arg>
session::forward_handler_binder<Arg>::forward_handler_binder(
    const this_type& other)
  : func_(other.func_)
  , session_(other.session_)
{
}

#endif // defined(MA_NO_IMPLICIT_MOVE_CONSTRUCTOR) || !defined(NDEBUG)

template <typename Arg>
void session::forward_handler_binder<Arg>::operator()(Arg& arg)
{
  ((*session_).*func_)(arg);
}

#endif // defined(MA_HAS_RVALUE_REFS)
       //     && defined(MA_BIND_HAS_NO_MOVE_CONSTRUCTOR)

} // namespace server
} // namespace echo
} // namespace ma

#endif // MA_ECHO_SERVER_SESSION_HPP
