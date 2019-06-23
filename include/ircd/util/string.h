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
inline namespace util
{
	using string_closure_size = std::function<size_t (const mutable_buffer &)>;
	using string_closure_view = std::function<string_view (const mutable_buffer &)>;

	// OR this with a string size to trigger a shrink_to_fit() after closure.
	constexpr const size_t SHRINK_TO_FIT
	{
		1UL << (sizeof(size_t) * 8 - 1)
	};

	// Simple string generation by copy from existing buffer.
	std::string string(const char *const &buf, const size_t &size);
	std::string string(const uint8_t *const &buf, const size_t &size);
	std::string string(const const_buffer &);

	// String generation from closure. The closure is presented a buffer of
	// size for writing into. Closure returns how much it wrote via size or
	// view of written portion.
	std::string string(const size_t &size, const string_closure_size &);
	std::string string(const size_t &size, const string_closure_view &);

	// toString()'ish template suite (see defs)
	template<class T> std::string string(const mutable_buffer &buf, const T &s);
	template<class T> std::string string(const T &s);

	template<class F, class... A> std::string string_buffer(const size_t &, F&&, A&&...);
}}

/// Convenience template for working with various functions throughout IRCd
/// with the pattern `size_t func(mutable_buffer, ...)`. This function closes
/// over your function to supply the leading mutable_buffer and use the return
/// value of your function to satisfy util::string() as usual. The prototype
/// pattern works with the closures declared in this suite, so string_view
/// return types work too.
template<class F,
         class... A>
std::string
ircd::util::string_buffer(const size_t &size,
                          F&& function,
                          A&&... arguments)
{
	return string(size, [&function, &arguments...]
	(const mutable_buffer &buf)
	{
		return function(buf, std::forward<A>(arguments)...);
	});
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
