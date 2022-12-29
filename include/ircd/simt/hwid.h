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
#define HAVE_IRCD_SIMT_HWID_H

/// HW_ID Register (legacy)
struct ircd_simt_hwid
{
	uint
	wave,    ///< Wave buffer slot number.
	simd,    ///< SIMD assigned to within unit.
	pipl,    ///< Pipeline from which wave dispatched.
	cu,      ///< Compute Unit ID.
	sa,      ///< Shader Array within engine.
	se,      ///< Shader Engine ID.
	tg,      ///< Thread-group ID.
	vm,      ///< Virtual Memory ID.
	queue,   ///< Queue from which wave was dispatched.
	state,   ///< State ID (graphics only, not compute).
	me;      ///< Micro-engine ID.
};

/// Query hardware identification attributes (legacy)
inline struct ircd_simt_hwid
__attribute__((always_inline))
ircd_simt_hwid()
{
	uint val = 0;
	#if defined(__AMDGCN__)
	asm volatile
	(
		"s_getreg_b32 %0, hwreg(HW_REG_HW_ID, 0, 32)"
		: "=s" (val)
	);
	#endif

	return (struct ircd_simt_hwid)
	{
		.wave   = (val >> 0)  & 0xf,
		.simd   = (val >> 4)  & 0x3,
		.pipl   = (val >> 6)  & 0x3,
		.cu     = (val >> 8)  & 0xf,
		.sa     = (val >> 12) & 0x1,
		.se     = (val >> 13) & 0x3,
		.tg     = (val >> 16) & 0xf,
		.vm     = (val >> 20) & 0xf,
		.queue  = (val >> 24) & 0x7,
		.state  = (val >> 27) & 0x7,
		.me     = (val >> 30) & 0x3,
	};
}
