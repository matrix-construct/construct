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
#define HAVE_IRCD_SIMT_HWID1_H

/// HW_ID1 Register
struct ircd_simt_hwid1
{
	uint
	wave,    ///< Wave ID within SIMD.
	simd,    ///< SIMD within WGP: [0] = row, [1] = col
	wgd,     ///< Workgroup Processor ID (distance from SPI)
	wgt,     ///< Workgroup Processor ID (tier above/below SPI)
	sa,      ///< Shader Array within SE.
	se,      ///< Shader Engine ID.
	dpr;     ///< Double-precision float units per SIMD.
};

/// Query hardware identification attributes
inline struct ircd_simt_hwid1
__attribute__((always_inline))
ircd_simt_hwid1()
{
	uint val = 0;
	#if defined(__AMDDNA__)
	asm volatile
	(
		"s_getreg_b32 %0, hwreg(HW_REG_HW_ID1, 0, 32)"
		: "=s" (val)
	);
	#endif

	return (struct ircd_simt_hwid1)
	{
		.wave   = (val >> 0)  & 0x1f,
		.simd   = (val >> 8)  & 0x03,
		.wgd    = (val >> 10) & 0x07,
		.wgt    = (val >> 13) & 0x01,
		.sa     = (val >> 16) & 0x01,
		.se     = (val >> 18) & 0x07,
		.dpr    = (val >> 29) & 0x07,
	};
}
