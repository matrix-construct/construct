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
#define HAVE_IRCD_GPT_GPT_H

/// Generative Pre-trained Transformer
///
namespace ircd::gpt
{
	IRCD_EXCEPTION(ircd::error, error)

	struct samp;
	struct step;
	struct epoch;
	struct task;

	extern log::log log;
}

#include "vocab.h"
#include "token.h"
#include "vector.h"
#include "model.h"
#include "opts.h"
#include "ctrl.h"
#include "pipe/pipe.h"
#include "samp.h"
#include "step.h"
#include "epoch.h"
#include "task.h"

namespace ircd::gpt
{
	void backprop(const opts &, const u32, const f32, model::decoder &, f32 *const __restrict__ [2]) noexcept;

	void log_debug(const opts &, const ctrl &);
	void log_debug_token(const opts &, const ctrl &, const uint);
	void log_debug_attns(const opts &, const ctrl &);
	void log_debug_attns_top(const opts &, const ctrl &);
	void log_debug_labels(const opts &, const ctrl &);
	void log_debug_topn(const opts &, const ctrl &);
	void log_debug_prof(const opts &, const ctrl &, const pipe::prof &);
}
