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

	/// Request priority. Higher value request will take priority over lower
	/// value. Lowest value is zero. Negative value will receive a contextual
	/// value internally (generally just zero). Default is -1.
	int8_t priority {-1};

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
};

inline
ircd::fs::read_opts::read_opts(const off_t &offset)
:offset{offset}
{}
