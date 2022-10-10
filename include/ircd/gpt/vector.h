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
#define HAVE_IRCD_GPT_VECTOR_H

static __constant const uint
ircd_gpt_context_tokens = 512, // 1024,
ircd_gpt_vector_elems = 768,
ircd_gpt_attn_rank = 12,
ircd_gpt_attn_segs = 3,
ircd_gpt_ffnn_segs = 4;

static __constant const uint
ircd_gpt_vector_attn_elems = ircd_gpt_vector_elems / ircd_gpt_attn_rank,
ircd_gpt_attn_fcon_elems = ircd_gpt_vector_elems * ircd_gpt_attn_segs,
ircd_gpt_ffnn_fcon_elems = ircd_gpt_vector_elems * ircd_gpt_ffnn_segs;

//
// embed vector
//

#if defined(__SIZEOF_FLOAT__)
union ircd_gpt_vector
{
	float
	elem[ircd_gpt_vector_elems],
	attn[ircd_gpt_attn_rank][ircd_gpt_vector_attn_elems];
};
#endif

#if defined(__SIZEOF_FLOAT4__)
union ircd_gpt_vector_f32x4
{
	float4
	elem[ircd_gpt_vector_elems / 4],
	attn[ircd_gpt_attn_rank][ircd_gpt_vector_attn_elems / 4];

	union ircd_gpt_vector
	vector;
};
#endif

//
// attn qkv
//

#if defined(__SIZEOF_FLOAT__)
struct ircd_gpt_attn_qkv
{
	union ircd_gpt_vector
	qry,
	key,
	val;
};
#endif

#if defined(__SIZEOF_FLOAT4__)
struct ircd_gpt_attn_qkv_f32x4
{
	union ircd_gpt_vector_f32x4
	qry,
	key,
	val;
};
#endif

//
// attn aperature
//

#if defined(__SIZEOF_FLOAT__)
union ircd_gpt_attn_aperature
{
	float
	fcon[ircd_gpt_attn_fcon_elems],
	proj[ircd_gpt_attn_segs][ircd_gpt_vector_elems],
	qkv[ircd_gpt_attn_segs][ircd_gpt_attn_rank][ircd_gpt_vector_attn_elems];

	union ircd_gpt_vector
	vector[ircd_gpt_attn_segs];
};
#endif

#if defined(__SIZEOF_FLOAT4__)
union ircd_gpt_attn_aperature_f32x4
{
	float4
	fcon[ircd_gpt_attn_fcon_elems / 4],
	proj[ircd_gpt_attn_segs][ircd_gpt_vector_elems / 4],
	qkv[ircd_gpt_attn_segs][ircd_gpt_attn_rank][ircd_gpt_vector_attn_elems / 4];

	union ircd_gpt_vector_f32x4
	vector[ircd_gpt_attn_segs];
};
#endif

//
// ffnn aperature
//

#if defined(__SIZEOF_FLOAT__)
union ircd_gpt_ffnn_aperature
{
	float
	fcon[ircd_gpt_ffnn_fcon_elems],
	proj[ircd_gpt_ffnn_segs][ircd_gpt_vector_elems];

	union ircd_gpt_vector
	vector[ircd_gpt_ffnn_segs];
};
#endif

#if defined(__SIZEOF_FLOAT4__)
union ircd_gpt_ffnn_aperature_f32x4
{
	float4
	fcon[ircd_gpt_ffnn_fcon_elems / 4],
	proj[ircd_gpt_ffnn_segs][ircd_gpt_vector_elems / 4];

	union ircd_gpt_vector_f32x4
	vector[ircd_gpt_ffnn_segs];
};
#endif
