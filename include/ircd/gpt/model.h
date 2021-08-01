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
	struct embed;
	struct decoder;

	constexpr auto align {64};
	extern decoder *default_model;
	extern string_view default_dataset;
	extern std::vector<json::object> default_data;
}

/// Attention aperature
struct ircd::gpt::model::attn
{
	float
	attn_bias    alignas(align) [2304],
	attn_weight  alignas(align) [768][2304];

	bool
	bias         alignas(align) [1024][1024];

	float
	proj_bias    alignas(align) [768],
	proj_weight  alignas(align) [768][768];
};

/// Feed-forward neural network
struct ircd::gpt::model::ffnn
{
	float
	fc_bias      alignas(align) [3072],
	fc_weight    alignas(align) [768][3072];

	float
	proj_bias    alignas(align) [768],
	proj_weight  alignas(align) [3072][768];
};

/// Layer normalization
struct ircd::gpt::model::norm
{
	float
	bias    alignas(align) [768],
	weight  alignas(align) [768];
};

/// Transformer block
struct ircd::gpt::model::block
{
	norm ln1;
	model::attn attn;

	norm ln2;
	model::ffnn ffnn;
};

/// Vocabulary embeddings
struct ircd::gpt::model::embed
{
	float
	pos     alignas(align) [1024][768],
	token   alignas(align) [65536][768];
};

struct ircd::gpt::model::decoder
{
	block layer[12];

	norm f;
	embed word;
};
