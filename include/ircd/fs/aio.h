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
	struct kernel;
	struct request;

	extern kernel *context;
	extern struct stats stats;
}

/// Statistics structure.
///
/// Subtract complete from requests to get the count of requests pending
/// "in flight." Subtract errors from complete to only get the count of
/// fully successful operations. complete will always eventually match
/// up with requests.
///
/// Subtract complete_bytes from requests_bytes to get the total bytes
/// currently pending "in flight." Subtract errors_bytes from response_bytes
/// to only get bytes successfully read or written. complete_bytes will
/// always eventually match up with requests_bytes.
///
struct ircd::fs::aio::stats
{
	uint64_t requests {0};             ///< count of requests created
	uint64_t complete {0};             ///< count of requests completed
	uint64_t handles {0};              ///< count of event_fd callbacks
	uint64_t events {0};               ///< count of events from io_getevents
	uint64_t cancel {0};               ///< count of requests canceled
	uint64_t errors {0};               ///< count of response errcodes
	uint64_t reads {0};                ///< count of read complete
	uint64_t writes {0};               ///< count of write complete

	uint64_t requests_bytes {0};       ///< total bytes for requests created
	uint64_t complete_bytes {0};       ///< total bytes for requests completed
	uint64_t errors_bytes {0};         ///< total bytes for completed w/ errc
	uint64_t cancel_bytes {0};         ///< total bytes for cancels
	uint64_t read_bytes {0};           ///< total bytes for read completed
	uint64_t write_bytes {0};          ///< total bytes for write completed

};

struct ircd::fs::aio::init
{
	init();
	~init() noexcept;
};
