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
#define HAVE_IRCD_BUFFER_UNIQUE_BUFFER_H

namespace ircd::buffer
{
	std::unique_ptr<char, decltype(&std::free)> aligned_alloc(const size_t &align, const size_t &size);
}

/// Like unique_ptr, this template holds ownership of an allocated buffer
///
template<class buffer>
struct ircd::buffer::unique_buffer
:buffer
{
	buffer release();

	unique_buffer(const size_t &size, const size_t &align = 0);
	explicit unique_buffer(const buffer &);
	unique_buffer();
	unique_buffer(unique_buffer &&) noexcept;
	unique_buffer(const unique_buffer &) = delete;
	unique_buffer &operator=(unique_buffer &&) noexcept;
	unique_buffer &operator=(const unique_buffer &) = delete;
	~unique_buffer() noexcept;
};

template<class buffer>
ircd::buffer::unique_buffer<buffer>::unique_buffer()
:buffer
{
	nullptr, nullptr
}
{}

template<class buffer>
ircd::buffer::unique_buffer<buffer>::unique_buffer(const buffer &src)
:unique_buffer
{
	size(src)
}
{
	const mutable_buffer dst
	{
		const_cast<char *>(data(*this)), size(*this)
	};

	copy(dst, src);
}

template<class buffer>
ircd::buffer::unique_buffer<buffer>::unique_buffer(const size_t &size,
                                                   const size_t &align)
:buffer
{
	aligned_alloc(align, size).release(), size
}
{
}

template<class buffer>
ircd::buffer::unique_buffer<buffer>::unique_buffer(unique_buffer &&other)
noexcept
:buffer
{
	std::move(static_cast<buffer &>(other))
}
{
	get<0>(other) = nullptr;
}

template<class buffer>
ircd::buffer::unique_buffer<buffer> &
ircd::buffer::unique_buffer<buffer>::operator=(unique_buffer &&other)
noexcept
{
	this->~unique_buffer();

	static_cast<buffer &>(*this) = std::move(static_cast<buffer &>(other));
	get<0>(other) = nullptr;

	return *this;
}

template<class buffer>
ircd::buffer::unique_buffer<buffer>::~unique_buffer()
noexcept
{
	std::free(const_cast<char *>(data(*this)));
}

template<class buffer>
buffer
ircd::buffer::unique_buffer<buffer>::release()
{
	const buffer ret{static_cast<buffer>(*this)};
	static_cast<buffer &>(*this) = buffer{};
	return ret;
}

inline std::unique_ptr<char, decltype(&std::free)>
ircd::buffer::aligned_alloc(const size_t &align,
                            const size_t &size)
{
	static const size_t &align_default{16};
	const size_t &alignment
	{
		align?: align_default
	};

	int errc;
	void *ret;
	if(unlikely((errc = ::posix_memalign(&ret, alignment, size)) != 0))
		throw std::system_error
		{
			errc, std::system_category()
		};

	assert(errc == 0);
	assert(ret != nullptr);
	return std::unique_ptr<char, decltype(&std::free)>
	{
		reinterpret_cast<char *>(ret), std::free
	};
}
