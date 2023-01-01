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
#define HAVE_IRCD_SIMT_HWID2_H

/// HW_ID2 Register
struct ircd_simt_hwid2
{
	uint
	queue,   ///< Queue ID (shader stage).
	pipl,    ///< Pipeline ID.
	me,      ///< Micro-engine ID: 0 = graphics, 1 & 2 = ACE compute
	state,   ///< State context ID.
	wg,      ///< Work-group ID within the WGP.
	vm;      ///< Virtual Memory ID.
};

/// Query hardware identification attributes
inline struct ircd_simt_hwid2
__attribute__((always_inline))
ircd_simt_hwid2()
{
	uint val = 0;
	#if defined(__AMDDNA__)
	asm volatile
	(
		"s_getreg_b32 %0, hwreg(HW_REG_HW_ID2, 0, 32)"
		: "=s" (val)
	);
	#endif

	return (struct ircd_simt_hwid2)
	{
		.queue  = (val >> 0)  & 0x0f,
		.pipl   = (val >> 4)  & 0x03,
		.me     = (val >> 8)  & 0x03,
		.state  = (val >> 12) & 0x07,
		.wg     = (val >> 16) & 0x1f,
		.vm     = (val >> 24) & 0x0f,
	};
}
