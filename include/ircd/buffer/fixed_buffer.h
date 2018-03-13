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
#define HAVE_IRCD_BUFFER_FIXED_BUFFER_H

/// fixed_buffer wraps an std::array with construction and conversions apropos
/// the ircd::buffer suite. fixed_buffer should be punnable. Its only memory
/// footprint is the array itself and
///
template<class buffer,
         size_t SIZE>
struct ircd::buffer::fixed_buffer
:std::array<typename std::remove_const<typename buffer::value_type>::type, SIZE>
{
	using mutable_type = typename std::remove_const<typename buffer::value_type>::type;
	using const_type = typename std::add_const<mutable_type>::type;
	using array_type = std::array<mutable_type, SIZE>;

	operator buffer() const;
	operator buffer();

	using array_type::array_type;
	fixed_buffer(const nullptr_t &);
	fixed_buffer(const std::function<void (const mutable_buffer &)> &closure);
	fixed_buffer(buffer b);
	fixed_buffer() = default;
};

static_assert
(
	// Assertion over an arbitrary but common template configuration.
	std::is_standard_layout<ircd::buffer::fixed_buffer<ircd::buffer::const_buffer, 32>>::value,
	"ircd::buffer::fixed_buffer must be standard layout"
);

template<class buffer,
         size_t SIZE>
ircd::buffer::fixed_buffer<buffer, SIZE>::fixed_buffer(const nullptr_t &)
:array_type{{0}}
{}

template<class buffer,
         size_t SIZE>
ircd::buffer::fixed_buffer<buffer, SIZE>::fixed_buffer(const std::function<void (const mutable_buffer &)> &closure)
{
	closure(mutable_buffer{reinterpret_cast<mutable_buffer::iterator>(this->data()), this->size()});
}

template<class buffer,
         size_t SIZE>
ircd::buffer::fixed_buffer<buffer, SIZE>::fixed_buffer(buffer b)
:array_type{std::begin(b), std::end(b)}
{}

template<class buffer,
         size_t SIZE>
ircd::buffer::fixed_buffer<buffer, SIZE>::operator
buffer()
{
	return { std::begin(*this), std::end(*this) };
}

template<class buffer,
         size_t SIZE>
ircd::buffer::fixed_buffer<buffer, SIZE>::operator
buffer()
const
{
	return { std::begin(*this), std::end(*this) };
}
