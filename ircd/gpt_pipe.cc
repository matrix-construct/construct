// Tensor Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/gpt/pipe/pipe.h>

namespace ircd::gpt
{
	void transform(ctor_ctrl &, const ctor_opts &);
}

namespace ircd::gpt::pipe
{
	static ircd::cl::exec::opts negative_opts, positive_opts, selfattn_opts, cathode_opts, anode_opts,
	lmhead_opts, lmamax_opts;

	extern const ircd::run::changed handle_quit;
}

decltype(ircd::gpt::pipe::default_model)
ircd::gpt::pipe::default_model;

decltype(ircd::gpt::pipe::default_code)
ircd::gpt::pipe::default_code;

decltype(ircd::gpt::pipe::default_desc)
ircd::gpt::pipe::default_desc;

decltype(ircd::gpt::pipe::handle_quit)
ircd::gpt::pipe::handle_quit
{
	run::level::QUIT, pipe::fini
};

void
ircd::gpt::pipe::init()
{
	const auto &default_model
	{
		*gpt::model::default_model
	};

	assert(!pipe::default_model);
	pipe::default_model = new pipe::model
	{
		default_model, default_model.word
	};

	pipe::default_code = new pipe::code
	{

	};

	pipe::default_desc = new pipe::desc
	{
		*pipe::default_code, *pipe::default_model
	};
}

void
ircd::gpt::pipe::fini()
noexcept
{
	delete default_desc;
	default_desc = nullptr;

	delete default_code;
	default_code = nullptr;

	delete default_model;
	default_model = nullptr;
}

//
// pipe
//

void
ircd::gpt::transform(ctor_ctrl &ctrl,
                     const ctor_opts &opts)
{
	if(unlikely(!pipe::default_model))
		pipe::init();

	ctrl.call = -1;
	pipe::exec
	{
		ctrl, opts
	};

	if(unlikely(ctrl.call <= 0))
		throw error
		{
			"hyper (#%d) :%s",
			abs(ctrl.call),
			ctrl.body.str,
		};
}

//
// pipe::exec
//

ircd::gpt::pipe::exec::exec(ctor_ctrl &ctrl,
                            const ctor_opts &opts)
:desc
{
	default_desc
}
,out_ctrl
{
	reinterpret_cast<char *>(&ctrl), sizeof(ctor_ctrl)
}
,in_ctrl
{
	reinterpret_cast<const char *>(&ctrl), sizeof(ctor_ctrl)
}
,in_opts
{
	reinterpret_cast<const char *>(&opts), sizeof(ctor_opts)
}
,range_anode
{
	{ ctrl.tokens, 0, },
	{           1, 0, },
}
,range_coil
{
	{ ctrl.tokens * 192UL, 0, },
	{               192UL, 0, },
}
,range_negative
{
	range_coil
}
,range_selfattn
{
	range_coil
}
,range_positive
{
	range_coil
}
,range_cathode
{
	{                 1 * 192UL, 0 },
	{                     192UL, 0 },
	{ (ctrl.tokens - 1) * 192UL, 0 },
}
,range_lmhead
{
	{ 262 * 192UL, 0 },  // align_up(50257) / 192
	{       192UL, 0 },
}
,range_lmamax
{
	{   1 * 192UL, 0 },
	{       192UL, 0 },
}
,send
{
	{ desc->opts, in_opts },
	{ desc->ctrl, in_ctrl },
}
,tail
{
	{ desc->anode,     range_anode,   anode_opts    },
}
,coil
{
	{ desc->layer[0x00]->negative, range_negative, negative_opts },
	{ desc->layer[0x00]->selfattn, range_selfattn, selfattn_opts },
	{ desc->layer[0x00]->positive, range_positive, positive_opts },
	{ desc->layer[0x01]->negative, range_negative, negative_opts },
	{ desc->layer[0x01]->selfattn, range_selfattn, selfattn_opts },
	{ desc->layer[0x01]->positive, range_positive, positive_opts },
	{ desc->layer[0x02]->negative, range_negative, negative_opts },
	{ desc->layer[0x02]->selfattn, range_selfattn, selfattn_opts },
	{ desc->layer[0x02]->positive, range_positive, positive_opts },
	{ desc->layer[0x03]->negative, range_negative, negative_opts },
	{ desc->layer[0x03]->selfattn, range_selfattn, selfattn_opts },
	{ desc->layer[0x03]->positive, range_positive, positive_opts },
	{ desc->layer[0x04]->negative, range_negative, negative_opts },
	{ desc->layer[0x04]->selfattn, range_selfattn, selfattn_opts },
	{ desc->layer[0x04]->positive, range_positive, positive_opts },
	{ desc->layer[0x05]->negative, range_negative, negative_opts },
	{ desc->layer[0x05]->selfattn, range_selfattn, selfattn_opts },
	{ desc->layer[0x05]->positive, range_positive, positive_opts },
	{ desc->layer[0x06]->negative, range_negative, negative_opts },
	{ desc->layer[0x06]->selfattn, range_selfattn, selfattn_opts },
	{ desc->layer[0x06]->positive, range_positive, positive_opts },
	{ desc->layer[0x07]->negative, range_negative, negative_opts },
	{ desc->layer[0x07]->selfattn, range_selfattn, selfattn_opts },
	{ desc->layer[0x07]->positive, range_positive, positive_opts },
	{ desc->layer[0x08]->negative, range_negative, negative_opts },
	{ desc->layer[0x08]->selfattn, range_selfattn, selfattn_opts },
	{ desc->layer[0x08]->positive, range_positive, positive_opts },
	{ desc->layer[0x09]->negative, range_negative, negative_opts },
	{ desc->layer[0x09]->selfattn, range_selfattn, selfattn_opts },
	{ desc->layer[0x09]->positive, range_positive, positive_opts },
	{ desc->layer[0x0a]->negative, range_negative, negative_opts },
	{ desc->layer[0x0a]->selfattn, range_selfattn, selfattn_opts },
	{ desc->layer[0x0a]->positive, range_positive, positive_opts },
	{ desc->layer[0x0b]->negative, range_negative, negative_opts },
	{ desc->layer[0x0b]->selfattn, range_selfattn, selfattn_opts },
	{ desc->layer[0x0b]->positive, range_positive, positive_opts },
}
,head
{
	{ desc->cathode,  range_cathode,  cathode_opts  },
	{ desc->lmhead,   range_lmhead,   lmhead_opts   },
	{ desc->lmamax,   range_lmamax,   lmamax_opts   },
}
,recv
{
	{ desc->ctrl, out_ctrl },
}
{
}

ircd::gpt::pipe::exec::~exec()
noexcept
{
}

//
// code
//

decltype(ircd::gpt::pipe::code::compile_opts)
ircd::gpt::pipe::code::compile_opts
{
	" -cl-strict-aliasing"
	" -cl-no-signed-zeros"
	" -cl-finite-math-only"
	" -cl-unsafe-math-optimizations"
	" -cl-fast-relaxed-math"
	//" -cl-mad-enable"
	//" -cl-single-precision-constant"
	//" -cl-fp32-correctly-rounded-divide-sqrt"
};

ircd::gpt::pipe::code::code()
:cl::code{[]
{
	const fs::fd fd
	{

	};

	const std::string read
	{
		fs::read(fd)
	};

	const string_view bin
	{
		read
	};

	const vector_view<const string_view> bins
	(
		&bin, 1
	);

	return cl::code
	{
		bins, compile_opts
	};
}()}
{
}

ircd::gpt::pipe::code::~code()
noexcept
{
}

//
// pipe::desc
//

ircd::gpt::pipe::desc::desc(pipe::code &code,
                            pipe::model &model)
:model
{
	&model
}
,code
{
	&code
}
,opts
{
	4_KiB,
	const_buffer{}
}
,ctrl
{
	4_KiB,
	mutable_buffer{}
}
,state
{
	32 * 3 * 768 * sizeof(float),
	mutable_buffer{}
}
,xattn
{
	32 * 1 * 768 * sizeof(float),
	mutable_buffer{}
}
,accum
{
	32 * 768 * sizeof(float),
	mutable_buffer{}
}
,logit
{
	65536 * sizeof(float),
	mutable_buffer{}
}
,anode
{
	code,
	"ctor_anode2",
	ctrl,
	opts,
	accum,
	model.embed->pos,
	model.embed->token,
}
,layer
{
	std::make_unique<struct desc::layer>(*this, 0x00),
	std::make_unique<struct desc::layer>(*this, 0x01),
	std::make_unique<struct desc::layer>(*this, 0x02),
	std::make_unique<struct desc::layer>(*this, 0x03),
	std::make_unique<struct desc::layer>(*this, 0x04),
	std::make_unique<struct desc::layer>(*this, 0x05),
	std::make_unique<struct desc::layer>(*this, 0x06),
	std::make_unique<struct desc::layer>(*this, 0x07),
	std::make_unique<struct desc::layer>(*this, 0x08),
	std::make_unique<struct desc::layer>(*this, 0x09),
	std::make_unique<struct desc::layer>(*this, 0x0a),
	std::make_unique<struct desc::layer>(*this, 0x0b),
}
,cathode
{
	code,
	"ctor_cathode",
	ctrl,
	opts,
	accum,
	model.decode->norm.bias,
	model.decode->norm.weight,
}
,lmhead
{
	code,
	"ctor_lmhead",
	ctrl,
	opts,
	logit,
	accum,
	model.embed->token,
}
,lmamax
{
	code,
	"ctor_lmamax",
	ctrl,
	opts,
	logit,
}
{
}

//
// pipe::desc::layer
//

ircd::gpt::pipe::desc::layer::layer(pipe::desc &desc,
                                    const int laynum)
:negative
{
	*desc.code,
	"ctor_attn_fcon",
	desc.ctrl,
	desc.opts,
	desc.state,
	desc.accum,
	desc.model->decode->block[laynum].attn.norm.bias,
	desc.model->decode->block[laynum].attn.norm.weight,
	desc.model->decode->block[laynum].attn.fcon.bias,
	desc.model->decode->block[laynum].attn.fcon.weight,
}
,selfattn
{
	*desc.code,
	"ctor_attn_self",
	desc.ctrl,
	desc.opts,
	desc.xattn,
	desc.state,
	desc.model->decode->block[laynum].attn.mask,
}
,positive
{
	*desc.code,
	"ctor_backend",
	desc.ctrl,
	desc.opts,
	desc.accum,
	desc.xattn,
	desc.model->decode->block[laynum].attn.proj.bias,
	desc.model->decode->block[laynum].attn.proj.weight,
	desc.model->decode->block[laynum].ffnn.norm.bias,
	desc.model->decode->block[laynum].ffnn.norm.weight,
	desc.model->decode->block[laynum].ffnn.fcon.bias,
	desc.model->decode->block[laynum].ffnn.fcon.weight,
	desc.model->decode->block[laynum].ffnn.proj.bias,
	desc.model->decode->block[laynum].ffnn.proj.weight,
}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// model
//

//
// pipe::model::model
//

ircd::gpt::pipe::model::model(const gpt::model::decoder &decoder,
                              const gpt::model::embed &embed)
:decode
{
	std::make_unique<model::decoder>(decoder)
}
,embed
{
	std::make_unique<model::language>(embed)
}
{
}

ircd::gpt::pipe::model::~model()
noexcept
{
}

//
// pipe::model::language
//

ircd::gpt::pipe::model::language::language(const gpt::model::embed &embed)
:pos
{
	sizeof(embed.pos),
	const_buffer{embed.pos}
}
,token
{
	sizeof(embed.token),
	const_buffer{embed.token}
}
{
}

ircd::gpt::pipe::model::language::~language()
noexcept
{
}

//
// pipe::model::decoder
//

ircd::gpt::pipe::model::decoder::decoder(const gpt::model::decoder &decoder)
:block
{
	{ decoder.layer[0x00], 0x00, },
	{ decoder.layer[0x01], 0x01, },
	{ decoder.layer[0x02], 0x02, },
	{ decoder.layer[0x03], 0x03, },
	{ decoder.layer[0x04], 0x04, },
	{ decoder.layer[0x05], 0x05, },
	{ decoder.layer[0x06], 0x06, },
	{ decoder.layer[0x07], 0x07, },
	{ decoder.layer[0x08], 0x08, },
	{ decoder.layer[0x09], 0x09, },
	{ decoder.layer[0x0a], 0x0a, },
	{ decoder.layer[0x0b], 0x0b, },
}
,norm
{
	const_buffer{decoder.f.bias},
	const_buffer{decoder.f.weight},
}
{
}

ircd::gpt::pipe::model::decoder::~decoder()
noexcept
{
}

//
// pipe::model::block
//

ircd::gpt::pipe::model::block::block(const gpt::model::block &block,
                                     const size_t layer)
:master
{
	sizeof(block), const_buffer
	{
		reinterpret_cast<const char *>(&block), sizeof(block)
	}
}
,attn
{
	master,
	0,
	block.ln1,
	block.attn,
}
,ffnn
{
	master,
	sizeof(block.ln1) + sizeof(block.attn),
	block.ln2,
	block.ffnn,
}
{
}

//
// pipe::model::ffnn
//

ircd::gpt::pipe::model::ffnn::ffnn(cl::data &master,
                                   const off_t offset,
                                   const gpt::model::norm &norm,
                                   const gpt::model::ffnn &ffnn)
:norm
{
	master,
	offset,
	const_buffer{norm.bias},
	const_buffer{norm.weight},
}
,fcon
{
	master,
	offset + off_t(sizeof(norm)),
	const_buffer{ffnn.fc_bias},
	const_buffer{ffnn.fc_weight},
}
,proj
{
	master,
	offset + off_t(sizeof(norm) + sizeof(ffnn.fc_bias) + sizeof(ffnn.fc_weight)),
	const_buffer{ffnn.proj_bias},
	const_buffer{ffnn.proj_weight},
}
{
	always_assert
	(
		ircd::data(const_buffer{ffnn.proj_weight})
		==
		ircd::data(const_buffer{norm.bias}) +
		sizeof(norm) +
		sizeof(ffnn.fc_bias) +
		sizeof(ffnn.fc_weight) +
		ircd::size(const_buffer{ffnn.proj_bias})
	);
}

//
// pipe::model::attn
//

ircd::gpt::pipe::model::attn::attn(cl::data &master,
                                   const off_t offset,
                                   const gpt::model::norm &norm,
                                   const gpt::model::attn &attn)
:norm
{
	master,
	offset,
	const_buffer{norm.bias},
	const_buffer{norm.weight},
}
,fcon
{
	master,
	offset + off_t(sizeof(norm)),
	const_buffer{attn.attn_bias},
	const_buffer{attn.attn_weight},
}
,proj
{
	master,
	offset + off_t(sizeof(norm) + sizeof(attn.attn_bias) + sizeof(attn.attn_weight) + sizeof(attn.bias)),
	const_buffer{attn.proj_bias},
	const_buffer{attn.proj_weight},
}
,mask
{
	master,
	{
		sizeof(attn.bias),
		offset + off_t(sizeof(norm) + sizeof(attn.attn_bias) + sizeof(attn.attn_weight)),
	},
}
{
	always_assert
	(
		ircd::data(const_buffer{attn.proj_weight})
		==
		ircd::data(const_buffer{norm.bias}) +
		sizeof(norm) +
		sizeof(attn.bias) +
		sizeof(attn.attn_bias) +
		sizeof(attn.attn_weight) +
		ircd::size(const_buffer{attn.proj_bias})
	);
}

//
// pipe::model::tensor
//

ircd::gpt::pipe::model::tensor::tensor(const const_buffer &bias,
                                       const const_buffer &weight)
:bias
{
	ircd::size(bias),
	bias,
}
,weight
{
	ircd::size(weight),
	weight,
}
{
}

ircd::gpt::pipe::model::tensor::tensor(cl::data &master,
                                       const off_t offset,
                                       const const_buffer &bias,
                                       const const_buffer &weight)
:bias
{
	master,
	{
		ircd::size(bias),           // size
		offset,                     // offset
	},
}
,weight
{
	master,
	{
		ircd::size(weight),         // size
		offset + ircd::size(bias),  // offset
	}
}
{
}
