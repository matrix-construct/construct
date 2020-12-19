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
#define HAVE_IRCD_IOS_EPOLL_H

// Forward declarations because sys/epoll.h is not included here
extern "C"
{
	struct epoll_event;
}

// This interface provides special workarounds and optimizations for the epoll
// reactor in boost::asio on linux systems. It has to be used voluntarily by
// the embedder of libircd, who can hook calls to epoll_wait(2), and forward
// those calls to this interface. Our implementation then makes the syscall.
namespace ircd::ios
{
	using epoll_wait_proto = int (int, struct ::epoll_event *, int, int);

	template<epoll_wait_proto *>
	int epoll_wait(int, struct ::epoll_event *, int, int) noexcept;
}

/// This reduces the number of syscalls to epoll_wait(2), which tend to occur
/// at the start of every epoch except in a minority of cases. These syscalls
/// produce no ready events 99% of the time.
///
/// boost::asio tends to call epoll_wait(2) with timeout=0 (non-blocking) when
/// it has more work queued that it will execute. If there's nothing queued it
/// will set a timeout. We don't need to collect epoll events so aggressively.
/// It's incumbent upon us to not spam thousands of non-blocking syscalls which
/// yield no results, especially when it negates the efficiency of ircd::ctx's
/// fast userspace context switching. We trade some responsiveness for things
/// like asio::signal_set but gain overall performance which now has actual
/// impact in the post-meltdown/spectre virtualized reality.
///
template<ircd::ios::epoll_wait_proto *_real_epoll_wait>
[[using gnu: hot, always_inline]]
inline int
ircd::ios::epoll_wait(int _epfd,
                      struct ::epoll_event *const _events,
                      int _maxevents,
                      int _timeout)
noexcept
{
	// Call elision tick counter.
	thread_local uint64_t tick;

	// Configured frequency to allow the call.
	const uint64_t freq
	{
		empt::freq
	};

	const bool skip
	{
		!freq || (tick < freq)
	};

	const bool peek
	{
		_timeout == 0
	};

	// Always allow blocking calls; only allow non-blocking calls which
	// satisfy our conditions.
	const bool call
	{
		!peek || !skip
	};

	// Modify tick counter prior to call, while we have the line.
	tick -= boolmask<decltype(tick)>(call) & tick;
	tick += boolmask<decltype(tick)>(!call) & 0x01;

	const int ret
	{
		call?
			_real_epoll_wait(_epfd, _events, _maxevents, _timeout): 0
	};

	// Update stats
	empt::peek += peek;
	empt::skip += !call;
	empt::call += call;
	empt::none += call && ret == 0;
	empt::result += ret & boolmask<uint>(ret >= 0);
	empt::load_low += ret >= _maxevents / 8;
	empt::load_med += ret >= _maxevents / 4;
	empt::load_high += ret >= _maxevents / 2;
	empt::load_stall += ret >= _maxevents / 1;

	if constexpr(profile::logging) if(call)
		log::logf
		{
			log, ircd::log::DEBUG,
			"EPOLL %5d tick:%lu peek:%lu skip:%lu call:%lu none:%lu result:%lu low:%lu med:%lu high:%lu stall:%lu",
			ret,
			tick,
			uint64_t(empt::peek),
			uint64_t(empt::skip),
			uint64_t(empt::call),
			uint64_t(empt::none),
			uint64_t(empt::result),
			uint64_t(empt::load_low),
			uint64_t(empt::load_med),
			uint64_t(empt::load_high),
			uint64_t(empt::load_stall),
		};

	assert(call || ret == 0);
	assert(ret <= _maxevents);
	return ret;
}
