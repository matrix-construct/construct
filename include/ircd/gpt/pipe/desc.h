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

	pipe::model *model;
	pipe::code *code;

	cl::data
	state,         // qry/key/val projection (tokens * embed * 3 * float)
	accum,         // accumulator (tokens * embed * float)
	logit,         // result logit vector (50257 * float)
	logexp,        // outputs distribution (50257 * float)
	logsm,         // outputs distribution (50257 * float)
	ctrl,          // control page
	opts;          // options page

	cl::kern
	lm_embed,
	lm_norm,
	lm_logit,
	lm_logsm,
	lm_select,
	lm_norm_backprop,
	lm_embed_backprop;

	std::unique_ptr<struct desc::layer>
	layer[12];

	desc(pipe::code &, pipe::model &);
};

struct ircd::gpt::pipe::desc::layer
{
	cl::kern
	negative,
	positive,
	backattn,
	backffnn;

	layer(pipe::desc &, const int);
};
