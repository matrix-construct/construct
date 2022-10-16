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

	struct prop;
	struct text;

	extern decoder *default_model;
	extern float *default_moment[2];
	extern float *default_checkpoint[3];
	extern string_view default_dataset;
	extern std::vector<json::object> default_data;

	constexpr auto alignment {4096};
	extern conf::item<bool> cache_shared;
}

/// Layer normalization
struct ircd::gpt::model::norm
{
	union ircd_gpt_vector
	bias alignas(alignment),
	weight alignas(alignment);
};

/// Attention aperature
struct ircd::gpt::model::attn
{
	model::norm
	norm;

	union ircd_gpt_attn_aperature
	fcon_bias alignas(alignment),
	fcon_weight alignas(alignment) [768];

	union ircd_gpt_vector
	proj_bias alignas(alignment),
	proj_weight alignas(alignment) [768];
};

/// Feed-forward neural network
struct ircd::gpt::model::ffnn
{
	model::norm
	norm;

	union ircd_gpt_ffnn_aperature
	fcon_bias alignas(alignment),
	fcon_weight alignas(alignment) [768];

	union ircd_gpt_vector
	proj_bias alignas(alignment),
	proj_weight alignas(alignment) [3072];
};

/// Transformer block
struct ircd::gpt::model::block
{
	model::attn
	attn;

	model::ffnn
	ffnn;
};

/// Vocabulary embeddings
struct ircd::gpt::model::embed
{
	model::norm
	norm;

	union ircd_gpt_vector
	pos alignas(alignment) [1024],
	token alignas(alignment) [65536];
};

/// Transformer decoder
struct alignas(ircd::gpt::model::alignment)
ircd::gpt::model::decoder
{
	model::block
	layer[12];

	model::embed
	embed;
};

struct ircd::gpt::model::prop
{
	static constexpr const char
	*const ended {"ended"},
	*const id {"id"},
	*const length {"length"},
	*const text {"text"};
};

struct ircd::gpt::model::text
:json::tuple
<
	json::property<prop::ended, bool>,
	json::property<prop::id, uint>,
	json::property<prop::length, uint>,
	json::property<prop::text, json::string>
>
{
	using super_type::tuple;
};
