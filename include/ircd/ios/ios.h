// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_IOS_IOS_H

/// Forward declarations for boost::asio because it is not included here.
namespace boost::asio
{
	struct executor;
}

namespace ircd
{
	namespace asio = boost::asio;      ///< Alias so that asio:: can be used.

	extern const info::versions boost_version_api, boost_version_abi;
}

namespace ircd::ios
{
	extern log::log log;
	extern asio::executor user, main;
	extern std::thread::id main_thread_id;
	extern thread_local bool is_main_thread;

	bool available() noexcept;
	const uint64_t &epoch() noexcept;

	void forked_parent();
	void forked_child();
	void forking();

	void init(asio::executor &&);
}

namespace ircd::ios::profile
{
	constexpr bool history {false};
	constexpr bool logging {false};
}

#include "descriptor.h"
#include "handler.h"
#include "asio.h"
#include "empt.h"
#include "dispatch.h"
#include "epoll.h"

inline const uint64_t &
__attribute__((always_inline))
ircd::ios::epoch()
noexcept
{
	return handler::epoch;
}
