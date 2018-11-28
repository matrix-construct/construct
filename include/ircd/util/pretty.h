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
#define HAVE_IRCD_UTIL_PRETTY_H

namespace ircd::util
{
	// Human readable space suite
	using human_readable_size = std::tuple<uint64_t, long double, const string_view &>;

	// Default format strings used for pretty(size).
	extern const string_view pretty_size_fmt;
	extern const string_view pretty_only_size_fmt;

	// Process the value to be made pretty first with one of the following:
	human_readable_size iec(const uint64_t &value);
	human_readable_size si(const uint64_t &value);

	// Generate a pretty string with a custom fmt string (low-level)
	string_view pretty(const mutable_buffer &out, const string_view &fmt, const human_readable_size &);
	std::string pretty(const human_readable_size &, const string_view &fmt);

	// Generate a pretty string with the pretty_fmt_default implied.
	string_view pretty(const mutable_buffer &out, const human_readable_size &);
	std::string pretty(const human_readable_size &);

	// Generate a pretty string with another (simpler) default fmt string.
	string_view pretty_only(const mutable_buffer &out, const human_readable_size &);
	std::string pretty_only(const human_readable_size &);

	// Human readable time suite (for timers and counts; otherwise see date.h)
	string_view pretty_nanoseconds(const mutable_buffer &out, const long double &);
	template<class r, class p> string_view pretty(const mutable_buffer &out, const duration<r, p> &);
	template<class r, class p> std::string pretty(const duration<r, p> &);
}

template<class rep,
         class period>
std::string
ircd::util::pretty(const duration<rep, period> &d)
{
	return util::string(32, [&d]
	(const mutable_buffer &out)
	{
		return pretty(out, d);
	});
}

template<class rep,
         class period>
ircd::string_view
ircd::util::pretty(const mutable_buffer &out,
                   const duration<rep, period> &d)
{
	const auto &ns(duration_cast<nanoseconds>(d));
	return pretty_nanoseconds(out, ns.count());
}
