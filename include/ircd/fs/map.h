// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_FS_MAP_H

namespace ircd::fs
{
	struct map;

	size_t advise(const map &, const int &, const size_t &, const opts & = opts_default);
	size_t prefetch(const map &, const size_t &size, const opts & = opts_default);
	size_t evict(const map &, const size_t &size, const opts & = opts_default);
}

/// Interface to map file into memory.
///
/// Note that this was created specifically for file maps and not intended to
/// be a generic mmap(2) interface, at least for now.
struct ircd::fs::map
:mutable_buffer
{
	struct opts;

	static const opts default_opts;

	map() = default;
	map(const fd &, const opts &opts = default_opts, const size_t &size = 0UL);
	map(map &&) noexcept;
	map(const map &) = delete;
	map &operator=(map &&) noexcept;
	map &operator=(const map &) = delete;
	~map() noexcept;
};

/// Descriptor options (open options)
struct ircd::fs::map::opts
:fd::opts
{
	bool execute {false};
	bool shared {false};
	bool reserve {false};
	bool populate {false};
	bool locked {false};
	bool huge2mb {false};
	bool huge1gb {false};

	opts(const fd::opts &opts = {std::ios::in})
	:fd::opts(opts)
	{}
};

inline
ircd::fs::map::map(map &&other)
noexcept
:mutable_buffer{other}
{
	static_cast<mutable_buffer &>(other) = {};
}
