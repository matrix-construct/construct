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
#define HAVE_IRCD_FS_AIO_H

// Public and unconditional interface for AIO. This file is part of the
// standard include stack and available whether or not this platform is Linux
// with AIO, and whether or not it's enabled, etc. If it is not most of this
// stuff does nothing and will have null values.

/// Asynchronous filesystem Input/Output
///
namespace ircd::fs::aio
{
	struct init;
	struct stats;
	struct system;
	struct request;

	extern const bool support;
	extern const bool support_fsync;
	extern const bool support_fdsync;

	extern const size_t MAX_EVENTS;
	extern const size_t MAX_REQPRIO;

	extern conf::item<bool> enable;
	extern conf::item<size_t> max_events;
	extern conf::item<size_t> max_submit;

	extern struct stats stats;
	extern struct system *system;

	bool for_each_completed(const std::function<bool (const request &)> &);
	bool for_each_queued(const std::function<bool (const request &)> &);
	size_t count_queued(const op &);
}

/// Statistics structure.
///
struct ircd::fs::aio::stats
{
	uint64_t requests {0};             ///< count of requests created
	uint64_t complete {0};             ///< count of requests completed
	uint64_t submits {0};              ///< count of io_submit calls
	uint64_t chases {0};               ///< count of chase calls
	uint64_t handles {0};              ///< count of event_fd callbacks
	uint64_t events {0};               ///< count of events from io_getevents
	uint64_t cancel {0};               ///< count of requests canceled
	uint64_t errors {0};               ///< count of response errcodes
	uint64_t reads {0};                ///< count of read complete
	uint64_t writes {0};               ///< count of write complete
	uint64_t stalls {0};               ///< count of io_submit's blocking.

	uint64_t bytes_requests {0};       ///< total bytes for requests created
	uint64_t bytes_complete {0};       ///< total bytes for requests completed
	uint64_t bytes_errors {0};         ///< total bytes for completed w/ errc
	uint64_t bytes_cancel {0};         ///< total bytes for cancels
	uint64_t bytes_read {0};           ///< total bytes for read completed
	uint64_t bytes_write {0};          ///< total bytes for write completed

	uint32_t cur_bytes_write {0};      ///< pending write bytes
	uint16_t cur_reads {0};            ///< pending reads
	uint16_t cur_writes {0};           ///< pending write
	uint16_t cur_queued {0};           ///< nr of requests in userspace queue
	uint16_t cur_submits {0};          ///< nr requests in flight with kernel

	uint16_t max_requests {0};         ///< maximum observed pending requests
	uint16_t max_reads {0};            ///< maximum observed pending reads
	uint16_t max_writes {0};           ///< maximum observed pending write
	uint16_t max_queued {0};           ///< maximum observed in queue.
	uint16_t max_submits {0};          ///< maximum observed in flight.
};

struct ircd::fs::aio::init
{
	init();
	~init() noexcept;
};
