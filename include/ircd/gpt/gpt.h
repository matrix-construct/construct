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

	struct opts;
	struct task;

	extern const opts default_opts;
	extern log::log log;
}

#include "vocab.h"
#include "model.h"
#include "task.h"
#include "generate.h"

/// Primary Options
///
/// Use this structure to configure and control specifics of the machine.
/// These settings are immutable for the operations. To maintain state between
/// calls see task.h
///
struct ircd::gpt::opts
{
	/// Specifies the nominal halting condition based on the sequence of
	/// tokens. Generation will complete when this sequence is witnessed. Set
	/// tokens to -1 starting from the back to not match that token. Setting
	/// all tokens to -1 will ignore this condition.
	uint accept_code[3][3]
	{
		{    13,  198,  -1U,  },
		{   198,  198,  -1U,  },
		{   -1U,  -1U,  -1U,  },
	};

	/// Specifies the exceptional halting condition based on the sequence of
	/// tokens. By default, the three zeros represent three outputs of '!'
	/// which is probably an error code; note that a true "!!!" is represented
	/// by token number 10185. Set tokens to -1 starting from the back to
	/// not match that token; generated output after errors is usually garbage.
	uint error_code[3][3]
	{
		{     0,    0,    0,  },
		{   -1U,    0,    0,  },
		{   -1U,    0,    0,  },
	};

	/// Limit number of output tokens. Default of -1 is unlimited; the number
	/// of tokens generated will be limited by other factors.
	uint limit
	{
		-1U
	};

	/// Flip random coins over the top k logits each round. Setting to 1
	/// deterministically selects the top logit.
	uint top_k
	{
		2
	};

	/// Pointer to the model
	const model::decoder *model
	{
		model::default_model
	};
};
