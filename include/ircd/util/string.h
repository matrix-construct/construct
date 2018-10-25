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
#define HAVE_IRCD_UTIL_STRING_H

//
// String generating patterns
//

namespace ircd::util
{
	std::string string(const char *const &buf, const size_t &size);
	std::string string(const uint8_t *const &buf, const size_t &size);
	std::string string(const size_t &size, const std::function<size_t (const mutable_buffer &)> &closure);
	std::string string(const size_t &size, const std::function<string_view (const mutable_buffer &)> &closure);
	template<class T> std::string string(const mutable_buffer &buf, const T &s);
	template<class T> std::string string(const T &s);
}

/// This is the ubiquitous ircd::string() template serving as the "toString()"
/// for the project. Particpating types that want to have a string(T)
/// returning an std::string must friend an operator<<(std::ostream &, T);
/// this is primarily for debug strings, not meant for performance or service.
///
template<class T>
std::string
ircd::util::string(const T &s)
{
	std::stringstream ss;
	ss << s;
	return ss.str();
}

/// Alternative to ircd::string() using a provided buffer for the
/// std::stringstream to avoid allocating one.
template<class T>
std::string
ircd::util::string(const mutable_buffer &buf,
                   const T &s)
{
	std::stringstream ss;
	pubsetbuf(ss, buf);
	ss << s;
	return ss.str();
}
