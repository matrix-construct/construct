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
#define HAVE_IRCD_SIMD_ACCUMULATE_H

namespace ircd::simd
{
	/// Transform block_t by pseudo-reference. The closure has an opportunity
	/// to modify the block while it is being streamed from the source to the
	/// destination. The mask indicates which elements of the block are valid
	/// if the input is smaller than the block size. This function returns
	/// a pair of integers which advance the output and input positions of the
	/// streams for the next iteration.
	template<class block_t>
	using accumulate_prototype = void (block_t &, block_t, block_t mask);

	template<class block_t,
	         class input_t,
	         class lambda>
	block_t accumulate(const input_t *, const u64x2, block_t, lambda&&) noexcept;
}

/// Streaming accumulation
///
template<class block_t,
         class input_t,
         class lambda>
inline block_t
ircd::simd::accumulate(const input_t *const __restrict__ in,
                       const u64x2 max,
                       block_t val,
                       lambda&& closure)
noexcept
{
	const u64x2 res
	{
		stream<block_t>(in, max, [&val, &closure]
		(const auto block, const auto mask)
		{
			closure(val, block, mask);
		})
	};

	return val;
}
