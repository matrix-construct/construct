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
#define HAVE_IRCD_SIMD_FOR_EACH_H

namespace ircd::simd
{
	template<class block_t>
	using for_each_fixed_proto = void (block_t, block_t mask);

	template<class block_t>
	using for_each_variable_proto = u64x2 (block_t, block_t mask);

	template<class block_t,
	         class lambda>
	using for_each_is_fixed_stride = std::is_same
	<
		std::invoke_result_t<lambda, block_t, block_t>, void
	>;

	template<class block_t,
	         class lambda>
	using for_each_is_variable_stride = std::is_same
	<
		std::invoke_result_t<lambda, block_t, block_t>, u64x2
	>;

	template<class block_t,
	         class lambda>
	using for_each_fixed_stride = std::enable_if
	<
		for_each_is_fixed_stride<block_t, lambda>::value, u64x2
	>;

	template<class block_t,
	         class lambda>
	using for_each_variable_stride = std::enable_if
	<
		for_each_is_variable_stride<block_t, lambda>::value, u64x2
	>;

	template<class block_t,
	         class lambda>
	typename for_each_fixed_stride<block_t, lambda>::type
	for_each(const block_t *, const u64x2, lambda&&) noexcept;

	template<class block_t,
	         class lambda>
	typename for_each_fixed_stride<block_t, lambda>::type
	for_each(const char *, const u64x2, lambda&&) noexcept;

	template<class block_t,
	         class lambda>
	typename for_each_variable_stride<block_t, lambda>::type
	for_each(const char *, const u64x2, lambda&&) noexcept;

	template<class block_t,
	         class lambda>
	const_buffer
	for_each(const const_buffer &, lambda&&) noexcept;
}

/// Streaming consumer
///
/// Convenience wrapper using const_buffer. This will forward to the
/// appropriate overload. The return buffer is a view on the input buffer
/// from the beginning up to the resulting counter value.
///
template<class block_t,
         class lambda>
inline ircd::const_buffer
ircd::simd::for_each(const const_buffer &buf,
                     lambda&& closure)
noexcept
{
	const u64x2 max
	{
		0, size(buf)
	};

	const auto res
	{
		for_each(data(buf), max, std::forward<lambda>(closure))
	};

	return const_buffer
	{
		data(buf), res[1]
	};
}

/// Streaming consumer
///
/// This template performs the loop boiler-plate for the developer who can
/// simply supply a conforming closure. Characteristics:
///
/// * byte-aligned (unaligned): the input buffer does not have to be aligned
/// and can be any size.
///
/// * variable-stride: progress for each iteration of the loop across the input
/// and buffer is not fixed; the transform function may advance the pointer
/// one to sizeof(block_t) bytes each iteration. Due to these characteristics,
/// unaligned bytes may be redundantly loaded and non-temporal features are
/// not used to optimize the operation.
///
/// u64x2 counter lanes = { available_to_user, input_length }; The argument
/// `max` gives the buffer size in that format. The return value is the
/// consumed bytes (final counter value) in that format. The first lane is
/// available to the user; its initial value is max[0] (also unused); it is
/// then accumulated with the first lane of the closure's return value; its
/// final value is returned in [0] of the return value.
///
/// Note that the closure must advance the stream one or more bytes each
/// iteration; a zero value is available for loop control: the loop will
/// break without another iteration.
///
template<class block_t,
         class lambda>
inline typename ircd::simd::for_each_variable_stride<block_t, lambda>::type
ircd::simd::for_each(const char *const __restrict__ in,
                     const u64x2 max,
                     lambda&& closure)
noexcept
{
	using block_t_u = unaligned<block_t>;

	u64x2 count
	{
		max[0], // preserved for caller
		0,      // input pos
	};

	u64x2 consume
	{
		0,
		-1UL    // non-zero to start loop
	};

	// primary broadband loop
	while(consume[1] && count[1] + sizeof(block_t) <= max[1])
	{
		static const auto mask
		{
			mask_full<block_t>()
		};

		const auto si
		{
			reinterpret_cast<const block_t_u *>(in + count[1])
		};

		const block_t block
		(
			*si
		);

		consume = closure(block, mask);
		count += consume;
	}

	// trailing narrowband loop
	while(consume[1] && count[1] < max[1])
	{
		block_t block {0}, mask {0};
		for(size_t i(0); count[1] + i < max[1] && i < sizeof(block_t); ++i)
		{
			block[i] = in[count[1] + i];
			mask[i] = 0xff;
		}

		consume = closure(block, mask);
		count += consume;
	}

	return u64x2
	{
		count[0],
		std::min(count[1], max[1])
	};
}

/// Streaming consumer
///
/// This template performs the loop boiler-plate for the developer who can
/// simply supply a conforming closure. Characteristics:
///
/// * byte-aligned (unaligned): the input buffer does not have to be aligned
/// and can be any size.
///
/// * fixed-stride: progress for each iteration of the loop across the input
/// and buffer is fixed at the block width; the transform function does not
/// control the iteration.
///
/// u64x2 counter lanes = { available_to_user, input_length }; The argument
/// `max` gives the buffer size in that format. The return value is the
/// consumed bytes (final counter value) in that format. The first lane is
/// available to the user; its initial value is max[0] (also unused).
///
template<class block_t,
         class lambda>
inline typename ircd::simd::for_each_fixed_stride<block_t, lambda>::type
ircd::simd::for_each(const char *const __restrict__ in,
                     const u64x2 max,
                     lambda&& closure)
noexcept
{
	using block_t_u = unaligned<block_t>;

	u64x2 count
	{
		max[0], // preserved for caller
		0,      // input pos
	};

	// primary broadband loop
	while(count[1] + sizeof(block_t) <= max[1])
	{
		static const u64x2 consume
		{
			0, sizeof(block_t)
		};

		static const auto mask
		{
			mask_full<block_t>()
		};

		const auto si
		{
			reinterpret_cast<const block_t_u *>(in + count[1])
		};

		const block_t block
		(
			*si
		);

		closure(block, mask);
		count += consume;
	}

	// trailing narrowband loop
	assert(count[1] + sizeof(block_t) > max[1]);
	if(likely(count[1] < max[1]))
	{
		size_t i(0);
		block_t block {0}, mask {0};
		for(; count[1] + i < max[1]; ++i)
		{
			block[i] = in[count[1] + i];
			mask[i] = 0xff;
		}

		closure(block, mask);
		count += u64x2 // consume remainder
		{
			0, i
		};
	}

	assert(count[0] == max[0]);
	return count;
}

/// Streaming consumer
///
/// This template performs the loop boiler-plate for the developer who can
/// simply supply a conforming closure. Characteristics:
///
/// * block-aligned
/// * fixed-stride
////
template<class block_t,
         class lambda>
inline typename ircd::simd::for_each_fixed_stride<block_t, lambda>::type
ircd::simd::for_each(const block_t *const __restrict__ in,
                     const u64x2 max,
                     lambda&& closure)
noexcept
{
	u64x2 count
	{
		max[0], // preserved for caller
		0,      // input pos
	};

	// primary broadband loop
	while(count[1] < max[1])
	{
		static const u64x2 consume
		{
			0, sizeof(block_t)
		};

		static const auto mask
		{
			mask_full<block_t>()
		};

		const auto si
		{
			in + count[1] / sizeof(block_t)
		};

		const block_t block
		(
			*si
		);

		closure(block, mask);
		count += consume;
	}

	assert(count[1] + sizeof(block_t) > max[1]);
	assert(count[0] == max[0]);
	return count;
}
