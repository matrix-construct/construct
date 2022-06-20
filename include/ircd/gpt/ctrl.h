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
#define HAVE_IRCD_GPT_CTRL_H

/// Result logit control block.
struct ircd_gpt_ctrl_logit
{
	/// Vocabulary token.
	ushort token;

	/// Padding #0.
	ushort flag;

	/// Result logit softmax probability.
	float samax;
};

/// Target label control block. Results for each target are registered
/// and state is updated each cycle.
struct ircd_gpt_ctrl_label
{
	/// Logit descriptor
	struct ircd_gpt_ctrl_logit logit;

	/// Loss state
	struct ircd_math_mean loss;

	/// Perplexity state
	struct ircd_math_mean ppl;
};

/// Master clock
struct ircd_gpt_ctrl_clk
{
	/// Master clock. The cycle count is incremented by one in device software
	/// after each repetition of the kernels producing one additional token.
	/// The cycle count resets to zero before the beginning of each sample.
	uint cycle;

	/// Master clock. Sample consists of one or more cycles; sample count is
	/// incremented by one in device software after every accept condition,
	/// growing monotonically for the `step`; resets to zero each `step`.
	uint samp;

	/// Master clock. Step (or timestep) consists of one or more samples. Step
	/// count is incremented by one in device software after each backward
	/// propagation. Step grows monotonically even across epochs.
	uint step;

	/// Master clock. Epoch consists of one or more steps; epoch count is
	/// incremented by one after every backward propagation.
	uint epoch;
};

/// Profiling block
struct ircd_gpt_ctrl_prof
{
	/// Host timestamp sampled at last control page transfer to the device.
	ulong released;

	/// Host timestamp sampled when this control page accquired by the host.
	ulong acquired;

	/// Device timestamp at beginning of cycle.
	ulong entered;

	/// Device timestamp at end of cycle.
	ulong finished;
};

/// Task Control Page
///
/// The control block is shared with our device software. Execution state is
/// maintained in the task control block across cycles. The control block is
/// the mutable state component for an execution; for the immutable component
/// also shared with device software see opts.h.
///
struct ircd_gpt_ctrl
{
	/// Accept register. If >= 0 the cycle produced a token which satisfies the
	/// indicated accept condition.
	int accept;

	/// Dispatch register. Device software wishes additional cycles to be
	/// commanded by the host. Effectively minimum distance until next accept.
	uint dispatch;

	/// Token counter. The counter indicates the number of valid tokens in
	/// the context buffer. This value must not exceed the opts.buffer_size.
	/// This value should not exceed the opts.context_size at least for now.
	uint count;

	/// Token counter. The counter indicates the number of valid tokens in
	/// the context buffer. This value must not exceed the opts.buffer_size.
	/// This value should not exceed the opts.context_size at least for now.
	uint tokens;

	/// Master clock.
	struct ircd_gpt_ctrl_clk clk;

	/// Profiling related.
	struct ircd_gpt_ctrl_prof prof;

	/// PRNG xoshiro256 internal state (note: see opts.h to seed the prng).
	ulong rand[4];

	/// Top result summary from the softed result logit softmax vector. This
	/// is updated each cycle by device software with extended statistics on
	/// the top N results.
	struct ircd_gpt_ctrl_logit top[16] __attribute__((aligned(8)));

	/// User label control block. Results for each target are registered
	/// and state is updated each cycle; averaged for each step.
	struct ircd_gpt_ctrl_label label[14] __attribute__((aligned(64)));

	/// Target result label; traces training token.
	struct ircd_gpt_ctrl_label target __attribute__((aligned(64)));

	/// Selected result token label.
	struct ircd_gpt_ctrl_label select __attribute__((aligned(64)));

	/// Incremented when the target is the selected token.
	uint hit, miss;

	/// Attention summary; [layer][head] => [token]. Each value points to a
	/// position in the token buffer. The top-scoring softmax result for each
	/// head in each layer is attending to the token.at(value) for this cycle.
	/// These values are completely updated every cycle.
	ushort attn[12][12];

	/// Header magic: host sets 0xDEADBEEF before release to device; device
	/// sets 0xC7012C7012 before release to host.
	ulong magic;

	/// Token buffer
	ushort token[1024] __attribute__((aligned(2048)));
}
__attribute__((aligned(4096)));

#if defined(__cplusplus)
namespace ircd::gpt
{
	using ctrl = struct ircd_gpt_ctrl;
	using ctrl_clk = struct ircd_gpt_ctrl_clk;
	using ctrl_prof = struct ircd_gpt_ctrl_prof;
	using ctrl_logit = struct ircd_gpt_ctrl_logit;
	using ctrl_label = struct ircd_gpt_ctrl_label;

	string_view debug_token_at(const mutable_buffer &, const opts &, const ctrl &, const uint, const uint fmt = -1U);
	string_view debug_token(const mutable_buffer &, const opts &, const ctrl &, const uint fmt = -1U);
	string_view debug_head(const mutable_buffer &, const opts &, const ctrl_clk &);
	string_view debug_head(const mutable_buffer &, const opts &, const ctrl &);
	string_view debug(const mutable_buffer &, const opts &, const ctrl_logit &, const uint fmt = 0);
	string_view debug(const mutable_buffer &, const opts &, const ctrl_label &, const uint fmt = 0);
	string_view debug(const mutable_buffer &, const opts &, const ctrl &);

	string_view debug_attn(const mutable_buffer &, const opts &, const ctrl &, const uint);
	string_view debug_label(const mutable_buffer &, const opts &, const ctrl &, const uint, const uint fmt = 0);
	string_view debug_top(const mutable_buffer &, const opts &, const ctrl &, const uint);
}
#endif

#if defined(__cplusplus)
static_assert(sizeof(struct ircd_gpt_ctrl) % 4096 == 0);
#endif

#if defined(__cplusplus) && defined(__GLIBCXX__)
static_assert(offsetof(struct ircd_gpt_ctrl, token) == 2048);
static_assert(std::is_standard_layout<struct ircd_gpt_ctrl>::value);
#endif
