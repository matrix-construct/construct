// Tensor Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::gpt::pipe
{
	static void profile_dumplog(pipe::exec &);

	static ircd::cl::exec::opts
	negative_opts, positive_opts, selfattn_opts,
	cathode_opts, anode_opts, lmhead_opts, lmamax_opts;

	extern conf::item<size_t> flush_cycles;
	extern conf::item<size_t> queue_cycles;
	extern const ircd::run::changed handle_quit;
}

decltype(ircd::gpt::pipe::queue_cycles)
ircd::gpt::pipe::queue_cycles
{
	{ "name",     "ircd.gpt.pipe.queue" },
	{ "default",  1L,                   },
};

decltype(ircd::gpt::pipe::flush_cycles)
ircd::gpt::pipe::flush_cycles
{
	{ "name",     "ircd.gpt.pipe.flush" },
	{ "default",  0L,                   },
};

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
ircd::gpt::generate(task &task)
{
	if(unlikely(!pipe::default_model))
		pipe::init();

	const auto &opts
	{
		*task.opts
	};

	auto &ctrl
	{
		*task.ctrl
	};

	ctrl.call = IRCD_GPT_ECOMPLETE;
	ctrl.host_tsc = prof::cycles();
	size_t cycle(ctrl.cycle);
	const size_t tokens(ctrl.tokens);

	std::deque<pipe::exec> list;
	for(; cycle < opts.limit; ++cycle)
	{
		// When the release/acquire bits are set the control pages are sent
		// and received; only set on first and last iterations of this loop.
		const bool
		rel(cycle == 0),
		acq(cycle + 1 >= opts.limit);

		// Enqueue the cycle's commands
		list.emplace_back
		(
			task, tokens + cycle, rel, acq
		);

		// Conditions for a cl::flush here; this is not default but
		// may be configured to improve some workloads.
		const bool flush
		{
			// Flushing here is enabled by the configuration
			pipe::flush_cycles

			// Skip flushing on cycles already performing IO or waiting.
			&& !rel && !acq && list.size() <= pipe::queue_cycles

			// The configuration item can specify an interval greater than
			// one between flushes.
			&& cycle % pipe::flush_cycles == 0
		};

		if(flush)
			cl::flush();

		// Enqueue consecutive repetitions of our kernel batch before waiting
		// on the first; based on the configuration. XXX get from ircd::cl
		if(list.size() <= pipe::queue_cycles)
			continue;

		// Profiling branch
		if((false))
		{
			auto &ex(list.front());
			profile_dumplog(ex);
		}

		// Destructing the front of the queue waits for completion by yielding
		// this ircd::ctx.
		list.pop_front();
	}

	// Wait for all unfinished
	list.clear();

	// Interp error codes
	if(unlikely(ctrl.call <= 0))
		throw error
		{
			"hyper (#%d) :%s",
			abs(int(ctrl.call)),
			reflect(ctrl.call),
		};

	always_assert(ctrl.cycle == cycle);
}

void
ircd::gpt::pipe::profile_dumplog(pipe::exec &exec)
{
	constexpr size_t coils
	{
		sizeof(exec.coil) / sizeof(cl::exec)
	};

	for(size_t i(0); i < coils; ++i)
	{
		exec.coil[i].wait();
		const auto &pro
		{
			exec.coil[i].profile()
		};

		char tmbuf[4][32] {{0}};
		log::logf
		{
			log, log::level::DEBUG,
			"coil:%-2lu %8s %8s %8s %8s\n",
			i,
			util::pretty(tmbuf[0], si(pro[0]), 1),
			util::pretty(tmbuf[1], si(pro[1]), 1),
			util::pretty(tmbuf[2], si(pro[2]), 1),
			util::pretty(tmbuf[3], si(pro[3]), 1),
		};
	}
}

//
// pipe::exec
//

ircd::gpt::pipe::exec::exec(task &task,
                            const size_t tokens,
                            const bool release,
                            const bool acquire)
:desc
{
	default_desc
}
,send_opts
{
	reinterpret_cast<const char *>(task.opts),
	release? sizeof(struct ircd_gpt_opts): 0
}
,send_ctrl
{
	reinterpret_cast<const char *>(task.ctrl),
	release? sizeof(struct ircd_gpt_task): 0
}
,recv_ctrl
{
	reinterpret_cast<char *>(task.ctrl),
	acquire? sizeof(struct ircd_gpt_task): 0
}
,range_lm_embed
{
	{      1 * 192UL, 0, },
	{          192UL, 0, },
}
,range_negative
{
	{ tokens * 192UL, 0, },
	{          192UL, 0, },
}
,range_positive
{
	{ tokens * 192UL, 0, },
	{          192UL, 0, },
}
,range_lm_norm
{
	{            1 * 192UL, 0 },
	{                192UL, 0 },
	{ (tokens - 1) * 192UL, 0 },
}
,range_lm_logit
{
	{ 262 * 192UL, 0 },  // align_up(50257) / 192
	{       192UL, 0 },
}
,range_lm_select
{
	{   1 * 192UL, 0 },
	{       192UL, 0 },
}
,release_opts
{
	desc->opts, send_opts
}
,release_ctrl
{
	desc->ctrl, send_ctrl
}
,lm_embed
{
	desc->lm_embed, range_lm_embed, anode_opts
}
,coil
{
	{ desc->layer[0x00]->negative, range_negative, negative_opts },
	{ desc->layer[0x00]->positive, range_positive, positive_opts },
	{ desc->layer[0x01]->negative, range_negative, negative_opts },
	{ desc->layer[0x01]->positive, range_positive, positive_opts },
	{ desc->layer[0x02]->negative, range_negative, negative_opts },
	{ desc->layer[0x02]->positive, range_positive, positive_opts },
	{ desc->layer[0x03]->negative, range_negative, negative_opts },
	{ desc->layer[0x03]->positive, range_positive, positive_opts },
	{ desc->layer[0x04]->negative, range_negative, negative_opts },
	{ desc->layer[0x04]->positive, range_positive, positive_opts },
	{ desc->layer[0x05]->negative, range_negative, negative_opts },
	{ desc->layer[0x05]->positive, range_positive, positive_opts },
	{ desc->layer[0x06]->negative, range_negative, negative_opts },
	{ desc->layer[0x06]->positive, range_positive, positive_opts },
	{ desc->layer[0x07]->negative, range_negative, negative_opts },
	{ desc->layer[0x07]->positive, range_positive, positive_opts },
	{ desc->layer[0x08]->negative, range_negative, negative_opts },
	{ desc->layer[0x08]->positive, range_positive, positive_opts },
	{ desc->layer[0x09]->negative, range_negative, negative_opts },
	{ desc->layer[0x09]->positive, range_positive, positive_opts },
	{ desc->layer[0x0a]->negative, range_negative, negative_opts },
	{ desc->layer[0x0a]->positive, range_positive, positive_opts },
	{ desc->layer[0x0b]->negative, range_negative, negative_opts },
	{ desc->layer[0x0b]->positive, range_positive, positive_opts },
}
,lm_norm
{
	desc->lm_norm, range_lm_norm, cathode_opts
}
,lm_logit
{
	desc->lm_logit, range_lm_logit, lmhead_opts
}
,lm_select
{
	desc->lm_select, range_lm_select, lmamax_opts
}
,acquire_ctrl
{
	desc->ctrl, recv_ctrl
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
,state
{
	96 * 3 * 768 * sizeof(float),
	mutable_buffer{}
}
,accum
{
	96 * 768 * sizeof(float),
	mutable_buffer{}
}
,logit
{
	65536 * sizeof(float),
	mutable_buffer{}
}
,ctrl
{
	sizeof(struct ircd_gpt_task),
	mutable_buffer{}
}
,opts
{
	sizeof(struct ircd_gpt_opts),
	const_buffer{}
}
,lm_embed
{
	code,
	"ircd_gpt_lm_embed",
	ctrl,
	opts,
	accum,
	model.embed->pos,
	model.embed->token,
}
,lm_norm
{
	code,
	"ircd_gpt_lm_norm",
	ctrl,
	opts,
	accum,
	model.decode->norm.bias,
	model.decode->norm.weight,
}
,lm_logit
{
	code,
	"ircd_gpt_lm_logit",
	ctrl,
	opts,
	logit,
	accum,
	model.embed->token,
}
,lm_select
{
	code,
	"ircd_gpt_lm_select",
	ctrl,
	opts,
	logit,
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
	"ircd_gpt_attn_fcon",
	desc.ctrl,
	desc.opts,
	desc.state,
	desc.accum,
	desc.model->decode->block[laynum].attn.norm.bias,
	desc.model->decode->block[laynum].attn.norm.weight,
	desc.model->decode->block[laynum].attn.fcon.bias,
	desc.model->decode->block[laynum].attn.fcon.weight,
}
,positive
{
	*desc.code,
	"ircd_gpt_coil",
	desc.ctrl,
	desc.opts,
	desc.accum,
	desc.state,
	desc.model->decode->block[laynum].attn.mask,
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
:master
{
	sizeof(gpt::model::block) * 12 + sizeof(gpt::model::norm), const_buffer
	{
		reinterpret_cast<const char *>(decoder.layer),
		sizeof(decoder.layer) + sizeof(decoder.f)
	}
}
,block
{
	{ master, sizeof(gpt::model::block) * 0x00, decoder.layer[0x00], 0x00, },
	{ master, sizeof(gpt::model::block) * 0x01, decoder.layer[0x01], 0x01, },
	{ master, sizeof(gpt::model::block) * 0x02, decoder.layer[0x02], 0x02, },
	{ master, sizeof(gpt::model::block) * 0x03, decoder.layer[0x03], 0x03, },
	{ master, sizeof(gpt::model::block) * 0x04, decoder.layer[0x04], 0x04, },
	{ master, sizeof(gpt::model::block) * 0x05, decoder.layer[0x05], 0x05, },
	{ master, sizeof(gpt::model::block) * 0x06, decoder.layer[0x06], 0x06, },
	{ master, sizeof(gpt::model::block) * 0x07, decoder.layer[0x07], 0x07, },
	{ master, sizeof(gpt::model::block) * 0x08, decoder.layer[0x08], 0x08, },
	{ master, sizeof(gpt::model::block) * 0x09, decoder.layer[0x09], 0x09, },
	{ master, sizeof(gpt::model::block) * 0x0a, decoder.layer[0x0a], 0x0a, },
	{ master, sizeof(gpt::model::block) * 0x0b, decoder.layer[0x0b], 0x0b, },
}
,norm
{
	master,
	off_t(sizeof(gpt::model::block) * 12),
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

ircd::gpt::pipe::model::block::block(cl::data &master,
                                     const off_t offset,
                                     const gpt::model::block &block,
                                     const size_t layer)
:attn
{
	master,
	offset,
	block.ln1,
	block.attn,
}
,ffnn
{
	master,
	offset + off_t(sizeof(block.ln1) + sizeof(block.attn)),
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

//
// gpt::task
//

ircd::gpt::task::task(const gpt::opts *const opts,
                      struct ircd_gpt_task *const ctrl)
:opts
{
	opts
}
,ctrl
{
	ctrl
}
{
	memset(this->ctrl, 0x0, sizeof(ircd_gpt_task));

    this->ctrl->rand[0] = this->opts->seed;
    this->ctrl->rand[1] = this->opts->seed;
    this->ctrl->rand[2] = -1UL;
    this->ctrl->rand[3] = -1UL;
}

ircd::gpt::task::~task()
noexcept
{
}

//
// hypercall
//

ircd::string_view
ircd::gpt::reflect(const enum ircd_gpt_hypercall code)
noexcept
{
	switch(code)
	{
		case IRCD_GPT_ACCEPT:      return "ACCEPT";
		case IRCD_GPT_ECOMPLETE:   return "ECOMPLETE";
	}

	return "??????";
}
