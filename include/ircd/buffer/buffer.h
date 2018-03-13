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
#define HAVE_IRCD_BUFFER_H

// Forward declarations from boost::asio because it is not included here. IRCd
// buffers are not based directly on the boost ones but are easily converted
// when passing our buffer to an asio function.
namespace boost::asio
{
	struct const_buffer;
	struct mutable_buffer;
}

/// Lightweight buffer interface compatible with boost::asio IO buffers and vectors
///
/// A const_buffer is a pair of iterators like `const char *` meant for sending
/// data; a mutable_buffer is a pair of iterators meant for receiving.
///
/// These templates offer tools for individual buffers as well as tools for
/// iterations of buffers.  An iteration of buffers is an iovector that is
/// passed to our sockets etc. The ircd::iov template can host an iteration of
/// buffers. The `template template` functions are tools for a container of
/// buffers of any permutation.
///
namespace ircd::buffer
{
	template<class it> struct buffer;
	struct const_buffer;
	struct mutable_buffer;
	struct window_buffer;
	template<class buffer, size_t SIZE> struct fixed_buffer;
	template<class buffer, uint align = 16> struct unique_buffer;
	template<class buffer> struct shared_buffer;

	template<size_t SIZE> using fixed_const_buffer = fixed_buffer<const_buffer, SIZE>;
	template<size_t SIZE> using fixed_mutable_buffer = fixed_buffer<mutable_buffer, SIZE>;
	template<template<class> class I> using const_buffers = I<const_buffer>;
	template<template<class> class I> using mutable_buffers = I<mutable_buffer>;

	// Preconstructed null buffers
	extern const mutable_buffer null_buffer;
	extern const ilist<mutable_buffer> null_buffers;

	// Single buffer iteration of contents
	template<class it> const it &begin(const buffer<it> &buffer);
	template<class it> const it &end(const buffer<it> &buffer);
	template<class it> it &begin(buffer<it> &buffer);
	template<class it> it &end(buffer<it> &buffer);
	template<class it> std::reverse_iterator<it> rbegin(const buffer<it> &buffer);
	template<class it> std::reverse_iterator<it> rend(const buffer<it> &buffer);

	// Single buffer tools
	template<class it> bool null(const buffer<it> &buffer);
	template<class it> bool full(const buffer<it> &buffer);
	template<class it> bool empty(const buffer<it> &buffer);
	template<class it> bool operator!(const buffer<it> &buffer);
	template<class it> size_t size(const buffer<it> &buffer);
	template<class it> const it &data(const buffer<it> &buffer);
	template<class it> size_t consume(buffer<it> &buffer, const size_t &bytes);
	template<class it> buffer<it> operator+(const buffer<it> &buffer, const size_t &bytes);
	template<class it> it copy(it &dest, const it &stop, const const_buffer &);
	template<size_t SIZE> size_t copy(const mutable_buffer &dst, const char (&buf)[SIZE]);
	size_t copy(const mutable_buffer &dst, const const_buffer &src);
	size_t reverse(const mutable_buffer &dst, const const_buffer &src);
	void reverse(const mutable_buffer &buf);
	void zero(const mutable_buffer &buf);

	// Iterable of buffers tools
	template<template<class> class I, class T> size_t size(const I<T> &buffers);
	template<template<class> class I, class T> size_t copy(const mutable_buffer &, const I<T> &buffer);
	template<template<class> class I, class T> size_t consume(I<T> &buffers, const size_t &bytes);

	// Convenience copy to std stream
	template<class it> std::ostream &operator<<(std::ostream &s, const buffer<it> &buffer);
	template<template<class> class I, class T> std::ostream &operator<<(std::ostream &s, const I<T> &buffers);
}

#include "buffer_base.h"
#include "mutable_buffer.h"
#include "const_buffer.h"
#include "fixed_buffer.h"
#include "window_buffer.h"
#include "unique_buffer.h"
#include "shared_buffer.h"

// Export these important aliases down to main ircd namespace
namespace ircd
{
	using buffer::const_buffer;
	using buffer::mutable_buffer;
	using buffer::fixed_buffer;
	using buffer::unique_buffer;
	using buffer::shared_buffer;
	using buffer::null_buffer;
	using buffer::window_buffer;
	using buffer::fixed_const_buffer;
	using buffer::fixed_mutable_buffer;

	using buffer::const_buffers;
	using buffer::mutable_buffers;

	using buffer::size;
	using buffer::data;
	using buffer::copy;
	using buffer::consume;
}

template<template<class>
         class buffers,
         class T>
std::ostream &
ircd::buffer::operator<<(std::ostream &s, const buffers<T> &b)
{
	using it = typename T::iterator;

	std::for_each(std::begin(b), std::end(b), [&s]
	(const buffer<it> &b)
	{
		s << b;
	});

	return s;
}

template<template<class>
         class buffers,
         class T>
size_t
ircd::buffer::consume(buffers<T> &b,
                      const size_t &bytes)
{
	ssize_t remain(bytes);
	for(auto it(std::begin(b)); it != std::end(b) && remain > 0; ++it)
	{
		using buffer = typename buffers<T>::value_type;
		using iterator = typename buffer::iterator;

		buffer &b(const_cast<buffer &>(*it));
		const ssize_t bsz(size(b));
		const ssize_t csz{std::min(remain, bsz)};
		remain -= consume(b, csz);
		assert(remain >= 0);
	}

	assert(ssize_t(bytes) >= remain);
	return bytes - remain;
}

template<template<class>
         class buffers,
         class T>
size_t
ircd::buffer::copy(const mutable_buffer &dest,
                   const buffers<T> &b)
{
	using it = typename T::iterator;

	size_t ret(0);
	for(const buffer<it> &b : b)
		ret += copy(data(dest) + ret, size(dest) - ret, b);

	return ret;
}

template<template<class>
         class buffers,
         class T>
size_t
ircd::buffer::size(const buffers<T> &b)
{
	using it = typename T::iterator;

	return std::accumulate(std::begin(b), std::end(b), size_t(0), []
	(auto ret, const buffer<it> &b)
	{
		return ret += size(b);
	});
}

template<class it>
std::ostream &
ircd::buffer::operator<<(std::ostream &s, const buffer<it> &buffer)
{
	assert(!null(buffer) || get<1>(buffer) == nullptr);
	s.write(data(buffer), size(buffer));
	return s;
}

inline void
ircd::buffer::reverse(const mutable_buffer &buf)
{
	std::reverse(data(buf), data(buf) + size(buf));
}

inline size_t
ircd::buffer::reverse(const mutable_buffer &dst,
                      const const_buffer &src)
{
	const size_t ret{std::min(size(dst), size(src))};
	std::reverse_copy(data(src), data(src) + ret, data(dst));
	return ret;
}

template<size_t SIZE>
__attribute__((error
(
	"Copy source is an array. Is this a string literal? Do you want to copy the \\0?"
	" Disambiguate this by typing the source string_view or const_buffer."
)))
inline size_t
ircd::buffer::copy(const mutable_buffer &dst,
                   const char (&buf)[SIZE])
{
	return copy(dst, const_buffer{buf});
}

inline size_t
ircd::buffer::copy(const mutable_buffer &dst,
                   const const_buffer &src)
{
	auto e{begin(dst)};
	copy(e, end(dst), src);
	assert(std::distance(begin(dst), e) >= 0);
	return std::distance(begin(dst), e);
}

template<class it>
it
ircd::buffer::copy(it &dest,
                   const it &stop,
                   const const_buffer &src)
{
	const it ret{dest};
	const ssize_t srcsz(size(src));
	assert(ret <= stop);
	const ssize_t remain{std::distance(ret, stop)};
	const ssize_t cpsz{std::min(srcsz, remain)};
	assert(cpsz <= srcsz);
	assert(cpsz <= remain);
	assert(remain >= 0);
	memcpy(ret, data(src), cpsz);
	dest += cpsz;
	assert(dest <= stop);
	return ret;
}

template<class it>
ircd::buffer::buffer<it>
ircd::buffer::operator+(const buffer<it> &buffer,
                        const size_t &bytes)
{
	const size_t advance
	{
		std::min(bytes, size(buffer))
	};

	return
	{
		begin(buffer) + advance, size(buffer) - advance
	};
}

template<class it>
size_t
ircd::buffer::consume(buffer<it> &buffer,
                      const size_t &bytes)
{
	assert(!null(buffer));
	assert(bytes <= size(buffer));
	get<0>(buffer) += bytes;
	assert(get<0>(buffer) <= get<1>(buffer));
	return size(buffer);
}

template<class it>
const it &
ircd::buffer::data(const buffer<it> &buffer)
{
	return get<0>(buffer);
}

template<class it>
size_t
ircd::buffer::size(const buffer<it> &buffer)
{
	assert(get<0>(buffer) <= get<1>(buffer));
	assert(!null(buffer) || get<1>(buffer) == nullptr);
	return std::distance(get<0>(buffer), get<1>(buffer));
}

template<class it>
bool
ircd::buffer::operator!(const buffer<it> &buffer)
{
	return empty(buffer);
}

template<class it>
bool
ircd::buffer::empty(const buffer<it> &buffer)
{
	return null(buffer) || std::distance(get<0>(buffer), get<1>(buffer)) == 0;
}

template<class it>
bool
ircd::buffer::full(const buffer<it> &buffer)
{
	return std::distance(get<0>(buffer), get<1>(buffer)) == 0;
}

template<class it>
bool
ircd::buffer::null(const buffer<it> &buffer)
{
	return get<0>(buffer) == nullptr;
}

template<class it>
std::reverse_iterator<it>
ircd::buffer::rend(const buffer<it> &buffer)
{
	return std::reverse_iterator<it>(get<0>(buffer));
}

template<class it>
std::reverse_iterator<it>
ircd::buffer::rbegin(const buffer<it> &buffer)
{
	return std::reverse_iterator<it>(get<0>(buffer) + size(buffer));
}

template<class it>
it &
ircd::buffer::end(buffer<it> &buffer)
{
	return get<1>(buffer);
}

template<class it>
it &
ircd::buffer::begin(buffer<it> &buffer)
{
	return get<0>(buffer);
}

template<class it>
const it &
ircd::buffer::end(const buffer<it> &buffer)
{
	return get<1>(buffer);
}

template<class it>
const it &
ircd::buffer::begin(const buffer<it> &buffer)
{
	return get<0>(buffer);
}

template<class it>
ircd::buffer::buffer<it>::operator std::string()
const
{
	return { reinterpret_cast<const char *>(data(*this)), size(*this) };
}

template<class it>
ircd::buffer::buffer<it>::operator std::string_view()
const
{
	return { reinterpret_cast<const char *>(data(*this)), size(*this) };
}

template<class it>
ircd::buffer::buffer<it>::operator string_view()
const
{
	return { reinterpret_cast<const char *>(data(*this)), size(*this) };
}
