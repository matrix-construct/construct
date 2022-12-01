// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_SIMT_CYCLES_H

#if defined(__OPENCL_VERSION__)
inline ulong
__attribute__((always_inline))
ircd_simt_cycles()
{
	// Compiles but doesn't link on SPIR
	#if __has_builtin(__builtin_readcyclecounter) && !defined(__SPIR)
		return __builtin_readcyclecounter();
	#else
		return 0;
	#endif
}
#endif

#if defined(__OPENCL_VERSION__)
/// Read the realtime-clock cycle counting timestamp `s_memrealtime` otherwise
/// falls back to the default cycle counter (which defaults to zero if also not
/// supported).
///
/// This is more consistent than `s_memtime` but slightly less granular; it is
/// probably provided by a control unit rather than directly in an SIMD/ALU.
inline ulong
__attribute__((always_inline))
ircd_simt_cycles_rtc()
{
	ulong ret;
	#if defined(__AMDGCN__)
		asm volatile
		(
			"s_memrealtime %0"
			: "=s" (ret)
		);
	#else
		ret = ircd_simt_cycles();
	#endif

	return ret;
}
#endif
