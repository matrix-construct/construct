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
#define HAVE_IRCD_SIMD_STREAM_H

namespace ircd::simd
{
	// Using the AVX512 vector type by default as it conveniently matches the
	// cache-line size on the relevant platforms and simplifies our syntax below
	// by being a single object. On those w/o AVX512 it uses an isomorphic
	// configuration of the best available regs.
	using stream_line_t = u512x1;

	template<class block_t>
	using stream_proto = void (block_t &);

	// Platforms that have non-temporal store support; this is all of x86_64
	constexpr bool stream_has_store
	{
		#if defined(__SSE2__) && !defined(RB_GENERIC)
			true
		#else
			false
		#endif
	};

	// Platforms that have non-temporal load support; sidenote SSE4.1 can do
	// 16 byte loads and AVX2 can do 32 byte loads; SSE2 cannot do loads.
	constexpr bool stream_has_load
	{
		#if defined(__AVX__) && !defined(RB_GENERIC)
			true
		#else
			false
		#endif
	};

	// The number of cache lines we'll have "in flight" which is basically
	// just a gimmick to unroll the loop such that each iteration covers
	// the full register file. On SSE with 256 bytes of register file we can
	// name 4 cache lines at once; on AVX with 512 bytes we can name 8, etc.
	constexpr size_t stream_max_lines
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
	constexpr size_t stream_latency
	{
		16
	};

	template<size_t = stream_max_lines,
	         class lambda>
	mutable_buffer
	stream(const mutable_buffer &, const const_buffer &, lambda&&) noexcept;
}

/// Non-temporal stream. This copies from an aligned source to an aligned
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
template<size_t bandwidth,
         class lambda>
inline ircd::mutable_buffer
ircd::simd::stream(const mutable_buffer &dst,
                   const const_buffer &src,
                   lambda&& closure)
noexcept
{
	using line_t = stream_line_t;

	constexpr auto file_lines
	{
		std::min(bandwidth, stream_max_lines)
	};

	// Assert valid arguments
	assert(!overlap(src, dst));
	assert(aligned(data(src), sizeof(line_t)));
	assert(aligned(data(dst), sizeof(line_t)));
	assert(size(dst) % (sizeof(line_t) * file_lines));

	// Size in bytes to be copied
	const size_t copy_size
	{
		std::min(size(src), size(dst))
	};

	// Number of lines to be copied.
	const size_t copy_lines
	{
		(copy_size / sizeof(line_t)) + bool(copy_size % sizeof(line_t))
	};

	// destination base ptr
	line_t *const __restrict__ out
	{
		reinterpret_cast<line_t *__restrict__>(data(dst))
	};

	// source base ptr
	const line_t *const __restrict__ in
	{
		reinterpret_cast<const line_t *__restrict__>(data(src))
	};

	if constexpr(!stream_has_load)
		#pragma clang loop unroll(disable)
		for(size_t i(0); i < stream_latency; ++i)
			__builtin_prefetch(in + i, 0, 0);

	for(size_t i(0); i < copy_lines; i += file_lines)
	{
		if constexpr(!stream_has_load)
			for(size_t j(0); j < file_lines; ++j)
				__builtin_prefetch(in + i + stream_latency + j, 0, 0);

		line_t line[file_lines];
		for(size_t j(0); j < file_lines; ++j)
			#if defined(__clang__)
				line[j] = __builtin_nontemporal_load(in + i + j);
			#else
				line[j] = in[i + j];
			#endif

		for(size_t j(0); j < file_lines; ++j)
			closure(line[j]);

		for(size_t j(0); j < file_lines; ++j)
			#if defined(__clang__)
				__builtin_nontemporal_store(line[j], out + i + j);
			#else
				*(out + i + j) = line[j]; //TODO: XXX
			#endif
	}

	if constexpr(stream_has_store)
		asm volatile ("sfence");

	return mutable_buffer
	{
		data(dst), copy_size
	};
}
