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
:std::shared_ptr<char>
,buffer
{
	shared_buffer();
	explicit shared_buffer(const size_t &size, const size_t &align = 0);
	shared_buffer(unique_buffer<buffer> &&) noexcept;
	explicit shared_buffer(const const_buffer &);
	shared_buffer(shared_buffer &&) = default;
	shared_buffer(const shared_buffer &) = default;
	shared_buffer &operator=(shared_buffer &&) = default;
	shared_buffer &operator=(const shared_buffer &) = default;
	shared_buffer &operator=(unique_buffer<buffer> &&) noexcept;
	~shared_buffer() = default;
};

template<class buffer>
ircd::buffer::shared_buffer<buffer>::shared_buffer()
:std::shared_ptr<char>
{
	nullptr, std::free
}
{}

template<class buffer>
ircd::buffer::shared_buffer<buffer>::shared_buffer(const const_buffer &src)
:shared_buffer
{
	size(src)
}
{
	const mutable_buffer dst{data(*this), size(*this)};
	assert(size(dst) == size(src));
	copy(dst, src);
}

template<class buffer>
ircd::buffer::shared_buffer<buffer>::shared_buffer(unique_buffer<buffer> &&buf)
noexcept
:std::shared_ptr<char>
{
	data(buf), std::free
}
,buffer
{
	this->std::shared_ptr<char>::get(), size(buf)
}
{
	buf.release();
}

template<class buffer>
ircd::buffer::shared_buffer<buffer>::shared_buffer(const size_t &size,
                                                   const size_t &align)
:std::shared_ptr<char>
{
	allocator::aligned_alloc(align, size).release(), std::free
}
,buffer
{
	this->std::shared_ptr<char>::get(), size
}
{}

template<class buffer>
ircd::buffer::shared_buffer<buffer> &
ircd::buffer::shared_buffer<buffer>::operator=(unique_buffer<buffer> &&buf)
noexcept
{
	this->std::shared_ptr<char>::reset(data(buf), std::free);
	*static_cast<buffer *>(this) =
	{
		this->std::shared_ptr<char>::get(), size(buf)
	};

	buf.release();
	return *this;
}
