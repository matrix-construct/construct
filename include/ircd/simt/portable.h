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
#define HAVE_IRCD_SIMT_PORTABLE_H

//
// Decide the proper form of the restrict qualifier for the platform, if any.
//

#if defined(__OPENCL_VERSION__)
	#if defined(__SPIR)
		#define restrict
	#elif defined(__cplusplus)
		#define restrict __restrict
	#endif
#endif

//
// Decide the proper form of static linkage for the platform
//

#if defined(__OPENCL_VERSION__)
	#if __OPENCL_VERSION__ < 120
		#define static __attribute__((internal_linkage))
	#endif
#endif

//
// When headers are shared between opencl and non-opencl units, address space
// qualifiers may be required for the former and can be ignored for the latter.
// Note this header itself may be included by both opencl and non-opencl units.
//

#if !defined(__OPENCL_VERSION__)
	#define __constant static
#endif

//
// OpenCL wants __asm__ but also allow asm
//

#if defined(__OPENCL_VERSION__)
	#if !defined(asm)
		#define asm __asm__
	#endif
#endif

//
// Silently drop calls to printf() on unsupporting platforms; this way we can
// leave the same code unmodified and quietly discard the output.
//

#if defined(__OPENCL_VERSION__)
	#if __OPENCL_VERSION__ < 200
		#define printf(...)
	#endif
#endif

//
// XXX
//

#if defined(__OPENCL_VERSION__)
	#if !defined(__SIZEOF_FLOAT4__)
		#define __SIZEOF_FLOAT4__ 16
	#endif
#endif

#if defined(__OPENCL_VERSION__)
	#if !defined(__SIZEOF_FLOAT8__)
		#define __SIZEOF_FLOAT8__ 32
	#endif
#endif

#if defined(__OPENCL_VERSION__)
	#if !defined(__SIZEOF_FLOAT16__)
		#define __SIZEOF_FLOAT16__ 64
	#endif
#endif

//
// Differentiate RDNA over GCN
//

#if defined(__AMDGCN__) && !defined(__AMDDNA__)
	#if __AMDGCN_WAVEFRONT_SIZE == 32
		#define __AMDDNA__ 1
	#endif
#endif
