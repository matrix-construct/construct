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

/// Like unique_ptr, this template holds ownership of an allocated buffer
///
template<class buffer,
         uint alignment>
struct ircd::buffer::unique_buffer
:buffer
{
	unique_buffer(std::unique_ptr<uint8_t[]> &&, const size_t &size);
	unique_buffer(const size_t &size);
	unique_buffer();
	unique_buffer(unique_buffer &&) noexcept;
	unique_buffer(const unique_buffer &) = delete;
	unique_buffer &operator=(unique_buffer &&) noexcept;
	unique_buffer &operator=(const unique_buffer &) = delete;
	~unique_buffer() noexcept;
};

template<class buffer,
         uint alignment>
ircd::buffer::unique_buffer<buffer, alignment>::unique_buffer()
:buffer
{
	nullptr, nullptr
}
{}

template<class buffer,
         uint alignment>
ircd::buffer::unique_buffer<buffer, alignment>::unique_buffer(std::unique_ptr<uint8_t[]> &&b,
                                                              const size_t &size)
:buffer
{
	typename buffer::iterator(b.release()), size
}
{}

template<class buffer,
         uint alignment>
ircd::buffer::unique_buffer<buffer, alignment>::unique_buffer(const size_t &size)
:unique_buffer<buffer, alignment>
{
	std::unique_ptr<uint8_t[]>
	{
		//TODO: Can't use a template parameter to the attribute even though
		// it's known at compile time. Hardcoding this until fixed with better
		// aligned dynamic memory.
		//new __attribute__((aligned(alignment))) uint8_t[size]
		new __attribute__((aligned(16))) uint8_t[size]
	},
	size
}
{
	// Alignment can only be 16 bytes for now
	assert(alignment == 16);
}

template<class buffer,
         uint alignment>
ircd::buffer::unique_buffer<buffer, alignment>::unique_buffer(unique_buffer &&other)
noexcept
:buffer
{
	std::move(static_cast<buffer &>(other))
}
{
	get<0>(other) = nullptr;
}

template<class buffer,
         uint alignment>
ircd::buffer::unique_buffer<buffer, alignment> &
ircd::buffer::unique_buffer<buffer, alignment>::operator=(unique_buffer &&other)
noexcept
{
	this->~unique_buffer();

	static_cast<buffer &>(*this) = std::move(static_cast<buffer &>(other));
	get<0>(other) = nullptr;

	return *this;
}

template<class buffer,
         uint alignment>
ircd::buffer::unique_buffer<buffer, alignment>::~unique_buffer()
noexcept
{
	delete[] data(*this);
}
