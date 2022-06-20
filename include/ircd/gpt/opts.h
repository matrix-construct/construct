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

#if defined(__cplusplus)
namespace ircd::gpt::model
{
	struct decoder;
}
#endif

/// Task Options Page
///
/// The option block is directly shared with task software as constant data.
/// This stucture and its mutable companion in `task.h` determine the outcome
/// of the next execution cycle; options are immutable to device software but
/// may be changed by the host between executions cycles if desired.
///
struct ircd_gpt_opts
{
	#if defined(__cplusplus)
	ircd_gpt_opts() noexcept;
	#endif

	//
	// Frontside
	//

	/// Seed for the task's PRNG.
	ulong seed;

	/// Flip random coins over the top k logits each round. Setting to 1
	/// deterministically selects the top logit.
	uint top_k;

	/// Flip a random coin between 0 and top_p ( = 90 = 0.9) for logit select.
	float top_p;

	/// Registers the top n result logits in the ctrl block each cycle.
	uint top_n;

	/// Number of target labels to register results for in the ctrl block.
	uint labels;

	/// Number of pages available after the control block for the frame log.
	uint frames;

	/// Limit number of output tokens. Default of -1; other halting conditions
	/// will be used.
	uint limit;

	/// Bitbar toggling various debug modes.
	uint debug;

	/// Accepting condition codes.
	ushort accept[4][8] __attribute__((aligned(4)));

	//
	// Backside
	//

	/// Samples per step.
	uint batch_size;

	/// Training steps
	uint training_steps;

	/// Validation steps
	uint validation_steps;

	/// Testing steps
	uint testing_steps;

	/// Learning rate
	float alpha;

	/// Decay rate
	float beta[2];

	/// Denorm smoothing
	float epsilon;

	/// Tuning convergence rate
	float lambda;

	//
	// Model dimensions
	//

	/// Number of possible target n-grams.
	uint logits;

	/// Specifies the token buffer size in tokens.
	uint buffer_tokens;

	/// Specifies the token context size in tokens.
	uint context_tokens;

	/// Decoding layers.
	uint layers;

	/// SIMD lane count.
	uint lanes;

	/// Embedding vector elements
	uint embed_elems;

	/// (computed) `embed_elems` / `lanes`
	uint embed_width;

	/// Cross-attention dimension
	uint attn_rank;

	/// Attention unit fcon width multiple
	uint attn_mult;

	/// (computed) attention unit width multiple
	uint attn_elems;

	/// (computed) Attention unit X dimension
	uint attn_fcon_width;

	/// (computed) Attention unit Y dimension
	uint attn_fcon_height;

	/// (computed) Attention unit X dimension
	uint attn_proj_width;

	/// (computed) Attention unit Y dimension
	uint attn_proj_height;

	/// (computed) Packed attention array total element count
	uint attn_self_elems;

	/// MLP unit fcon width multiple
	uint ffnn_mult;

	/// (computed) FFNN unit width multiple
	uint ffnn_elems;

	/// (computed) MLP backend X dimension
	uint ffnn_fcon_width;

	/// (computed) MLP backend Y dimension
	uint ffnn_fcon_height;

	/// (computed) MLP backend X dimension
	uint ffnn_proj_width;

	/// (computed) MLP backend Y dimension
	uint ffnn_proj_height;
}
__attribute__((aligned(4096)));

#if defined(__cplusplus)
namespace ircd::gpt
{
	using opts = ::ircd_gpt_opts;
}
#endif

#if defined(__cplusplus)
static_assert(sizeof(struct ircd_gpt_opts) == 4096);
#endif

#if defined(__cplusplus) && defined(__GLIBCXX__)
static_assert(std::is_standard_layout<struct ircd_gpt_opts>::value);
#endif
