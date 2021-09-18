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

	struct task;
	struct gate;

	extern log::log log;
}

#include "hypercall.h"
#include "vocab.h"
#include "model.h"
#include "token.h"
#include "gate.h"
#include "opts.h"
#include "ctrl.h"
#include "task.h"
#include "pipe/pipe.h"
#include "generate.h"
