// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_SIMD_GENERATE_H

namespace ircd::simd
{
	template<class block_t>
	using generate_fixed_proto = void (block_t &);

	template<class block_t>
	using generate_variable_proto = u64x2 (block_t &);

	template<class block_t,
	         class lambda>
	using generate_is_fixed_stride = std::is_same
	<
		std::invoke_result_t<lambda, block_t &>, void
	>;

	template<class block_t,
	         class lambda>
	using generate_is_variable_stride = std::is_same
	<
		std::invoke_result_t<lambda, block_t &>, u64x2
	>;

	template<class block_t,
	         class lambda>
	using generate_fixed_stride = std::enable_if
	<
		generate_is_fixed_stride<block_t, lambda>::value, u64x2
	>;

	template<class block_t,
	         class lambda>
	using generate_variable_stride = std::enable_if
	<
		generate_is_variable_stride<block_t, lambda>::value, u64x2
	>;

	template<class block_t,
	         class lambda>
	typename generate_fixed_stride<block_t, lambda>::type
	generate(block_t *, const u64x2, lambda&&) noexcept;

	template<class block_t,
	         class lambda>
	typename generate_fixed_stride<block_t, lambda>::type
	generate(char *, const u64x2, lambda&&) noexcept;

	template<class block_t,
	         class lambda>
	typename generate_variable_stride<block_t, lambda>::type
	generate(char *, const u64x2, lambda&&) noexcept;

	template<class block_t,
	         class lambda>
	mutable_buffer
	generate(const mutable_buffer &, lambda&&) noexcept;
}

/// Streaming generator
///
/// Convenience wrapper using mutable_buffer. This will forward to the
/// appropriate overload. The return buffer is a view on the input buffer
/// from the beginning up to the resulting counter value.
///
template<class block_t,
         class lambda>
inline ircd::mutable_buffer
ircd::simd::generate(const mutable_buffer &buf,
                     lambda&& closure)
noexcept
{
	const u64x2 max
	{
		size(buf), 0
	};

	const auto res
	{
		generate(data(buf), max, std::forward<lambda>(closure))
	};

	return mutable_buffer
	{
		data(buf), res[0]
	};
}

/// Streaming generator
///
/// This template performs the loop boiler-plate for the developer who can
/// simply supply a conforming closure. Characteristics:
///
/// * byte-aligned (unaligned): the output buffer does not have to be aligned
/// and can be any size.
///
/// * variable-stride: progress for each iteration of the loop across the
/// output and buffer is not fixed; the transform function may advance the
/// pointer one to sizeof(block_t) bytes each iteration.
///
/// u64x2 counter lanes = { output_length, available_to_user }; The argument
/// `max` gives the buffer size in that format. The return value is the
/// produced bytes (final counter value) in that format. The second lane is
/// available to the user; its initial value is max[1] (also unused); it is
/// then accumulated with the first lane of the closure's return value; its
/// final value is returned in [1] of the return value.
///
/// Note that the closure must advance the stream one or more bytes each
/// iteration; a zero value is available for loop control: the loop will
/// break without another iteration.
///
template<class block_t,
         class lambda>
inline typename ircd::simd::generate_variable_stride<block_t, lambda>::type
ircd::simd::generate(char *const __restrict__ out,
                     const u64x2 max,
                     lambda&& closure)
noexcept
{
	using block_t_u = unaligned<block_t>;

	u64x2 count
	{
		0,      // output pos
		max[1], // preserved for caller
	};

	u64x2 produce
	{
		-1UL,   // non-zero to start loop
		0,
	};

	// primary broadband loop
	while(produce[0] && count[0] + sizeof(block_t) <= max[0])
	{
		const auto di
		{
			reinterpret_cast<block_t_u *>(out + count[0])
		};

		auto &block
		(
			*di
		);

		produce = closure(block);
		count += produce;
	}

	// trailing narrowband loop
	while(produce[0] && count[0] < max[0])
	{
		block_t block {0};
		produce = closure(block);
		for(size_t i(0); count[0] + i < max[0] && i < sizeof(block_t); ++i)
			out[count[0] + i] = block[i];

		count += produce;
	}

	return u64x2
	{
		std::min(count[0], max[0]),
		count[1],
	};
}

/// Streaming generator
///
/// This template performs the loop boiler-plate for the developer who can
/// simply supply a conforming closure. Characteristics:
///
/// * byte-aligned (unaligned): the output buffer does not have to be aligned
/// and can be any size.
///
/// * fixed-stride: progress for each iteration of the loop across the output
/// buffer is fixed at the block width; the closure does not control the
/// iteration.
///
/// u64x2 counter lanes = { output_length, available_to_user }; The argument
/// `max` gives the buffer size in that format. The return value is the
/// produced bytes (final counter value) in that format. The second lane is
/// available to the user; its initial value is max[1] (also unused).
///
template<class block_t,
         class lambda>
inline typename ircd::simd::generate_fixed_stride<block_t, lambda>::type
ircd::simd::generate(char *const __restrict__ out,
                     const u64x2 max,
                     lambda&& closure)
noexcept
{
	using block_t_u = unaligned<block_t>;

	u64x2 count
	{
		0,      // output pos
		max[1], // preserved for caller
	};

	// primary broadband loop
	while(count[0] + sizeof(block_t) <= max[0])
	{
		static const u64x2 produce
		{
			sizeof(block_t), 0
		};

		const auto di
		{
			reinterpret_cast<block_t_u *>(out + count[0])
		};

		auto &block
		(
			*di
		);

		closure(block);
		count += produce;
	}

	// trailing narrowband loop
	assert(count[0] + sizeof(block_t) > max[0]);
	if(likely(count[0] < max[0]))
	{
		block_t block;
		closure(block);

		size_t i(0);
		for(; count[0] + i < max[0]; ++i)
			out[count[0] + i] = block[i];

		count += u64x2
		{
			i, 0
		};
	}

	assert(count[0] == max[0]);
	return count;
}

/// Streaming generator
///
/// This template performs the loop boiler-plate for the developer who can
/// simply supply a conforming closure. Characteristics:
///
/// * block-aligned
/// * fixed-stride
////
template<class block_t,
         class lambda>
inline typename ircd::simd::generate_fixed_stride<block_t, lambda>::type
ircd::simd::generate(block_t *const __restrict__ out,
                     const u64x2 max,
                     lambda&& closure)
noexcept
{
	u64x2 count
	{
		0,      // input pos
		max[1], // preserved for caller
	};

	// primary broadband loop
	while(count[0] < max[0])
	{
		static const u64x2 produce
		{
			sizeof(block_t), 0
		};

		const auto di
		{
			out + count[0] / sizeof(block_t)
		};

		auto &block
		(
			*di
		);

		closure(block);
		count += produce;
	}

	assert(count[0] + sizeof(block_t) > max[0]);
	assert(count[0] == max[0]);
	return count;
}
