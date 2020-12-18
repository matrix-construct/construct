// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_FS_IOU_H

// Public and unconditional interface for io_uring. This file is part of the
// standard include stack and available whether or not this platform is Linux
// with io_uring (>= 5.1), and whether or not it's enabled, etc. If it is not
// most of this stuff does nothing and will have null values.

extern "C"
{
	struct io_uring_sqe;
}

/// Input/Output Userspace Ring buffering.
///
/// Note that fs::aio and fs::iou are never used simultaneously. If io_uring
/// is supported by both the compilation and the kernel at runtime then it
/// will be selected over AIO.
namespace ircd::fs::iou
{
	struct init;
	struct system;
	struct request;
	enum state :uint8_t;

	// a priori
	extern const bool support;
	extern const size_t MAX_EVENTS;

	// configuration
	extern conf::item<bool> enable;
	extern conf::item<size_t> max_events;
	extern conf::item<size_t> max_submit;

	// runtime state
	extern struct aio::stats &stats;
	extern struct system *system;

	// util
	string_view reflect(const state &);
	const_iovec_view iovec(const request &);
	const struct ::io_uring_sqe &sqe(const request &);
	struct ::io_uring_sqe &sqe(request &);

	// iterate requests
	static bool for_each(const state &, const std::function<bool (const request &)> &);
	static bool for_each(const std::function<bool (const request &)> &);

	// count requests
	static size_t count(const state &, const op &);
	static size_t count(const state &);
	static size_t count(const op &);
}

struct ircd::fs::iou::request
{
	const fs::opts *opts {nullptr};
	fs::op op {fs::op::NOOP};
	std::error_code ec;
	int32_t res {-1};
	int32_t id {-1};

	request() = default;
	request(const fd &, const const_iovec_view &, const fs::opts *const &);
	~request() noexcept;
};

/// Enumeration of states for a request.
enum ircd::fs::iou::state
:uint8_t
{
	INVALID,
	QUEUED,
	SUBMITTED,
	COMPLETED,

	_NUM
};

/// Internal use; this is simply declared here for when internal headers are
/// not available for this build so a weak no-op definition can be defined.
struct ircd::fs::iou::init
{
	init();
	~init() noexcept;
};
