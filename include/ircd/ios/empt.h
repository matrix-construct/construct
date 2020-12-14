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
#define HAVE_IRCD_IOS_EMPT_H

/// Emption interface.
///
/// On supporting systems and with the cooperation of libircd's embedder these
/// items can aid with optimizing and/or profiling the boost::asio core event
/// loop. See epoll.h for an epoll_wait(2) use of these items. This is a
/// separate header/namespace so this can remain abstract and applied to
/// different platforms.
///
namespace ircd::ios::empt
{
	extern conf::item<uint64_t> freq;

	extern stats::item<uint64_t *> peek;
	extern stats::item<uint64_t *> skip;
	extern stats::item<uint64_t *> call;
	extern stats::item<uint64_t *> none;
	extern stats::item<uint64_t *> result;
	extern stats::item<uint64_t *> load_low;
	extern stats::item<uint64_t *> load_med;
	extern stats::item<uint64_t *> load_high;
	extern stats::item<uint64_t *> load_stall;
}
