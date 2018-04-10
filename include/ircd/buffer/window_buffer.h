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
#define HAVE_IRCD_BUFFER_WINDOW_BUFFER_H

/// The window_buffer is just two mutable_buffers. One of the two buffers
/// just spans an underlying space and the other buffer is a window of the
/// remaining space which shrinks toward the end as the space is consumed.
/// The window_buffer object inherits from the latter, so it always has the
/// appearance of a mutable_buffer windowing on the the next place to write.
///
/// The recommended usage of this device is actually through the operator()
/// closure, which will automatically resize the window based on the return
/// value in the closure.
///
struct ircd::buffer::window_buffer
:mutable_buffer
{
	using closure = std::function<size_t (const mutable_buffer &)>;

	mutable_buffer base;

	size_t remaining() const;
	size_t consumed() const;

	const_buffer completed() const;
	explicit operator const_buffer() const;
	mutable_buffer completed();

	const_buffer operator()(const closure &closure);
	const_buffer rewind(const size_t &n = 1);

	window_buffer(const mutable_buffer &base);
	window_buffer() = default;
};

inline
ircd::buffer::window_buffer::window_buffer(const mutable_buffer &base)
:mutable_buffer{base}
,base{base}
{}

inline ircd::buffer::const_buffer
ircd::buffer::window_buffer::rewind(const size_t &n)
{
	const size_t nmax{std::min(n, consumed())};
	static_cast<mutable_buffer &>(*this).begin() -= nmax;
	assert(base.begin() <= begin());
	assert(begin() <= base.end());
	return completed();
}

inline ircd::buffer::const_buffer
ircd::buffer::window_buffer::operator()(const closure &closure)
{
	consume(*this, closure(*this));
	return completed();
}

/// View the completed portion of the stream
inline ircd::buffer::mutable_buffer
ircd::buffer::window_buffer::completed()
{
	assert(base.begin() <= begin());
	assert(base.begin() + consumed() <= base.end());
	return { base.begin(), base.begin() + consumed() };
}

/// Convenience conversion to get the completed portion
inline ircd::buffer::window_buffer::operator
const_buffer()
const
{
	return completed();
}

/// View the completed portion of the stream
inline ircd::buffer::const_buffer
ircd::buffer::window_buffer::completed()
const
{
	assert(base.begin() <= begin());
	assert(base.begin() + consumed() <= base.end());
	return { base.begin(), base.begin() + consumed() };
}

/// Bytes used by writes to the stream buffer
inline size_t
ircd::buffer::window_buffer::consumed()
const
{
	assert(begin() >= base.begin());
	assert(begin() <= base.end());
	return std::distance(base.begin(), begin());
}

/// Bytes remaining for writes to the stream buffer (same as size(*this))
inline size_t
ircd::buffer::window_buffer::remaining()
const
{
	assert(begin() <= base.end());
	const size_t ret(std::distance(begin(), base.end()));
	assert(ret == size(*this));
	return ret;
}
