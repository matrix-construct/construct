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

/// Epoch Precision Interrupt Controller
///
struct ircd_gpt_ctrl_epic
{
	/// Accumulates the number of task cycles. The cycle counter is incremented
	/// by device software after each repetition of the kernel pipeline to
	/// produce one additional token.
	ulong cycle;

	/// Accumulates the epoch count for the task. The counter is incremented
	/// by one in device software before control returns back to the host.
	/// Several cycles may occur during each epoch.
	ulong epoch;

	/// Accumulates the training epoch count for the task. The counter is
	/// incremented by one in device software for each backward propagation.
	ulong step;

	/// Updated by the host with the value of the timestamp register as sampled
	/// immediately before each transfer of control to the device.
	ulong host_tsc;

	/// Accumulates time in microseconds elapsed for the task.
	ulong elapsed;
};

/// Token Context Buffer (Control Block)
///
struct ircd_gpt_ctrl_tokens
{
	/// Token ring head. Tokens in the ring extend behind the head for
	/// `tokens`. The `head` value is automatically modulated by device
	/// software to wrap around the ring.
	uint head;

	/// Token counter. The counter indicates the number of valid tokens in
	/// the context buffer. This value must not exceed the buffer size.
	uint count;

	/// Accumulates the number of tokens produced by the task. Several tokens
	/// may be produced each epoch, but currently only one token is produced
	/// each cycle.
	ulong produced;

	/// Accumulates the number tokens witnessed by the task. The number of
	/// tokens in the context for each cycle is counted as witnessed.
	ulong witnessed;
};

/// Target label register (abridged)
///
struct ircd_gpt_ctrl_logit
{
	/// Vocabulary token.
	ushort token;

	/// Padding #0.
	ushort _pad0;

	/// Result logit softmax probability.
	float samax;
};

/// Target label register (full)
///
struct ircd_gpt_ctrl_label
{
	/// Vocabulary token.
	ushort token;

	/// Padding #0.
	ushort _pad0;

	/// Result logit softmax probability.
	float samax;

	/// Loss state
	struct ircd_math_mean loss;

	/// Perplexity state
	struct ircd_math_mean perp;
}
__attribute__((aligned(64)));

/// Task Control Page
///
/// The control block is shared with our device software. Execution state is
/// maintained in the task control block across cycles. The control block is
/// the mutable state component for an execution; for the immutable component
/// also shared with device software see opts.h.
///
struct ircd_gpt_ctrl
{
	/// Epoch counting & interrupt control block.
	struct ircd_gpt_ctrl_epic epic;

	/// Token context control block. Contains state for the token context
	/// buffer; the buffer with the tokens themselves is elsewhere.
	struct ircd_gpt_ctrl_tokens tokens;

	/// Top result summary from the softed result logit softmax vector. This
	/// is updated each cycle by device software with extended statistics on
	/// the top N results.
	struct ircd_gpt_ctrl_logit top[16];

	/// Target label control block. Results for each target are registered
	/// and state is updated each cycle.
	struct ircd_gpt_ctrl_label label[4];

	/// Result logit vector softmax internal state.
	struct ircd_math_samax samax;

	/// PRNG xoshiro256 internal state (note: see opts.h to seed the prng).
	ulong rand[4];

	/// Perform backprop TODO: XXX
	bool prop;

	/// Header magic 0xC7012C70
	uint magic;

	/// The token buffer starts at offset 2048 and continues to the end of
	/// the page; options specify the size of the tokens buffer in tokens.
	/// Additional pages must be attached for larger buffer sizes.
	ushort token[] __attribute__((aligned(2048)));
}
__attribute__((aligned(4096)));

#ifdef __cplusplus
namespace ircd::gpt
{
	using ctrl = struct ircd_gpt_ctrl;
}
#endif

#ifdef __cplusplus
static_assert(sizeof(struct ircd_gpt_ctrl) == 4096);
static_assert(offsetof(struct ircd_gpt_ctrl, token) == 2048);
static_assert(std::is_standard_layout<struct ircd_gpt_ctrl>::value);
#endif
