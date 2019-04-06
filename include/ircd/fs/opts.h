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

	int reqprio(int);
}

/// Options common to all operations
struct ircd::fs::opts
{
	static const int highest_priority;

	/// Offset in the file. If this is -1, for writes, it indicates an append
	/// at the end of the file (RWF_APPEND or legacy non-atomic lseek()).
	off_t offset {0};

	/// Request priority. Lower value takes priority over higher. The lowest
	/// possible priority value is special, on supporting platforms (RWF_HIPRI).
	/// One can either simply set the integer minimum or use the extern value.
	int8_t priority {0};

	/// Submits the I/O request immediately rather than allowing IRCd to
	/// queue requests for a few iterations of the ircd::ios event loop.
	/// (only relevant to aio).
	bool nodelay {false};

	/// Setting this to false enables non-blocking behavior. If the operation
	/// would block, EAGAIN is returned. This is only available with RWF_NOWAIT
	/// on newer systems, otherwise this value is ignored and is always true.
	/// This feature makes up for the fact that O_NONBLOCK when opening the
	/// file is ineffective for regular files.
	bool blocking {true};

	/// Determines whether this operation is conducted via AIO. If not, a
	/// direct syscall is made. Using AIO will only block one ircd::ctx while
	/// a direct syscall will block the thread (all contexts). If AIO is not
	/// available or not enabled, or doesn't support this operation, setting
	/// this has no effect.
	bool aio {true};

	/// The enumerated operation code to identify the type of request being
	/// made at runtime from any abstract list of requests. This is set by
	/// the fs:: interface call and not the user in most cases. The user
	/// should not rely on this value being preserved if, e.g. they set a read
	/// opcode and then pass the opts structure to write().
	enum op op {op::NOOP};

	opts(const off_t &, const enum op &op);
	opts() = default;
};

inline
ircd::fs::opts::opts(const off_t &offset,
                     const enum op &op)
:offset{offset}
,op{op}
{}
