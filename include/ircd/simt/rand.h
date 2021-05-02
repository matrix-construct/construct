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
#define HAVE_IRCD_SIMT_RAND_H

#ifdef __OPENCL_C_VERSION__
/// Generate the next pseudo-random 64-bit sequence from the 256-bit state
/// and update the state for the next call.
inline ulong
ircd_simt_rand_xoshiro256p(ulong s[4])
{
	const ulong
	ret = s[0] + s[3],
	ent = s[1] << 17;

	s[2] ^= s[0];
	s[3] ^= s[1];
	s[1] ^= s[3];
	s[0] ^= s[3];
	s[2] ^= ent;

	s[3] = rotate(s[3], 45UL); // (s[3] << 45) | (s[3] >> (64 - 45));

	return ret;
}
#endif

#ifdef __OPENCL_C_VERSION__
/// Generate the next pseudo-random 64-bit sequence from the 256-bit global
/// state and update the state for the next call.
inline ulong
ircd_simt_rand_xoshiro256pg(__global ulong s[4])
{
	ulong _s[4], ret;
	for(uint i = 0; i < 4; i++)
		_s[i] = s[i];

	ret = ircd_simt_rand_xoshiro256p(_s);
	for(uint i = 0; i < 4; i++)
		s[i] = _s[i];

	return ret;
}
#endif
