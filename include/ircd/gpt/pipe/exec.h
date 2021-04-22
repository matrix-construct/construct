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
#define HAVE_IRCD_GPT_PIPE_EXEC_H

/// Perform one task cycle on the device.
///
/// Constructions of this object enqueue device commands to complete an
/// additional epoch of the task as provided by `ctrl` and `opts`.
///
/// Destructions of this object yield the ircd::ctx until those commands
/// are complete.
///
/// Consecutive cycles on the device without stopping (a.k.a. pipelining) is
/// achieved by constructing several objects before following with destructions
/// i.e in a std::deque.
///
struct ircd::gpt::pipe::exec
{
	pipe::desc *desc;

	const_buffer
	send_opts,         // Set when sending the options page.
	send_ctrl,         // Set when sending the control page.
	send_coil,         // Set when sending the updated model.
	send_head;         // Set when sending the updated model.

	mutable_buffer
	recv_ctrl;         // Set when receiving the control page.

	cl::kern::range
	range_lm_embed,    // Dimension range of the lm_embed kernel.
	range_negative,    // Dimension range of a layer kernel.
	range_positive,    // Dimension range of a layer kernel.
	range_lm_norm,     // Dimension range of the final norm kernel.
	range_lm_logit,    // Dimension range of the language logit kernel.
	range_lm_logsm,    // Dimension range of the language statistic kernel.
	range_lm_select;   // Dimension range of the language token kernel.

	cl::exec
	release_opts,      // Release the options page.
	release_ctrl,      // Release the control page.
	release_coil,      // Release updates to the model.
	release_head,      // Release updates to the model.
	lm_embed,          // Compute token and positional embeddings.
	coil[12 * 2],      // Pass over all layers.
	lm_norm,           // Final normalization.
	lm_logit,          // Compute language logits.
	lm_logsm,          // Statistics on the logits.
	lm_select,         // Select next token.
	acquire_ctrl;      // Acquire the control page.

	exec(task &, const size_t tokens, const bool rel, const bool acq);
	~exec() noexcept;
};
