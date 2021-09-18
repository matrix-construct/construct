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
#define HAVE_IRCD_GPT_GATE_H

/// Task Gate Descriptor
///
struct ircd_gpt_gate
{
	ushort offset;
	ushort code[7];
}
__attribute__((aligned(16)));

#ifdef __cplusplus
struct ircd::gpt::gate
:ircd_gpt_gate
{
	gate()
	:ircd_gpt_gate{0}
	{}
};
#endif
