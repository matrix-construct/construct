// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_GPT_PIPE_RANGE_H

struct ircd::gpt::pipe::range
{
	cl::kern::range
	_full,
	_last,
	alloc,
	embed,
	attn,
	ffnn,
	fffnn,
	fnorm,
	logit,
	logsm,
	select,
	prop_embed,
	prop_norm,
	prop_attn,
	prop_ffnn;

	range(const opts &opts,
	      const uint tick,
	      const uint count,
	      const uint tokens,
	      const uint cached,
	      const bool fwd,
	      const bool rev) noexcept;
};
