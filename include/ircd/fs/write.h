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
:opts
{
	/// for allocate()
	bool keep_size {false};

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

	/// Whether to update the fd's offset on appends. This happens naturally
	/// when the file is opened in append mode. If not, we get the same per-
	/// write atomic seek behavior if RWF_APPEND is supported. In the latter
	/// case, this option determines whether the fd's offset is affected.
	bool update_offset {true};

	/// Whether to RWF_SYNC or RWF_DSYNC depending on the metadata option. This
	/// is a range-sync, it only covers the offset and size of the write;
	/// perhaps a worthy replacement for sync_file_range(2).
	bool sync {false};

	/// When sync is true: if metadata is true RWF_SYNC (like fsync(2)) is used,
	/// otherwise RWF_DSYNC (like fdsync(2)) is used. This is only if available,
	/// Careful, if it is not available you are responsible for following the
	/// write with fsync(2)/fdsync(2) yourself.
	bool metadata {false};

	write_opts(const off_t & = 0);
};

inline
ircd::fs::write_opts::write_opts(const off_t &offset)
:opts{offset, op::WRITE}
{}
