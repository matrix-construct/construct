// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_ASIO_H

///
/// Boost library
///
/// It is better to add a boost header which we have omitted here rather than
/// in your definition file, and then #include <ircd/asio.h>. This is because
/// we can compose a precompiled header for your definition file much easier
/// this way.
///
/// Note that there is no precompile for this header right now, only the
/// standard headers. That still significantly improves compile times of these
/// boost headers for the time being...
///

#define BOOST_COROUTINES_NO_DEPRECATION_WARNING

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/io_service.hpp>

///
/// The following IRCd headers are not included in the main stdinc.h list of
/// includes because they require boost directly or symbols which we cannot
/// forward declare. You should include this in your definition file if you
/// need these low-level interfaces.
///

#include <ircd/ctx/continuation.h>
#include <ircd/net/asio.h>
