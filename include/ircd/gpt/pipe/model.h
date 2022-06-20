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
#define HAVE_IRCD_GPT_PIPE_MODEL_H

struct ircd::gpt::pipe::model
{
	struct matrix;
	struct tensor;
	struct norm;
	struct proj;
	struct fcon;
	struct attn;
	struct ffnn;
	struct block;
	struct embed;
	struct decoder;

	const gpt::model::decoder *decode_const {nullptr};
	gpt::model::decoder *decode_mutable {nullptr};
	std::unique_ptr<model::decoder> decode;

	model(const gpt::model::decoder &);
	model(gpt::model::decoder &);
	~model() noexcept;
};

struct ircd::gpt::pipe::model::matrix
{
	cl::data
	param,       // Weights
	moment[2];   // Adaptive moment estimations

	matrix(cl::data *, const off_t, const const_buffer &param);
	matrix(cl::data *, const off_t, const mutable_buffer &param);
};

struct ircd::gpt::pipe::model::tensor
{
	matrix
	bias,
	weight;

	tensor(cl::data *, const off_t, const const_buffer &bias, const off_t, const const_buffer &weight);
	tensor(cl::data *, const off_t, const mutable_buffer &bias, const off_t, const mutable_buffer &weight);
};

struct ircd::gpt::pipe::model::attn
{
	tensor
	norm,
	fcon,
	proj;

	attn(cl::data *, const off_t, const gpt::model::attn &);
	attn(cl::data *, const off_t, gpt::model::attn &);
};

struct ircd::gpt::pipe::model::ffnn
{
	tensor
	norm,
	fcon,
	proj;

	ffnn(cl::data *, const off_t, const gpt::model::ffnn &);
	ffnn(cl::data *, const off_t, gpt::model::ffnn &);
};

struct ircd::gpt::pipe::model::block
{
	model::attn
	attn;

	model::ffnn
	ffnn;

	block(cl::data *, const off_t, const gpt::model::block &, const size_t);
	block(cl::data *, const off_t, gpt::model::block &, const size_t);
};

struct ircd::gpt::pipe::model::embed
{
	tensor
	norm;

	matrix
	pos,
	token;

	embed(cl::data *, const off_t, const gpt::model::embed &);
	embed(cl::data *, const off_t, gpt::model::embed &);
};

struct ircd::gpt::pipe::model::decoder
{
	// Combined-layer memory roots
	cl::data
	master[3];

	// Layer blocks
	model::block
	layer[12];

	// Language model head
	model::embed
	embed;

	decoder(const gpt::model::decoder &);
	decoder(gpt::model::decoder &);
	~decoder() noexcept;
};
