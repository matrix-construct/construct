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
#define HAVE_IRCD_SIMT_HADD_H

#if defined(__OPENCL_VERSION__) && defined(__SIZEOF_FLOAT4__)
inline float
__attribute__((always_inline))
ircd_simt_hadd_f4(const float4 in)
{
	float ret = 0.0f;
	for(uint i = 0; i < 4; ++i)
		ret += in[i];

	return ret;
}
#endif

#if defined(__OPENCL_VERSION__) && defined(__SIZEOF_FLOAT8__)
inline float
__attribute__((always_inline))
ircd_simt_hadd_f8(const float8 in)
{
	float ret = 0.0f;
	for(uint i = 0; i < 8; ++i)
		ret += in[i];

	return ret;
}
#endif

#if defined(__OPENCL_VERSION__) && defined(__SIZEOF_FLOAT16__)
inline float
__attribute__((always_inline))
ircd_simt_hadd_f16(const float16 in)
{
	float ret = 0.0f;
	for(uint i = 0; i < 16; ++i)
		ret += in[i];

	return ret;
}
#endif
