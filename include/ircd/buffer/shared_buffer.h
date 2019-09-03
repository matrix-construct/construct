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
#define HAVE_IRCD_BUFFER_SHARED_BUFFER_H

/// Like shared_ptr, this template shares ownership of an allocated buffer
///
template<class buffer>
struct ircd::buffer::shared_buffer
:buffer
{
	std::shared_ptr<char> sp;

	shared_buffer(unique_buffer<buffer> &&) noexcept;
	explicit shared_buffer(const size_t &size, const size_t &align = 0);
	explicit shared_buffer(const const_buffer &);
	shared_buffer() = default;
	shared_buffer(shared_buffer &&) = default;
	shared_buffer(const shared_buffer &) = default;
	shared_buffer &operator=(shared_buffer &&) = default;
	shared_buffer &operator=(const shared_buffer &) = default;
	shared_buffer &operator=(unique_buffer<buffer> &&) noexcept;
	~shared_buffer() = default;
};

template<class buffer>
ircd::buffer::shared_buffer<buffer>::shared_buffer(const const_buffer &src)
:shared_buffer
{
	unique_buffer<buffer>
	{
		src
	}
}
{}

template<class buffer>
ircd::buffer::shared_buffer<buffer>::shared_buffer(const size_t &size,
                                                   const size_t &align)
:shared_buffer
{
	unique_buffer<buffer>
	{
		size, align
	}
}
{}

template<class buffer>
ircd::buffer::shared_buffer<buffer>::shared_buffer(unique_buffer<buffer> &&buf)
noexcept
:buffer
{
	data(buf), size(buf)
}
,sp
{
	data(buf), &std::free
}
{
	buf.release();
}

template<class buffer>
ircd::buffer::shared_buffer<buffer> &
ircd::buffer::shared_buffer<buffer>::operator=(unique_buffer<buffer> &&buf)
noexcept
{
	sp.reset(data(buf), &std::free);
	*static_cast<buffer *>(this) = buffer
	{
		sp.get(), size(buf)
	};

	buf.release();
	return *this;
}
