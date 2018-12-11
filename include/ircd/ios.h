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
	struct signal_set;
}

namespace ircd
{
	namespace asio = boost::asio;      ///< Alias so that asio:: can be used.

	extern const uint boost_version[3];
	extern const string_view boost_version_str;
}

namespace ircd::ios
{
	extern const std::thread::id static_thread_id;
	extern std::thread::id main_thread_id;
	extern asio::io_context *user;

	bool is_main_thread();
	void assert_main_thread();

	asio::io_context &get();
	void dispatch(std::function<void ()>);
	void post(std::function<void ()>);

	void init(asio::io_context &user);
}

namespace ircd
{
	using ios::assert_main_thread;
	using ios::is_main_thread;

	using ios::dispatch;
	using ios::post;
}

inline void
ircd::ios::assert_main_thread()
{
	assert(is_main_thread());
}

inline bool
ircd::ios::is_main_thread()
{
	return std::this_thread::get_id() == main_thread_id;
}
