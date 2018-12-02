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
#define HAVE_IRCD_FS_FSYNC_H

namespace ircd::fs
{
	struct fsync_opts extern const fsync_opts_default;

	void fdsync(const fd &, const fsync_opts & = fsync_opts_default);
	void fsync(const fd &, const fsync_opts & = fsync_opts_default);
}

/// Options for a write operation
struct ircd::fs::fsync_opts
{
	fsync_opts() = default;

	/// Determines whether this operation is conducted via AIO. If not, a
	/// direct syscall is made. Using AIO will only block one ircd::ctx while
	/// a direct syscall will block the thread (all contexts). If AIO is not
	/// available or enabled setting this has no effect.
	bool async {true};

	/// Request priority. This value is ignored by the kernel for the
	/// operations provided by this interface. It is still provided for
	/// consistency and may be used internally by IRCd in the future.
	int8_t priority {-1};
};
