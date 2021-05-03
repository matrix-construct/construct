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
#define HAVE_IRCD_GPT_TASK_CTRL_H

/// Task Control Page
///
/// The control block is shared with our device software. Execution state is
/// maintained in the task control block across cycles. The control block is
/// the mutable state component for an execution; for the immutable component
/// also shared with device software see opts.h.
///
struct ircd_gpt_task
{
	/// Epoch counting & interrupt control block.
	struct ircd_gpt_task_epic epic;

	/// Token context control block. Contains state for the token context
	/// buffer; the buffer with the tokens themselves is elsewhere.
	struct ircd_gpt_task_tokens tokens;

	/// Logit softmax state
	struct ircd_math_samax samax;

	/// Target label loss state
	struct ircd_math_mean loss;

	/// Target label perplexity score state
	struct ircd_math_mean perp;

	/// Target label certainty difference state
	struct ircd_math_mean cert;

	/// PRNG xoshiro256 state. This is the de facto random seed which can be
	/// set before cycle entry by the host. It is updated by device software
	/// when used.
	ulong rand[4];

	/// Perform backprop
	bool prop;

	/// Header magic 0xC7012C70
	uint magic;

	/// The token buffer starts at offset 2048 and continues to the end of
	/// the page; options specify the size of the tokens buffer in tokens.
	/// Additional pages must be attached for larger buffer sizes.
	ushort token[] __attribute__((aligned(2048)));
}
__attribute__((aligned(4096)));
