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
#define HAVE_IRCD_BUFFER_MOVE_H

// copy overlapping regions
namespace ircd::buffer
{
	char *&move(char *&dest, char *const &stop, const const_buffer &src);
	size_t move(const mutable_buffer &dst, const const_buffer &src);
	template<size_t SIZE> size_t move(const mutable_buffer &dst, const char (&)[SIZE]);
}

template<size_t SIZE>
#ifndef __clang__
__attribute__((error
#else
__attribute__((unavailable
#endif
(
	"Move source is an array. Is this a string literal? Do you want to move the \\0?"
	" Disambiguate this by typing the source string_view or const_buffer."
)))
inline size_t
ircd::buffer::move(const mutable_buffer &dst,
                   const char (&buf)[SIZE])
{
	return move(dst, const_buffer{buf});
}

inline size_t
ircd::buffer::move(const mutable_buffer &dst,
                   const const_buffer &src)
{
	char *const &s(begin(dst)), *e(s);
	e = move(e, end(dst), src);
	assert(std::distance(s, e) >= 0);
	return std::distance(s, e);
}

inline char *&
__attribute__((always_inline))
ircd::buffer::move(char *&dest,
                   char *const &stop,
                   const const_buffer &src)
{
	assert(dest <= stop);
	const size_t remain
	(
		std::distance(dest, stop)
	);

	const size_t cpsz
	{
		std::min(size(src), remain)
	};

	assert(cpsz <= size(src));
	assert(cpsz <= remain);
	__builtin_memmove(dest, data(src), cpsz);
	dest += cpsz;
	assert(dest <= stop);
	return dest;
}
