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
#define HAVE_IRCD_FS_READ_H

namespace ircd::fs
{
	struct read_opts extern const read_opts_default;

	// Yields ircd::ctx for read into buffers; returns bytes read
	size_t read(const fd &, const mutable_buffers &, const read_opts & = read_opts_default);
	size_t read(const string_view &path, const mutable_buffers &, const read_opts & = read_opts_default);

	// Yields ircd::ctx for read into buffer; returns view of read portion.
	const_buffer read(const fd &, const mutable_buffer &, const read_opts & = read_opts_default);
	const_buffer read(const string_view &path, const mutable_buffer &, const read_opts & = read_opts_default);

	// Yields ircd::ctx for read into allocated string; returns that string
	std::string read(const fd &, const read_opts & = read_opts_default);
	std::string read(const string_view &path, const read_opts & = read_opts_default);

	// Prefetch bytes for subsequent read(); offset is given in opts.
	void prefetch(const fd &, const size_t &, const read_opts & = read_opts_default);
}

/// Options for a read operation
struct ircd::fs::read_opts
{
	read_opts() = default;
	read_opts(const off_t &);

	/// Offset in the file to start the read from.
	off_t offset {0};

	/// Request priority. Lower value request will take priority over higher
	int8_t priority {0};

	/// Determines whether this operation is conducted via AIO. If not, a
	/// direct syscall is made. Using AIO will only block one ircd::ctx while
	/// a direct syscall will block the thread (all contexts). If AIO is not
	/// available or not enabled, or doesn't support this operation, setting
	/// this has no effect.
	bool aio {true};

	/// Yields the ircd::ctx until the buffers are full or EOF. This performs
	/// the unix incremental read loop internally. When this option is true,
	/// any return value from a function in the read suite will not be a
	/// partial value requiring another invocation of read.
	bool all {true};

	/// Whether to propagate an EINTR; otherwise we reinvoke the syscall. For a
	/// read(2) family call this can only happen before any data was read;
	/// an exception will be thrown. We default to true because we have faith
	/// in the useful propagation of an exception for this event.
	bool interruptible {true};

	/// Submits the I/O request immediately rather than allowing IRCd to
	/// queue requests for a few iterations of the ircd::ios event loop.
	/// (only relevant to aio).
	bool nodelay {false};
};

inline
ircd::fs::read_opts::read_opts(const off_t &offset)
:offset{offset}
{}
