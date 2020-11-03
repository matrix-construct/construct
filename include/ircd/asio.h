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

#define BOOST_COROUTINES_NO_DEPRECATION_WARNING
#pragma GCC visibility push(default)

// Boost preambles
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

// Needed for consistent interop with std::system_error
#include <boost/system/system_error.hpp>

// Boost ASIO stack
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/io_service.hpp>

#pragma GCC visibility pop

// Boost version dependent behavior for getting the io_service/io_context
// abstract executor (recent versions) or the derived instance (old versions).
namespace ircd::ios
{
	#if BOOST_VERSION >= 107000
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

#if BOOST_VERSION >= 107000
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
