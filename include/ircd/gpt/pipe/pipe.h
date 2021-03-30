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
#define HAVE_IRCD_GPT_PIPE_H

namespace ircd::gpt::pipe
{
	struct model;
	struct code;
	struct desc;
	struct exec;
	struct bank;

	extern model *default_model;
	extern code *default_code;
	extern desc *default_desc;

	void init(), fini() noexcept;
};

#include "model.h"
#include "ctrl.h"

struct ircd::gpt::pipe::code
:cl::code
{
	static const string_view compile_opts;

	code();
	~code() noexcept;
};

struct ircd::gpt::pipe::desc
{
	struct layer;

	pipe::model *model;
	pipe::code *code;

	cl::data opts;
	cl::data ctrl;
	cl::data state;
	cl::data xattn;
	cl::data accum;
	cl::data logit;
	cl::kern anode;
	std::unique_ptr<struct desc::layer> layer[12];
	cl::kern cathode;
	cl::kern lmhead;
	cl::kern lmamax;

	desc(pipe::code &, pipe::model &);
};

struct ircd::gpt::pipe::desc::layer
{
	cl::kern negative;
	cl::kern selfattn;
	cl::kern positive;

	layer(pipe::desc &, const int);
};

struct ircd::gpt::pipe::exec
{
	pipe::desc *desc;

	mutable_buffer out_ctrl;
	const_buffer in_ctrl, in_opts;

	cl::kern::range range_anode;
	cl::kern::range range_coil;
	cl::kern::range range_negative;
	cl::kern::range range_selfattn;
	cl::kern::range range_positive;
	cl::kern::range range_cathode;
	cl::kern::range range_lmhead;
	cl::kern::range range_lmamax;

	cl::exec send[2];
	cl::exec tail[1];
	cl::exec coil[12 * 3];
	cl::exec head[3];
	cl::exec recv[1];

	exec(ctor_ctrl &, const ctor_opts &);
	~exec() noexcept;
};
