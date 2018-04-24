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

namespace ircd {
namespace util {

/// This is the ubiquitous ircd::string() template serving as the "toString()"
/// for the project. Particpating types that want to have a string(T)
/// returning an std::string must friend an operator<<(std::ostream &, T);
/// this is primarily for debug strings, not meant for performance or service.
///
template<class T>
auto
string(const T &s)
{
	std::stringstream ss;
	ss << s;
	return ss.str();
}

inline auto
string(const char *const &buf,
       const size_t &size)
{
	return std::string{buf, size};
}

inline auto
string(const uint8_t *const &buf,
       const size_t &size)
{
	return string(reinterpret_cast<const char *>(buf), size);
}

/// Close over the common pattern to write directly into a post-C++11 standard
/// string through the data() member requiring a const_cast. Closure returns
/// the final size of the data written into the buffer.
inline auto
string(const size_t &size,
       const std::function<size_t (const mutable_buffer &)> &closure)
{
	std::string ret(size, char{});
	const mutable_buffer buf
	{
		const_cast<char *>(ret.data()), ret.size()
	};

	const size_t consumed
	{
		closure(buf)
	};

	assert(consumed <= buffer::size(buf));
	data(buf)[consumed] = '\0';
	ret.resize(consumed);
	return ret;
}

/// Close over the common pattern to write directly into a post-C++11 standard
/// string through the data() member requiring a const_cast. Closure returns
/// a view of the data actually written to the buffer.
inline auto
string(const size_t &size,
       const std::function<string_view (const mutable_buffer &)> &closure)
{
	return string(size, [&closure]
	(const mutable_buffer &buffer)
	{
		return ircd::size(closure(buffer));
	});
}

} // namespace util
} // namespace ircd
