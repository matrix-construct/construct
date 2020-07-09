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
#define HAVE_IRCD_BUFFER_STREAM_H

namespace ircd::buffer
{
	size_t stream_aligned(const mutable_buffer &dst, const const_buffer &src);
}

/// Non-temporal copy. This copies from an aligned source to an aligned
/// destination without the data cycling through the d-cache. The alignment
/// requirements are currently very strict. The source and destination buffers
/// must begin at a cache-line alignment and the size of the buffers must be
/// a multiple of something we'll call "register-file size" which is the size
/// of all named multimedia registers (256 for SSE, 512 for AVX, 2048 for
/// AVX512) so it's safe to say buffers should just be aligned and padded out
/// to 4K page-size to be safe. The size of the src argument itself can be an
/// arbitrary size and this function will return that size, but its backing
/// buffer must be padded out to alignment.
///
inline size_t
ircd::buffer::stream_aligned(const mutable_buffer &dst,
                             const const_buffer &src)
{
	// Platforms that have non-temporal store support; this is all of x86_64
	constexpr bool has_store
	{
		#if defined(__SSE2__) && !defined(RB_GENERIC)
			true
		#else
			false
		#endif
	};

	// Platforms that have non-temporal load support; sidenote SSE4.1 can do
	// 16 byte loads and AVX2 can do 32 byte loads; SSE2 cannot do loads.
	constexpr bool has_load
	{
		#if defined(__AVX__) && !defined(RB_GENERIC)
			true
		#else
			false
		#endif
	};

	// Use the AVX512 vector type unconditionally as it conveniently matches
	// the cache-line size on the relevant platforms and simplifies our syntax
	// below by being a single object. On those w/o AVX512 it uses an
	// isomorphic configuration of the best available regs.
	using block_type = u512x1;

	// The number of cache lines we'll have "in flight" which is basically
	// just a gimmick to unroll the loop such that each iteration covers
	// the full register file. On SSE with 256 bytes of register file we can
	// name 4 cache lines at once; on AVX with 512 bytes we can name 8, etc.
	constexpr size_t file_lines
	{
		#if defined(__AVX512F__)
			32
		#elif defined(__AVX__)
			8
		#else
			4
		#endif
	};

	// Configurable magic number only relevant to SSE2 systems which don't have
	// non-temporal load instructions. On these platforms we'll conduct a
	// prefetch loop and mark the lines NTA.
	constexpr size_t latency
	{
		16
	};

	// When the constexpr conditions aren't favorable we can fallback to
	// normal copy here.
	if constexpr(!has_store && !has_load)
		return copy(dst, src);

	// Assert valid arguments
	assert(!overlap(src, dst));
	assert(aligned(data(src), sizeof(block_type)));
	assert(aligned(data(dst), sizeof(block_type)));
	assert(size(dst) % (sizeof(block_type) * file_lines));

	// Size in bytes to be copied
	const size_t copy_size
	{
		std::min(size(src), size(dst))
	};

	// Number of lines to be copied.
	const size_t copy_lines
	{
		(copy_size / sizeof(block_type)) + bool(copy_size % sizeof(block_type))
	};

	// destination base ptr
	block_type *const __restrict__ out
	{
		reinterpret_cast<block_type *__restrict__>(data(dst))
	};

	// source base ptr
	const block_type *const __restrict__ in
	{
		reinterpret_cast<const block_type *__restrict__>(data(src))
	};

	if constexpr(!has_load)
		#pragma clang loop unroll(disable)
		for(size_t i(0); i < latency; ++i)
			__builtin_prefetch(in + i, 0, 0);

	for(size_t i(0); i < copy_lines; i += file_lines)
	{
		if constexpr(!has_load)
			for(size_t j(0); j < file_lines; ++j)
				__builtin_prefetch(in + i + latency + j, 0, 0);

		block_type block[file_lines];
		for(size_t j(0); j < file_lines; ++j)
			block[j] = __builtin_nontemporal_load(in + i + j);

		for(size_t j(0); j < file_lines; ++j)
			__builtin_nontemporal_store(block[j], out + i + j);
	}

	if constexpr(has_store)
		asm volatile ("sfence");

	return copy_size;
}
