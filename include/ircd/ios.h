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
#define HAVE_IRCD_IOS_H

// Boost headers are not exposed to our users unless explicitly included by a
// definition file. Other libircd headers may extend this namespace with more
// forward declarations.

/// Forward declarations for boost::asio because it is not included here.
namespace boost::asio
{
	struct io_context;
}

namespace ircd
{
	/// Alias so that asio:: can be used
	namespace asio = boost::asio;

	/// A record of the thread ID when static initialization took place (for ircd.cc)
	extern const std::thread::id static_thread_id;

	/// The thread ID of the main IRCd thread running the event loop.
	extern std::thread::id thread_id;

	/// The user's io_service
	extern asio::io_context *ios;

	bool is_main_thread();
	void assert_main_thread();

	void post(std::function<void ()>);
	void dispatch(std::function<void ()>);
}

inline void
ircd::assert_main_thread()
{
	assert(is_main_thread());
}

inline bool
ircd::is_main_thread()
{
	return std::this_thread::get_id() == ircd::thread_id;
}
