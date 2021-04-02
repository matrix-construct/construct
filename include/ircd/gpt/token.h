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
#ifdef __OPENCL_C_VERSION__
#define HAVE_IRCD_GPT_TOKEN_H

union ircd_gpt_token
{
	float
	word[768],
	attn[12][64];
};

union ircd_gpt_tokenv
{
	float4
	word[768/4],
	attn[12][64/4];
};

struct ircd_gpt_qkv
{
	union ircd_gpt_tokenv
	qry,
	key,
	val;
};

struct ircd_gpt_qkvv
{
	union ircd_gpt_tokenv
	qry,
	key,
	val;
};

struct ircd_gpt_attn_mask
{
	bool
	token[1024];
};

union ircd_gpt_aperature
{
	float
	word[768],
	fcon[2304],
	proj[3][768],
	qkv[3][12][64],
	attn[12][64];
};

union ircd_gpt_aperaturev
{
	float4
	word[768/4],
	fcon[2304/4],
	proj[3][768/4],
	qkv[3][12][64/4],
	attn[12][64/4];
};

#endif
