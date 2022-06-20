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
#define HAVE_IRCD_GPT_PIPE_DESC_H

/// Pipe descriptor
struct ircd::gpt::pipe::desc
{
	struct layer;

	// Model descriptor
	pipe::model *model;

	// Code descriptor
	pipe::code *code;

	// Memories
	cl::data
	opts,          // [root] options page
	ctrl,          // [root] control page
	master,        // [root] single allocation for additional buffers:
	state,         // [-sub] projection (layers * tokens * embed * 3 * float)
	accum,         // [-sub] accumulator (tokens * embed * float)
	logit,         // [-sub] result logit vector (50257 * float)
	attns,         // [-sub] result attention softmax
	frame[8];      // [root] result stream

	// Programs
	cl::kern
	alloc,
	enter,
	lm_embed,
	lm_norm,
	lm_logit,
	lm_logsm,
	lm_select,
	lm_prop_embed,
	lm_prop_norm,
	leave[8];

	/// Coil pack
	std::unique_ptr<struct desc::layer> layer[12];

	/// Attention projection for first N tokens already contained in `state`.
	uint cached {0};

	desc(const gpt::opts *const &,
	     gpt::ctrl *const &,
	     pipe::model &model,
	     pipe::code &code);
};

/// Pipe descriptor: coil layer
struct ircd::gpt::pipe::desc::layer
{
	cl::data
	state,         // [-sub] qry/key/val projection (tokens * embed * 3 * float)
	attns;         // [-sub] attn softmax result (((tokens * tokens) / 2) * 12 * float)

	cl::kern
	attn,
	ffnn,
	prop_attn,
	prop_ffnn;

	layer(pipe::desc &,
	      const gpt::opts *const &,
	      const uint laynum);
};
