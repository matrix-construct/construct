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
};

inline
ircd::fs::read_opts::read_opts(const off_t &offset)
:offset{offset}
{}
