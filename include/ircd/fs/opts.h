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
#define HAVE_IRCD_FS_SYNC_OPTS_H

namespace ircd::fs
{
	struct opts extern const opts_default;
}

/// Options common to all operations
struct ircd::fs::opts
{
    /// Offset in the file.
    off_t offset {0};

	/// Request priority. Lower value takes priority over higher.
	int8_t priority {0};

	/// Submits the I/O request immediately rather than allowing IRCd to
	/// queue requests for a few iterations of the ircd::ios event loop.
	/// (only relevant to aio).
	bool nodelay {false};

	/// Determines whether this operation is conducted via AIO. If not, a
	/// direct syscall is made. Using AIO will only block one ircd::ctx while
	/// a direct syscall will block the thread (all contexts). If AIO is not
	/// available or not enabled, or doesn't support this operation, setting
	/// this has no effect.
	bool aio {true};

	opts(const off_t &);
	opts() = default;
};

inline
ircd::fs::opts::opts(const off_t &offset)
:offset{offset}
{}
