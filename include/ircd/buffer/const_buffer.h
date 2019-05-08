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
#define HAVE_IRCD_BUFFER_CONST_BUFFER_H

struct ircd::buffer::const_buffer
:buffer<const char *>
{
	// Definition for this is somewhere in the .cc files where boost is incl.
	operator boost::asio::const_buffer() const;

	// For boost::spirit conceptual compliance; illegal/noop
	void insert(const char *const &, const char &);

	using buffer<const char *>::buffer;
	template<size_t SIZE> const_buffer(const char (&buf)[SIZE]);
	template<size_t SIZE> const_buffer(const std::array<char, SIZE> &buf);
	const_buffer(const buffer<const char *> &b);
	const_buffer(const buffer<char *> &b);
	const_buffer(const mutable_buffer &b);
	const_buffer(const string_view &s);
	const_buffer() = default;
};

inline
__attribute__((always_inline))
ircd::buffer::const_buffer::const_buffer(const buffer<const char *> &b)
:buffer<const char *>{b}
{}

inline
__attribute__((always_inline))
ircd::buffer::const_buffer::const_buffer(const buffer<char *> &b)
:buffer<const char *>{data(b), size(b)}
{}

inline
__attribute__((always_inline))
ircd::buffer::const_buffer::const_buffer(const mutable_buffer &b)
:buffer<const char *>{data(b), size(b)}
{}

inline
__attribute__((always_inline))
ircd::buffer::const_buffer::const_buffer(const string_view &s)
:buffer<const char *>{data(s), size(s)}
{}

template<size_t SIZE>
inline __attribute__((always_inline))
ircd::buffer::const_buffer::const_buffer(const char (&buf)[SIZE])
:buffer<const char *>{buf, SIZE}
{}

template<size_t SIZE>
inline __attribute__((always_inline))
ircd::buffer::const_buffer::const_buffer(const std::array<char, SIZE> &buf)
:buffer<const char *>{reinterpret_cast<const char *>(buf.data()), SIZE}
{}

#ifndef _NDEBUG
__attribute__((noreturn))
#endif
inline void
ircd::buffer::const_buffer::insert(const char *const &,
                                   const char &)
{
	assert(0);
}
