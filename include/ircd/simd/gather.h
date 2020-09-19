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
#define HAVE_IRCD_SIMD_GATHER_H

namespace ircd::simd
{
	template<int scale = 1,
	         class value,
	         class index_vector,
	         class value_vector>
	value_vector gather(const value *, const index_vector, const u64, value_vector) noexcept;
}

/// Gather values from memory locations into the returned vector. This template
/// emits vpgather on skylake and later. On broadwell and haswell and earlier
/// this template will not emit any vpgather by default.
///
/// Each lane in the index vector corresponds to each lane in the return vector.
/// Each bit in the mask corresponds to each lane as well.
/// The default values for each lane are provided in the last argument.
///
template<int scale,
         class value,
         class index_vector,
         class value_vector>
inline value_vector
ircd::simd::gather(const value *const __restrict__ base,
                   const index_vector index,
                   const u64 mask,
                   value_vector ret)
noexcept
{
	static_assert
	(
		lanes<index_vector>() == lanes<value_vector>()
	);

	auto *const out
	{
		reinterpret_cast<lane_type<value_vector> *>(&ret)
	};

	const auto *const idx
	{
		reinterpret_cast<const lane_type<index_vector> *>(&index)
	};

	#if defined(__AVX2__)
	#pragma clang loop unroll(disable)
	#endif
	for(size_t i(0); i < lanes<index_vector>(); ++i)
		if(mask & (1UL << i))
			out[i] = base[idx[i] * scale];

	return ret;
}
