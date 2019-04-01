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

	size_t size(const fd &);
	size_t block_size(const fd &);
	ulong fstype(const fd &);
	ulong device(const fd &);
}

/// File Desc++ptor. This is simply a native fd (i.e. integer) with c++ object
/// semantics.
struct ircd::fs::fd
{
	struct opts;

	int fdno{-1};

  public:
	operator const int &() const;
	operator bool() const;
	bool operator!() const;

	explicit fd(const int &);
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
	static conf::item<bool> direct_io_enable;

	/// std openmode passed from ctor.
	std::ios::openmode mode {std::ios::openmode(0)};

	/// open(2) flags. Usually generated from ios::open_mode ctor.
	ulong flags {0};

	/// open(2) mode_t mode used for file creation.
	ulong mask {0};

	/// Seek to end after open. This exists to convey the flag for open_mode.
	bool ate {false};

	/// (O_DIRECT) Direct IO bypassing the operating system caches.
	bool direct {false};

	/// (O_CLOEXEC) Close this descriptor on an exec().
	bool cloexec {true};

	/// Prevents file from being created if it doesn't exist. This clears
	/// any implied O_CREAT from the open_mode ctor and in flags too.
	bool nocreate {false};

	/// Construct options from an std::ios::open_mode bitmask.
	opts(const std::ios::openmode &);
	opts() = default;
};

inline bool
ircd::fs::fd::operator!()
const
{
	return !bool(*this);
}

inline ircd::fs::fd::operator
bool()
const
{
	return int(*this) >= 0;
}

inline ircd::fs::fd::operator
const int &()
const
{
	return fdno;
}
