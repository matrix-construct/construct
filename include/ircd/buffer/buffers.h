// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_BUFFER_BUFFERS_H

namespace ircd::buffer::buffers
{
	template<template<class> class I> using const_buffers = I<const_buffer>;
	template<template<class> class I> using mutable_buffers = I<mutable_buffer>;

	// Convenience null buffers
	extern const ilist<mutable_buffer> null_buffers;

	// Iterable of buffers tools
	template<template<class> class I, class T> size_t size(const I<T> &buffers);
	template<template<class> class I, class T> size_t copy(const mutable_buffer &, const I<T> &buffer);
	template<template<class> class I, class T> size_t consume(I<T> &buffers, const size_t &bytes);
	template<template<class> class I, class T> std::ostream &operator<<(std::ostream &s, const I<T> &buffers);
}

template<template<class>
         class buffers,
         class T>
std::ostream &
ircd::buffer::buffers::operator<<(std::ostream &s, const buffers<T> &b)
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
ircd::buffer::buffers::consume(buffers<T> &b,
                               const size_t &bytes)
{
	ssize_t remain(bytes);
	for(auto it(std::begin(b)); it != std::end(b) && remain > 0; ++it)
	{
		using buffer = typename buffers<T>::value_type;
		using iterator = typename buffer::iterator;
		using ircd::buffer::size;
		using ircd::buffer::consume;

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
ircd::buffer::buffers::copy(const mutable_buffer &dest,
                            const buffers<T> &b)
{
	using it = typename T::iterator;
	using ircd::buffer::copy;
	using ircd::buffer::size;

	size_t ret(0);
	for(const buffer<it> &b : b)
		ret += copy(dest + ret, b);

	return ret;
}

template<template<class>
         class buffers,
         class T>
size_t
ircd::buffer::buffers::size(const buffers<T> &b)
{
	using it = typename T::iterator;
	using ircd::buffer::size;

	return std::accumulate(std::begin(b), std::end(b), size_t(0), []
	(auto ret, const buffer<it> &b)
	{
		return ret += size(b);
	});
}
