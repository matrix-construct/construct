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
#define HAVE_IRCD_GPT_TOKEN_H

struct ircd_gpt_attn_mask
{
	bool
	token[1024];
};

union ircd_gpt_token
{
	float
	word[768],
	attn[12][64];
};

#ifdef __OPENCL_C_VERSION__
union ircd_gpt_tokenv
{
	float4
	word[768/4],
	attn[12][64/4];

	union ircd_gpt_token
	token;
};
#endif

struct ircd_gpt_attn_qkv
{
	union ircd_gpt_token
	qry,
	key,
	val;
};

#ifdef __OPENCL_C_VERSION__
struct ircd_gpt_attn_qkvv
{
	union ircd_gpt_tokenv
	qry,
	key,
	val;
};
#endif

union ircd_gpt_attn_aperature
{
	float
	word[768],
	fcon[2304],
	proj[3][768],
	qkv[3][12][64],
	attn[12][64];

	union ircd_gpt_token
	token[3];
};

#ifdef __OPENCL_C_VERSION__
union ircd_gpt_attn_aperaturev
{
	float4
	word[768/4],
	fcon[2304/4],
	proj[3][768/4],
	qkv[3][12][64/4],
	attn[12][64/4];

	union ircd_gpt_tokenv
	token[3];
};
#endif

union ircd_gpt_ffnn_aperature
{
	float
	word[768],
	fcon[3072],
	proj[4][768];

	union ircd_gpt_token
	token[4];
};

#ifdef __OPENCL_C_VERSION__
union ircd_gpt_ffnn_aperaturev
{
	float4
	word[768/4],
	fcon[3072/4],
	proj[4][768/4];

	union ircd_gpt_tokenv
	token[4];
};
#endif
