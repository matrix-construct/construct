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

// Fwd decl; circ dep.
namespace ircd::allocator
{
	std::unique_ptr<char, decltype(&std::free)>
	aligned_alloc(const size_t &align, const size_t &size);
}

/// Like unique_ptr, this template holds ownership of an allocated buffer
///
template<class buffer_type>
struct ircd::buffer::unique_buffer
:buffer_type
{
	explicit operator bool() const;
	bool operator!() const;

	buffer_type release();

	unique_buffer() = default;
	unique_buffer(const size_t &size, const size_t &align = 0);
	explicit unique_buffer(const const_buffer &);
	unique_buffer(unique_buffer &&) noexcept;
	unique_buffer(const unique_buffer &) = delete;
	unique_buffer &operator=(unique_buffer &&) & noexcept;
	unique_buffer &operator=(const unique_buffer &) = delete;
	~unique_buffer() noexcept;
};

template<class buffer_type>
inline
ircd::buffer::unique_buffer<buffer_type>::unique_buffer(const const_buffer &src)
:buffer_type
{
	allocator::aligned_alloc(0, ircd::buffer::size(src)).release(), ircd::buffer::size(src)
}
{
	using ircd::buffer::size;

	assert(this->begin() != nullptr);
	assert(size(src) == size(*this));
	const mutable_buffer dst
	{
		const_cast<char *>(this->begin()), size(src)
	};

	copy(dst, src);
}

template<class buffer_type>
inline
ircd::buffer::unique_buffer<buffer_type>::unique_buffer(const size_t &size,
                                                        const size_t &align)
:buffer_type
{
	allocator::aligned_alloc(align, size).release(), size
}
{}

template<class buffer_type>
inline
ircd::buffer::unique_buffer<buffer_type>::unique_buffer(unique_buffer &&other)
noexcept
:buffer_type
{
	other.release()
}
{
	assert(std::get<0>(other) == nullptr);
}

template<class buffer_type>
inline ircd::buffer::unique_buffer<buffer_type> &
ircd::buffer::unique_buffer<buffer_type>::operator=(unique_buffer &&other)
& noexcept
{
	this->~unique_buffer();

	static_cast<buffer_type &>(*this) = other.release();
	assert(std::get<0>(other) == nullptr);

	return *this;
}

template<class buffer_type>
inline
ircd::buffer::unique_buffer<buffer_type>::~unique_buffer()
noexcept
{
	std::free(const_cast<char *>(this->begin()));
}

template<class buffer_type>
inline buffer_type
ircd::buffer::unique_buffer<buffer_type>::release()
{
	const buffer_type ret{*this};
	std::get<0>(*this) = nullptr;
	std::get<1>(*this) = nullptr;
	return ret;
}

template<class buffer_type>
inline bool
ircd::buffer::unique_buffer<buffer_type>::operator!()
const
{
	return this->buffer_type::empty();
}

template<class buffer_type>
inline ircd::buffer::unique_buffer<buffer_type>::operator
bool()
const
{
	return !this->buffer_type::empty();
}
