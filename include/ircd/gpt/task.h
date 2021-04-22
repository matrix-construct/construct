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
#define HAVE_IRCD_GPT_TASK_H

/// Task Control Page
///
/// The control block is shared with our device software. Execution state is
/// maintained in the task control block across cycles. The control block is
/// the mutable state component for an execution; for the immutable component
/// also shared with device software see opts.h.
///
struct ircd_gpt_task
{
	/// Header magic 0xC7012C70
	uint magic;

	/// Hypercall code set by our device software upon completion and control
	/// transfer back to the host. Negative codes indicate errors, positive
	/// codes are used for status and/or procedure calls; zero is also an error.
	enum ircd_gpt_hypercall call;

	/// Token ring head. Tokens in the ring extend behind the head for
	/// `tokens`. The `head` value is automatically modulated by device
	/// software to wrap around the ring.
	uint head;

	/// Token counter. The counter indicates the number of valid tokens in
	/// the context buffer. This value must not exceed the buffer size.
	uint tokens;

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

	/// Accumulates the number of tokens produced by the task. Several tokens
	/// may be produced each epoch, but currently only one token is produced
	/// each cycle.
	ulong produced;

	/// Accumulates the number tokens witnessed by the task. The number of
	/// tokens in the context for each cycle is counted as witnessed.
	ulong witnessed;

	/// Accumulates time in microseconds elapsed for the task.
	ulong elapsed;

	/// PRNG xoshiro256 state. This is the de facto random seed which can be
	/// set before cycle entry by the host. It is updated by device software
	/// when used.
	ulong rand[4];

	/// Updated by the host with the value of the timestamp register as sampled
	/// immediately before each transfer of control to the device.
	ulong host_tsc;

	/// State counters for the accept/error sequence codes.
	uint accept_seq[4], error_seq[4];

	/// Logit softmax mu
	float samax_mu;

	/// Logit softmax sum
	float samax_sum;

	/// Logit softmax lambda
	float samax_lambda;

	/// Loss for last token of last cycle
	float loss;

	/// Sum loss over all cycles
	float loss_sum[4];

	/// Average loss over all cycles
	float loss_mean;

	/// Perplexity score for last token of last cycle
	float perp;

	/// Sum ppl over all cycles
	float perp_sum[4];

	/// Perplexity mean over context
	float perp_mean;

	/// Certainty difference score for last token of last cycle
	float cert;

	/// Sum certainty over all cycles
	float cert_sum[4];

	/// Certainty mean over context
	float cert_mean;

	/// Final loss
	float l2_loss;

	/// Final loss mean
	float l2_loss_mean;

	/// Perform backprop
	bool prop;

	/// The token buffer starts at offset 2048 and continues to the end of
	/// the page; options specify the size of the tokens buffer in tokens.
	/// Additional pages must be attached for larger buffer sizes.
	ushort token[] __attribute__((aligned(2048)));
}
__attribute__((aligned(4096)));

#ifdef __cplusplus
/// Task Context
///
/// State for a task.
struct ircd::gpt::task
{
	enum status :char;

	/// Reference to the attached options.
	const gpt::opts *opts {nullptr};

	/// Reference to control pages.
	struct ircd_gpt_task *ctrl {nullptr};

	/// Current task status.
	enum status status {'\0'};

	task(const gpt::opts *       = nullptr,
	     struct ircd_gpt_task *  = nullptr);

	~task() noexcept;
};

/// The current status of a task is indicated with intelligible characters
enum ircd::gpt::task::status
:char
{
	QUEUED    = 'Q',  ///< Queued for execution.
	RUNNING   = 'R',  ///< Currently being executed.
	ACCEPT    = 'A',  ///< Execution completed successfully.
	ERROR     = 'E',  ///< Execution did not complete successfully.
};

static_assert(sizeof(struct ircd_gpt_task) == 4096);
static_assert(offsetof(struct ircd_gpt_task, token) == 2048);
static_assert(std::is_standard_layout<struct ircd_gpt_task>::value);
#endif
