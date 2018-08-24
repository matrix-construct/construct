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

	// Yields ircd::ctx for write from buffer; returns view of written portion
	const_buffer write(const fd &, const const_buffer &, const write_opts & = write_opts_default);
	const_buffer write(const string_view &path, const const_buffer &, const write_opts & = write_opts_default);

	// Yields ircd::ctx to append to file from buffer; returns view of written portion
	const_buffer append(const fd &, const const_buffer &, const write_opts & = write_opts_default);
	const_buffer append(const string_view &path, const const_buffer &, const write_opts & = write_opts_default);

	// Yields ircd::ctx to overwrite (trunc) file from buffer; returns view of written portion
	const_buffer overwrite(const fd &, const const_buffer & = {}, const write_opts & = write_opts_default);
	const_buffer overwrite(const string_view &path, const const_buffer & = {}, const write_opts & = write_opts_default);

	// Truncate file to explicit size
	void truncate(const fd &, const size_t &, const write_opts & = write_opts_default);
	void truncate(const string_view &path, const size_t &, const write_opts & = write_opts_default);
}

/// Options for a write operation
struct ircd::fs::write_opts
{
	write_opts() = default;
	write_opts(const off_t &);

	/// Offset in the file to start the write from.
	off_t offset {0};

	/// Request priority (this option may be improved, avoid for now)
	int16_t priority {0};
};

inline
ircd::fs::write_opts::write_opts(const off_t &offset)
:offset{offset}
{}
inline ircd::const_buffer
ircd::fs::append(const string_view &path,
                 const const_buffer &buf,
                 const write_opts &opts)
{
	const fd fd
	{
		path, std::ios::out | std::ios::app
	};

	return write(fd, buf, opts);
}
