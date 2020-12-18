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

	extern const size_t MAX_EVENTS;
	extern const size_t MAX_REQPRIO;

	extern conf::item<bool> enable;
	extern conf::item<size_t> max_events;
	extern conf::item<size_t> max_submit;
	extern conf::item<bool> submit_coalesce;

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
	using item = ircd::stats::item<uint64_t *>;

	uint64_t value[32];
	size_t items;

	item requests;             ///< count of requests created
	item complete;             ///< count of requests completed
	item submits;              ///< count of io_submit calls
	item chases;               ///< count of chase calls
	item handles;              ///< count of event_fd callbacks
	item events;               ///< count of events from io_getevents
	item cancel;               ///< count of requests canceled
	item errors;               ///< count of response errcodes
	item reads;                ///< count of read complete
	item writes;               ///< count of write complete
	item stalls;               ///< count of io_submit's blocking.

	item bytes_requests;       ///< total bytes for requests created
	item bytes_complete;       ///< total bytes for requests completed
	item bytes_errors;         ///< total bytes for completed w/ errc
	item bytes_cancel;         ///< total bytes for cancels
	item bytes_read;           ///< total bytes for read completed
	item bytes_write;          ///< total bytes for write completed

	item cur_bytes_write;      ///< pending write bytes
	item cur_reads;            ///< pending reads
	item cur_writes;           ///< pending write
	item cur_queued;           ///< nr of requests in userspace queue
	item cur_submits;          ///< nr requests in flight with kernel

	item max_requests;         ///< maximum observed pending requests
	item max_reads;            ///< maximum observed pending reads
	item max_writes;           ///< maximum observed pending write
	item max_queued;           ///< maximum observed in queue.
	item max_submits;          ///< maximum observed in flight.

	stats();
};

struct ircd::fs::aio::init
{
	init();
	~init() noexcept;
};
