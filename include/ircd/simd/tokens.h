// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_SIMD_TOKENS_H

namespace ircd::simd
{
	template<class token_block_t,
             class input_block_t>
	using tokens_prototype = u64x2 (token_block_t &, input_block_t, input_block_t mask);

	template<class input_t,
	         class block_t,
	         class lambda>
	u64x2 tokens(block_t *, const char *, const u64x2, lambda&&) noexcept;
}

/// Tokenize a character string input into fixed-width elements of the output
/// array.
///
/// The input streams variably with character (byte) resolution; the return
/// value for the input side of the closure advances the input one or more
/// bytes for the next iteration.
///
/// The output streams variably with element (lane) resolution; the return
/// value for the output side of the closure advances the output zero or more
/// tokens at a time. Note iff the token type is u8 this value is a byte incr.
///
/// The closure performs the transformation for as many tokens as the block
/// size allows for at a time. Closure returns the number of tokens written
/// to the output block, and number of characters to consume off the input
/// block for the next iteration.
/// 
template<class input_t,
         class block_t,
         class lambda>
inline ircd::u64x2
ircd::simd::tokens(block_t *const __restrict__ out_,
                   const char *const __restrict__ in,
                   const u64x2 max,
                   lambda&& closure)
noexcept
{
	using input_t_u = unaligned<input_t>;
	using block_t_u = unaligned<block_t>;
	using token_t = lane_type<block_t>;

	u64x2 count
	{
		0, // total token count (one for each lane populated in block_t's)
		0, // input pos (bytes)
	};

	const auto out
	{
		reinterpret_cast<token_t *>(out_)
	};

	// primary broadband loop
	while(count[1] + sizeof(input_t) <= max[1] && count[0] + lanes<block_t>() <= max[0])
	{
		static const auto mask
		{
			mask_full<input_t>()
		};

		const auto di
		{
			reinterpret_cast<block_t_u *>(out + count[0])
		};

		const auto si
		{
			reinterpret_cast<const input_t_u *>(in + count[1])
		};

		const input_t input
		(
			*si
		);

		block_t output;
		const auto consume
		{
			closure(output, input, mask)
		};

		*di = output;
		count += consume;
	}

	// trailing narrowband loop
	while(count[0] < max[0] && count[1] < max[1])
	{
		block_t output;
		input_t input {0}, mask {0};
		for(size_t i(0); count[1] + i < max[1] && i < sizeof(input_t); ++i)
		{
			input[i] = in[count[1] + i];
			mask[i] = 0xff;
		}

		const auto consume
		{
			closure(output, input, mask)
		};

		assert(consume[0] < sizeof(block_t));
		for(size_t i(0); i < consume[0] && count[0] + i < max[0]; ++i)
			out[count[0] + i] = output[i];

		count += consume;
	}

	const u64x2 ret
	{
		std::min(count[0], max[0]),
		std::min(count[1], max[1]),
	};

	return ret;
}
