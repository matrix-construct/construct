// Tensor Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_GPT_MODEL_H

namespace ircd::gpt::model
{
	struct norm;
	struct attn;
	struct ffnn;
	struct block;
	struct decoder;
}

/// Attention aperature
struct ircd::gpt::model::attn
{
	float
	attn_bias    alignas(64) [2304],
	attn_weight  alignas(64) [768][2304],
	proj_bias    alignas(64) [768],
	proj_weight  alignas(64) [768][768];
	bool bias    alignas(64) [1024][1024];
};

/// Feed-forward neural network
struct ircd::gpt::model::ffnn
{
	float
	fc_bias      alignas(64) [3072],
	fc_weight    alignas(64) [768][3072],
	proj_bias    alignas(64) [768],
	proj_weight  alignas(64) [3072][768];
};

/// Layer normalization
struct ircd::gpt::model::norm
{
	float
	bias    alignas(64) [768],
	weight  alignas(64) [768];
};

/// Transformer block
struct ircd::gpt::model::block
{
	norm ln1;
	model::attn attn;
	norm ln2;
	model::ffnn ffnn;
};

struct ircd::gpt::model::decoder
{
	float
	wpe  alignas(64) [1024][768],
	wte  alignas(64) [65536][768];
	block layer[12];
	norm f;
};
