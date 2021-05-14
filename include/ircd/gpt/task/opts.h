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
	#ifdef __cplusplus
	ircd_gpt_opts(const ircd::gpt::model::decoder * = nullptr) noexcept;
	#endif

	/// Reference to the model (currently not available in device software).
	#ifndef __cplusplus
	const void *model;
	#else
	const ircd::gpt::model::decoder *model;
	#endif

	/// Seed for the task's PRNG.
	ulong seed;

	/// Limit number of output tokens. Default of -1 is unlimited; the number
	/// of tokens generated will be limited by other factors.
	uint limit;

	/// Flip random coins over the top k logits each round. Setting to 1
	/// deterministically selects the top logit.
	uint top_k;

	/// Specifies the token context size in tokens.
	uint context_tokens;

	/// Specifies the token buffer size in tokens.
	uint buffer_tokens;

	/// Embedding vector elements
	uint embed_elems;

	/// Attention unit fcon width multiple
	uint attn_mult;

	/// (computed) MLP unit fcon width multiple
	uint ffnn_mult;

	/// (computed) attention unit width multiple
	uint attn_elems;

	/// FFNN unit width multiple
	uint ffnn_elems;

	/// SIMD lane count
	uint lanes;

	/// (computed) `embed_elems` / `lanes`
	uint embed_width;

	/// (computed) Attention unit X dimension
	uint attn_width;

	/// (computed) Attention unit Y dimension
	uint attn_height;

	/// (computed) MLP backend X dimension
	uint ffnn_width;

	/// (computed) MLP backend Y dimension
	uint ffnn_height;

	/// Number of possible target n-grams.
	uint logits;

	/// Training steps
	uint training_steps;

	/// Validation steps
	uint validation_steps;

	/// Testing steps
	uint testing_steps;

	/// Target label
	ushort label;

	/// Bitbar toggling various debug modes
	ushort debug;

	/// Learning rate
	float alpha;

	/// Decay rate
	float beta[2];

	/// Denorm smoothing
	float epsilon;

	/// Number of gate descriptors attached to this page.
	uint gates;

	/// The gate descriptor table starts at offset 2048 and continues to the
	/// end of the page. For more descriptors additional pages must be
	/// attached.
	struct ircd_gpt_gate gate[] __attribute__((aligned(2048)));
}
__attribute__((aligned(4096)));

#ifdef __cplusplus
namespace ircd::gpt
{
	using opts = ::ircd_gpt_opts;
}

static_assert(sizeof(struct ircd_gpt_opts) == 4096);
static_assert(std::is_standard_layout<struct ircd_gpt_opts>::value);
#endif
