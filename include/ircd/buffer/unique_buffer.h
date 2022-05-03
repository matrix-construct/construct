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
	[[using gnu: malloc, alloc_align(1), alloc_size(2), returns_nonnull, warn_unused_result]]
	char *allocate(const size_t align, const size_t size);
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
:unique_buffer
{
	ircd::buffer::size(src)
}
{
	using ircd::buffer::size;

	assert(this->begin() != nullptr);
	assert(size(src) == size(*this));
	const auto ptr(std::get<0>(*this));
	const mutable_buffer dst
	{
		const_cast<char *>(ptr), size(src)
	};

	copy(dst, src);
}

template<class buffer_type>
inline
ircd::buffer::unique_buffer<buffer_type>::unique_buffer(const size_t &size,
                                                        const size_t &a)
:buffer_type
{
	allocator::allocate(a?: sizeof(void *), pad_to(size, a?: sizeof(void *))),
	size
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
	const auto ptr(std::get<0>(*this));
	std::free(const_cast<char *>(ptr));
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
