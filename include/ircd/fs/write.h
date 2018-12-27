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
#define HAVE_IRCD_FS_WRITE_H

namespace ircd::fs
{
	struct write_opts extern const write_opts_default;

	// Yields ircd::ctx for write from buffers; returns bytes written
	size_t write(const fd &, const const_buffers &, const write_opts & = write_opts_default);
	size_t write(const string_view &path, const const_buffers &, const write_opts & = write_opts_default);

	// Yields ircd::ctx for write from buffer; returns view of written portion
	const_buffer write(const fd &, const const_buffer &, const write_opts & = write_opts_default);
	const_buffer write(const string_view &path, const const_buffer &, const write_opts & = write_opts_default);

	// Yields ircd::ctx to append to file from buffers; returns bytes appended
	size_t append(const fd &, const const_buffers &, const write_opts & = write_opts_default);
	size_t append(const string_view &path, const const_buffers &, const write_opts & = write_opts_default);

	// Yields ircd::ctx to append to file from buffer; returns view of written portion
	const_buffer append(const fd &, const const_buffer &, const write_opts & = write_opts_default);
	const_buffer append(const string_view &path, const const_buffer &, const write_opts & = write_opts_default);

	// Yields ircd::ctx to overwrite (trunc) file from buffers; returns bytes written
	size_t overwrite(const fd &, const const_buffers &, const write_opts & = write_opts_default);
	size_t overwrite(const string_view &path, const const_buffers &, const write_opts & = write_opts_default);

	// Yields ircd::ctx to overwrite (trunc) file from buffer; returns view of written portion
	const_buffer overwrite(const fd &, const const_buffer & = {}, const write_opts & = write_opts_default);
	const_buffer overwrite(const string_view &path, const const_buffer & = {}, const write_opts & = write_opts_default);

	// Truncate file to explicit size
	void truncate(const fd &, const size_t &, const write_opts & = write_opts_default);
	void truncate(const string_view &path, const size_t &, const write_opts & = write_opts_default);

	// Allocate
	void allocate(const fd &, const size_t &size, const write_opts & = write_opts_default);
}

/// Options for a write operation
struct ircd::fs::write_opts
{
	write_opts() = default;
	write_opts(const off_t &);

	/// Offset in the file to start the write from. For append() if this zero
	/// then it will be internally set to the end of the file; otherwise if
	/// this is set it will write to that offset, even for append(), unless
	/// the host system later ignores the offset due to the file's openmode.
	off_t offset {0};

	/// Request priority. Lower value request will take priority over higher.
	int8_t priority {0};

	/// for allocate()
	bool keep_size {false};

	/// Determines whether this operation is conducted via AIO. If not, a
	/// direct syscall is made. Using AIO will only block one ircd::ctx while
	/// a direct syscall will block the thread (all contexts). If AIO is not
	/// available or not enabled, or doesn't support this operation, setting
	/// this has no effect.
	bool aio {true};

	/// Yields the ircd::ctx until the buffers are written. This performs
	/// the unix incremental write loop internally. When this option is true,
	/// any return value from a function in the write() suite will not be a
	/// partial value requiring another invocation of write().
	bool all {true};

	/// Whether to propagate an EINTR; otherwise we reinvoke the syscall. For a
	/// write(2) family call this can only happen before any data was written;
	/// an exception will be thrown. We default to true because we have faith
	/// in the useful propagation of an exception for this event.
	bool interruptible {true};

	/// Submits the I/O request immediately rather than allowing IRCd to
	/// queue requests for a few iterations of the ircd::ios event loop.
	/// (only relevant to aio).
	bool nodelay {false};
};

inline
ircd::fs::write_opts::write_opts(const off_t &offset)
:offset{offset}
{}
