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
#define HAVE_IRCD_SIMD_SUPPORT_H

//
// These values allow for `if constexpr` to be used for generating feature-
// specific code. This is preferred to using preprocessor statements.
//

namespace ircd::simd::support
{
	constexpr bool sse
	{
		#if defined(__SSE__)
			true
		#else
			false
		#endif
	};

	constexpr bool sse2
	{
		#if defined(__SSE2__)
			true
		#else
			false
		#endif
	};

	constexpr bool sse3
	{
		#if defined(__SSE3__)
			true
		#else
			false
		#endif
	};

	constexpr bool ssse3
	{
		#if defined(__SSSE3__)
			true
		#else
			false
		#endif
	};

	constexpr bool sse4a
	{
		#if defined(__SSE4A__)
			true
		#else
			false
		#endif
	};

	constexpr bool sse4_1
	{
		#if defined(__SSE4_1__)
			true
		#else
			false
		#endif
	};

	constexpr bool sse4_2
	{
		#if defined(__SSE4_2__)
			true
		#else
			false
		#endif
	};

	constexpr bool avx
	{
		#if defined(__AVX__)
			true
		#else
			false
		#endif
	};

	constexpr bool avx2
	{
		#if defined(__AVX2__)
			true
		#else
			false
		#endif
	};

	constexpr bool avx512f
	{
		#if defined(__AVX512F__)
			true
		#else
			false
		#endif
	};

	constexpr bool avx512vbmi
	{
		#if defined(__AVX512VBMI__)
			true
		#else
			false
		#endif
	};

	constexpr bool avx512vbmi2
	{
		#if defined(__AVX512VBMI2__)
			true
		#else
			false
		#endif
	};
}
