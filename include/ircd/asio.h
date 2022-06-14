// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#ifndef HAVE_IRCD_ASIO_H
#define HAVE_IRCD_ASIO_H

///
/// Boost library
///
/// It is better to add a boost header which we have omitted here rather than
/// in your definition file, and then #include <ircd/asio.h>. This is because
/// we can compose a precompiled header for your definition file much easier
/// this way.
///

// ircd.h is included here so that it can be compiled into this header. Then
// this becomes the single leading precompiled header.
#include <ircd/ircd.h>

// Boost preambles
#define BOOST_COROUTINES_NO_DEPRECATION_WARNING
#include <boost/version.hpp>
#include <boost/config.hpp>

// Workarounds / fixes
#if BOOST_VERSION >= 107000
namespace boost
{
	using std::begin;
	using std::end;
}
#endif

#include <RB_INC_SIGNAL_H
#include <RB_INC_SYS_MMAN_H
#include <RB_INC_SYS_EVENT_H
#include <RB_INC_SYS_EPOLL_H
#include <RB_INC_SYS_TIMERFD_H
#include <RB_INC_SYS_EVENTFD_H

#pragma GCC visibility push(internal)
#include <boost/throw_exception.hpp>
#pragma GCC visibility pop

#include <boost/system/system_error.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/asio/detail/config.hpp>
#include <boost/asio/detail/socket_types.hpp>
#include <boost/asio/ssl/detail/openssl_types.hpp>
#include <boost/coroutine/coroutine.hpp>

#if defined(BOOST_ASIO_HAS_EPOLL) || defined(BOOST_ASIO_HAS_KQUEUE)
#pragma GCC visibility push(protected)
#endif
#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl.hpp>
#if defined(BOOST_ASIO_HAS_EPOLL) || defined(BOOST_ASIO_HAS_KQUEUE)
#pragma GCC visibility pop
#endif

// Boost version dependent behavior for getting the io_service/io_context
// abstract executor (recent versions) or the derived instance (old versions).
namespace ircd::ios
{
	extern asio::executor user, main;
	extern std::optional<asio::io_context::strand> primary;

	#if BOOST_VERSION >= 107000 && BOOST_VERSION < 107400
	asio::executor &get() noexcept;
	#else
	asio::io_context &get() noexcept;
	#endif
}

// The following IRCd headers are not included in the main stdinc.h list of
// includes because they require boost directly or symbols which we cannot
// forward declare. You should include this in your definition file if you
// need these low-level interfaces.

// Context system headers depending on boost.
#include <ircd/ctx/continuation.h>

// Network system headers depending on boost.
#include <ircd/net/asio.h>

#if BOOST_VERSION >= 107000 && BOOST_VERSION < 107400
inline boost::asio::executor &
ircd::ios::get()
noexcept
{
	assert(bool(main));
	return main;
}
#else
inline boost::asio::io_context &
ircd::ios::get()
noexcept
{
	auto &context(mutable_cast(main.context()));
	return static_cast<asio::io_context &>(context);
}
#endif

#endif HAVE_IRCD_ASIO_H
