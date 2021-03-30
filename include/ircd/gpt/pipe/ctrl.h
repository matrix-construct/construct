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
#define HAVE_IRCD_GPT_PIPE_CTRL_H

struct ctor_ctrl
{
	long call;
	ulong pc;
	ulong tokens;
	ulong magic;
	uchar pad[1024 - 32];

	union
	{
		char str[3072];
		ushort token[1536];
	}
	body;
}
__attribute__((aligned(4096)));

struct ctor_opts
{
    uchar pad[4096];
}
__attribute__((aligned(4096)));

#ifndef __OPENCL_C_VERSION__
static_assert(sizeof(struct ctor_ctrl) == 4096);
#endif

#ifndef __OPENCL_C_VERSION__
static_assert(sizeof(struct ctor_opts) == 4096);
#endif

#ifndef __cplusplus

union token
{
	float
	word[768],
	attn[12][64];
};

union tokenv
{
	float4
	word[768/4],
	attn[12][64/4];
};

struct qkv
{
	union token
	qry,
	key,
	val;
};

struct qkvv
{
	union tokenv
	qry,
	key,
	val;
};

struct attn_mask
{
	bool
	token[1024];
};

union aperature
{
	float
	word[768],
	fcon[2304],
	proj[3][768],
	qkv[3][12][64],
	attn[12][64];
};

union aperaturev
{
	float4
	word[768/4],
	fcon[2304/4],
	proj[3][768/4],
	qkv[3][12][64/4],
	attn[12][64/4];
};

#endif
