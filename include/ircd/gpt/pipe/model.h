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
	struct decoder;
	struct language;

	std::unique_ptr<model::decoder> decode;
	std::unique_ptr<model::language> embed;
	bool invalid {false};

	model(const gpt::model::decoder &, const gpt::model::embed &);
	model(gpt::model::decoder &, gpt::model::embed &);
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

	tensor(cl::data *, const off_t, const const_buffer &bias, const const_buffer &weight);
	tensor(cl::data *, const off_t, const mutable_buffer &bias, const mutable_buffer &weight);
};

struct ircd::gpt::pipe::model::attn
{
	tensor
	norm,
	fcon,
	proj;

	cl::data
	mask;

	attn(cl::data *, const off_t, const gpt::model::norm &, const gpt::model::attn &);
	attn(cl::data *, const off_t, gpt::model::norm &, gpt::model::attn &);
};

struct ircd::gpt::pipe::model::ffnn
{
	tensor
	norm,
	fcon,
	proj;

	ffnn(cl::data *, const off_t, const gpt::model::norm &, const gpt::model::ffnn &);
	ffnn(cl::data *, const off_t, gpt::model::norm &, gpt::model::ffnn &);
};

struct ircd::gpt::pipe::model::block
{
	// Single layer memory roots
	cl::data
	master[3];

	// Layer units
	model::attn attn;
	model::ffnn ffnn;

	block(cl::data *, const off_t, const gpt::model::block &, const size_t);
	block(cl::data *, const off_t, gpt::model::block &, const size_t);
	block(const gpt::model::block &, const size_t);
	block(gpt::model::block &, const size_t);
};

struct ircd::gpt::pipe::model::language
{
	cl::data
	master[3];

	matrix
	pos,
	token;

	language(cl::data *, const off_t, const gpt::model::embed &);
	language(cl::data *, const off_t, gpt::model::embed &);
	language(const gpt::model::embed &);
	language( gpt::model::embed &);
	~language() noexcept;
};

struct ircd::gpt::pipe::model::decoder
{
	// Combined-layer memory roots
	cl::data
	master[3];

	// Layer blocks
	model::block
	block[12];

	// Final norm
	tensor norm;

	decoder(const gpt::model::decoder &);
	decoder(gpt::model::decoder &);
	~decoder() noexcept;
};
