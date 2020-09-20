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
#define HAVE_IRCD_SIMD_SCATTER_H

namespace ircd::simd
{
	template<int scale = 1,
	         class value,
	         class index_vector,
	         class value_vector>
	void scatter(value *, const index_vector, const u64, const value_vector) noexcept;
}

/// Scatter values to memory locations from the argument vector.
///
/// Each lane in the index vector corresponds to each lane in the operand vector.
/// Each bit in the mask corresponds to each lane as well.
///
template<int scale,
         class value,
         class index_vector,
         class value_vector>
inline void
ircd::simd::scatter(value *const __restrict__ base,
                    const index_vector index,
                    const u64 mask,
                    const value_vector val)
noexcept
{
	static_assert
	(
		lanes<index_vector>() == lanes<value_vector>()
	);

	const auto *const in
	{
		reinterpret_cast<const lane_type<value_vector> *>(&val)
	};

	const auto *const idx
	{
		reinterpret_cast<const lane_type<index_vector> *>(&index)
	};

	#if defined(__AVX512F__)
	#pragma clang loop unroll(disable)
	#endif
	for(size_t i(0); i < lanes<index_vector>(); ++i)
		if(mask & (1UL << i))
			base[idx[i] * scale] = in[i];
}
