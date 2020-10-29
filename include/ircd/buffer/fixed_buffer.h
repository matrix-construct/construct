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
template<class buffer_type,
         size_t SIZE>
struct ircd::buffer::fixed_buffer
:std::array<typename std::remove_const<typename buffer_type::value_type>::type, SIZE>
{
	using mutable_type = typename std::remove_const<typename buffer_type::value_type>::type;
	using const_type = typename std::add_const<mutable_type>::type;
	using array_type = std::array<mutable_type, SIZE>;
	using proto_type = void (const mutable_buffer &);
	using args_type = const mutable_buffer &;

	operator buffer_type() const;
	operator buffer_type();

	using array_type::array_type;
	fixed_buffer(buffer_type b);
	explicit fixed_buffer(nullptr_t);

	template<class F>
	fixed_buffer(F&&,
	             typename std::enable_if<std::is_invocable<F, args_type>::value, void>::type * = nullptr);

	fixed_buffer() = default;
};

static_assert
(
	// Assertion over an arbitrary but common template configuration.
	std::is_standard_layout<ircd::buffer::fixed_buffer<ircd::buffer::const_buffer, 32>>::value,
	"ircd::buffer::fixed_buffer must be standard layout"
);

template<class buffer_type,
         size_t SIZE>
template<class F>
inline
ircd::buffer::fixed_buffer<buffer_type, SIZE>::fixed_buffer(F&& closure,
                                                            typename std::enable_if<std::is_invocable<F, args_type>::value, void>::type *)
{
	closure(mutable_buffer
	{
		reinterpret_cast<mutable_buffer::iterator>(this->data()),
		this->size()
	});
}

template<class buffer_type,
         size_t SIZE>
inline
ircd::buffer::fixed_buffer<buffer_type, SIZE>::fixed_buffer(nullptr_t)
:array_type{{0}}
{}

template<class buffer_type,
         size_t SIZE>
inline
ircd::buffer::fixed_buffer<buffer_type, SIZE>::fixed_buffer(buffer_type b)
:array_type{std::begin(b), std::end(b)}
{}

template<class buffer_type,
         size_t SIZE>
inline
ircd::buffer::fixed_buffer<buffer_type, SIZE>::operator
buffer_type()
{
	return buffer_type
	{
		std::begin(*this), std::end(*this)
	};
}

template<class buffer_type,
         size_t SIZE>
inline
ircd::buffer::fixed_buffer<buffer_type, SIZE>::operator
buffer_type()
const
{
	return buffer_type
	{
		std::begin(*this), std::end(*this)
	};
}
