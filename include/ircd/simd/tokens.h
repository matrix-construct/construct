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
                   const u64x2 max_,
                   lambda&& closure)
noexcept
{
	char *const __restrict__ &out
	{
		reinterpret_cast<char *__restrict__>(out_)
	};

	const u64x2 max
	{
		max_[0] * sizeof_lane<block_t>(),
		max_[1]
	};

	assert(max[0] % sizeof(block_t) == 0);
	const u64x2 consumed
	{
		transform<input_t>(out, in, max, [&closure]
		(input_t &block, const auto mask)
		{
			const u64x2 res
			{
				closure(reinterpret_cast<block_t &>(block), block, mask)
			};

			// Closure returns the number of elems they filled of the output
			// block; we convert that to bytes for the transform template.
			assert(res[0] <= lanes<block_t>());
			assert(res[1] <= sizeof(block_t));
			return u64x2
			{
				res[0] * sizeof_lane<block_t>(),
				res[1],
			};
		})
	};

	assert(consumed[0] % sizeof_lane<block_t>() == 0);
	const u64x2 ret
	{
		consumed[0] / sizeof_lane<block_t>(),
		std::min(consumed[1], max[1]),
	};

	assert(ret[0] <= max_[0]);
	assert(ret[1] <= max_[1]);
	return ret;
}
