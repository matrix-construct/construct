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
#define HAVE_IRCD_SIMT_MEM_H

#if defined(__OPENCL_VERSION__)
__constant
struct ircd_simt_mem
{
	const uint
	local_bank_width,      ///< Bytes per bank
	local_banks;           ///< Banks per unit

	const uint
	global_bank_width,     ///< Bytes per bank
	global_chan_banks,     ///< Banks per channel
	global_chans;          ///< Channels per device
}
ircd_simt_mem = //TODO: XXX get values better
{
	.local_bank_width = 4,
	.local_banks = 32,

	.global_bank_width = 256,

	#if defined(__AMDDNA__)
		.global_chan_banks = 4,
	#else
		.global_chan_banks = 1,
	#endif

	#if defined(__AMDDNA__)
		.global_chans = 64,
	#elif defined(__AMDGCN__)
		.global_chans = 12,
	#elif defined(__R600__)
		.global_chans = 8,
	#else
		.global_chans = 1,
	#endif
};
#endif

#if defined(__OPENCL_VERSION__)
/// Given a global pointer, determines the memory channel and bank within that
/// channel returned as a ushort2: [0] = channel, [1] = bank.
static ushort2
ircd_simt_mem_chan_bank(__global const void *const ptr)
{
	const uint
	global_chan_width = ircd_simt_mem.global_chan_banks * ircd_simt_mem.global_bank_width,
	global_cache_width = ircd_simt_mem.global_chans * global_chan_width;

	const uintptr_t
	uptr = (uintptr_t)ptr,
	cb = uptr % global_cache_width;

	const ushort
	chan = cb / global_chan_width,
	coff = cb % global_chan_width,
	bank = coff / ircd_simt_mem.global_bank_width;

	return (ushort2)
	{
		chan, bank
	};
}
#endif
