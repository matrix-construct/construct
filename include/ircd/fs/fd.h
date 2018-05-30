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
#define HAVE_IRCD_FS_FD_H

namespace ircd::fs
{
	struct fd;
}

/// Object for maintaining state to an open file or directory. Instances can
/// be used with various functions around ircd::fs.
struct ircd::fs::fd
{
	struct opts;

	int fdno{-1};

  public:
	operator const int &() const
	{
		return fdno;
	}

	fd(const string_view &path, const opts &);
	fd(const string_view &path);
	fd() = default;
	fd(fd &&) noexcept;
	fd(const fd &) = delete;
	fd &operator=(fd &&) noexcept;
	fd &operator=(const fd &) = delete;
	~fd() noexcept(false);
};

struct ircd::fs::fd::opts
{
	ulong flags {0};
	ulong mask {0};
	bool ate {false};

	opts(const std::ios::open_mode &);
	opts() = default;
};
