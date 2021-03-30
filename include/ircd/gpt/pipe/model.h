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

	model(const gpt::model::decoder &, const gpt::model::embed &);
	~model() noexcept;
};

struct ircd::gpt::pipe::model::tensor
{
	cl::data bias, weight;

	tensor(const const_buffer &bias, const const_buffer &weight);
	tensor(cl::data &, const off_t, const const_buffer &bias, const const_buffer &weight);
};

struct ircd::gpt::pipe::model::attn
{
	tensor norm, fcon, proj;
	cl::data mask;

	attn(cl::data &, const off_t, const gpt::model::norm &, const gpt::model::attn &);
};

struct ircd::gpt::pipe::model::ffnn
{
	tensor norm, fcon, proj;

	ffnn(cl::data &, const off_t, const gpt::model::norm &, const gpt::model::ffnn &);
};

struct ircd::gpt::pipe::model::block
{
	cl::data master;
	model::attn attn;
	model::ffnn ffnn;

	block(const gpt::model::block &, const size_t);
};

struct ircd::gpt::pipe::model::decoder
{
	model::block block[12];
	tensor norm;

	decoder(const gpt::model::decoder &);
	~decoder() noexcept;
};

struct ircd::gpt::pipe::model::language
{
	cl::data pos, token;

	language(const gpt::model::embed &);
	~language() noexcept;
};
