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
#define HAVE_IRCD_GPT_OPTS_H

/// Task Options Page
///
/// The option block is directly shared with task software as constant data.
/// This stucture and its mutable companion in `task.h` determine the outcome
/// of the next execution cycle; options are immutable to device software but
/// may be changed by the host between executions cycles if desired.
///
struct ircd_gpt_opts
{
	/// Specifies the nominal halting condition based on a sequence of tokens.
	/// Generation will complete with success after one of these sequences is
	/// witnessed. Set tokens to -1 starting from the back for shorter
	/// sequences; zero-length sequences (all -1's) are never matched.
	uint accept_code[4][4]
	#ifdef __cplusplus
	{
		{    13,  198,  -1U,  -1U,  },
		{   198,  198,  -1U,  -1U,  },
		{   -1U,  -1U,  -1U,  -1U,  },
		{   -1U,  -1U,  -1U,  -1U,  },
	}
	#endif
	;

	/// Specifies the exceptional halting condition based on the sequence of
	/// tokens. By default, the three zeros represent three outputs of '!'
	/// which is probably an error; note that a true "!!!" is represented by
	/// token number 10185. Set tokens to -1 starting from the back to not
	/// match that token; generated output after errors is usually garbage.
	uint error_code[4][4]
	#ifdef __cplusplus
	{
		{     0,    0,    0,  -1U,  },
		{   -1U,  -1U,  -1U,  -1U,  },
		{   -1U,  -1U,  -1U,  -1U,  },
		{   -1U,  -1U,  -1U,  -1U,  },
	}
	#endif
	;

	/// Limit number of output tokens. Default of -1 is unlimited; the number
	/// of tokens generated will be limited by other factors.
	uint limit
	#ifdef __cplusplus
	{
		1
	}
	#endif
	;

	/// Flip random coins over the top k logits each round. Setting to 1
	/// deterministically selects the top logit.
	uint top_k
	#ifdef __cplusplus
	{
		2
	}
	#endif
	;

	/// Specifies the token context size in tokens.
	uint context_tokens
	#ifdef __cplusplus
	{
		1024
	}
	#endif
	;

	/// Specifies the token buffer size in tokens.
	uint buffer_tokens
	#ifdef __cplusplus
	{
		1024
	}
	#endif
	;

	/// Embedding vector elements
	uint embed_elems
	#ifdef __cplusplus
	{
		768
	}
	#endif
	;

	/// Attention unit fcon width multiple
	uint attn_mult
	#ifdef __cplusplus
	{
		3U
	}
	#endif
	;

	/// MLP unit fcon width multiple
	uint ffnn_mult
	#ifdef __cplusplus
	{
		4U
	}
	#endif
	;

	/// Attention unit width multiple
	uint attn_elems
	#ifdef __cplusplus
	{
		embed_elems * attn_mult
	}
	#endif
	;

	/// FFNN unit width multiple
	uint ffnn_elems
	#ifdef __cplusplus
	{
		embed_elems * ffnn_mult
	}
	#endif
	;

	/// SIMD lane count
	uint lanes
	#ifdef __cplusplus
	{
		4U
	}
	#endif
	;

	uint embed_width
	#ifdef __cplusplus
	{
		embed_elems / lanes
	}
	#endif
	;

	uint attn_width
	#ifdef __cplusplus
	{
		attn_elems / lanes
	}
	#endif
	;

	uint attn_height
	#ifdef __cplusplus
	{
		embed_elems / lanes
	}
	#endif
	;

	uint ffnn_width
	#ifdef __cplusplus
	{
		ffnn_elems / lanes
	}
	#endif
	;

	uint ffnn_height
	#ifdef __cplusplus
	{
		embed_elems / lanes
	}
	#endif
	;

	/// Specifies the token context size in tokens.
	uint logits
	#ifdef __cplusplus
	{
		50257
	}
	#endif
	;

	/// Seed for the task's PRNG.
	ulong seed
	#ifdef __cplusplus
	{
		1234567890UL
	}
	#endif
	;

	/// Training steps
	ulong training_steps
	#ifdef __cplusplus
	{
		250000
	}
	#endif
	;

	/// Validation steps
	ulong validation_steps
	#ifdef __cplusplus
	{
		5000
	}
	#endif
	;

	ushort label
	#ifdef __cplusplus
	{
		198
	}
	#endif
	;

	float alpha
	#ifdef __cplusplus
	{
		0.001
	}
	#endif
	;

	float beta[2]
	#ifdef __cplusplus
	{
		0.9,    // Beta1
		0.999,  // Beta2
	}
	#endif
	;

	float epsilon
	#ifdef __cplusplus
	{
		0.000001
	}
	#endif
	;
}
__attribute__((aligned(4096)));

#ifdef __cplusplus
/// Generator Task Options.
///
/// Parameters for a task. Options are constant and one instance can be shared
/// between multiple task instances. This structure extends the task options
/// page, starting a new page which is not visible to device software; C++ and
/// host pointers are available.
///
struct ircd::gpt::opts
:ircd_gpt_opts
{
	/// Pointer to the model
	const model::decoder *model
	{
		model::default_model
	};
};

static_assert(sizeof(struct ircd_gpt_opts) == 4096);
static_assert(std::is_standard_layout<struct ircd_gpt_opts>::value);
#endif
