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

	extern conf::item<size_t> queue_cycles;
	extern const ircd::run::changed handle_quit;

	static ircd::cl::exec::opts
	send_opts_opts, send_ctrl_opts, send_coil_opts, send_head_opts,
	anode_opts, negative_opts, positive_opts, cathode_opts,
	lmhead_opts, lmamax_opts, backprop_opts, recv_ctrl_opts;
}

decltype(ircd::gpt::pipe::queue_cycles)
ircd::gpt::pipe::queue_cycles
{
	{ "name",     "ircd.gpt.pipe.queue" },
	{ "default",  1L,                   },
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
	const gpt::model::decoder &default_model
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

	//XXX
	send_ctrl_opts.flush = true;
	send_ctrl_opts.nice = 1;
	lmamax_opts.flush = true;
	lmamax_opts.nice = 2;
	recv_ctrl_opts.flush = true;

	log::debug
	{
		log, "Pipe initialized from model:%p data:%p code:%p desc:%p",
		&default_model,
		pipe::default_model,
		pipe::default_code,
		pipe::default_desc,
	};
}

void
ircd::gpt::pipe::fini()
noexcept
{
	const auto pending
	{
		cl::work::list.size()
	};

	if(pending)
		log::warning
		{
			log, "Waiting for %zu pending tasks to leave the pipe...",
			pending,
		};

	cl::sync();

	delete default_desc;   default_desc = nullptr;
	delete default_code;   default_code = nullptr;
	delete default_model;  default_model = nullptr;
}

//
// pipe
//

void
ircd::gpt::pipe::generate(task &task)
{
	assert(pipe::default_model);

	assert(task.opts);
	const auto &opts
	{
		*task.opts
	};

	assert(task.ctrl);
	auto &ctrl
	{
		*task.ctrl
	};

	ctrl.epic.cycle = 0;
	ctrl.epic.host_tsc = prof::cycles();

	const auto tokens(ctrl.tokens.count);
	const auto epoch(ctrl.epic.epoch);
	volatile auto cycle(ctrl.epic.cycle);

	std::deque<pipe::exec> list;
	for(; cycle < opts.limit; ++cycle)
	{
		// When the release/acquire bits are set the control pages are sent
		// and received; only set on first and last iterations of this loop.
		const bool
		rel(cycle == 0),
		acq(cycle + 1 >= opts.limit || ctx::interruption_requested());

		// Enqueue the cycle's commands
		list.emplace_back
		(
			task, tokens + cycle, rel, acq
		);

		if(ctx::interruption_requested())
			if(acq || termination(ctx::cur()))
				break;

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

	assert(ctrl.magic == 0xC7012C70);
	assert(ctrl.epic.cycle == cycle || ctx::interruption_requested());
	this_ctx::interruption_point();
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
			"coil:%-2lu %8s %8s %8s %8s",
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
,send_coil
{
	reinterpret_cast<const char *>(gpt::model::default_model),
	release && desc->model->invalid? (sizeof(gpt::model::block) * 12 + sizeof(gpt::model::norm)): 0
}
,send_head
{
	reinterpret_cast<const char *>(&gpt::model::default_model->word),
	release && desc->model->invalid? sizeof(gpt::model::embed): 0
}
,recv_ctrl
{
	reinterpret_cast<char *>(task.ctrl),
	acquire? sizeof(struct ircd_gpt_task): 0
}
,range_lm_embed
{
	{ std::min(tokens,  12UL) * 192UL, 0, },
	{                           192UL, 0, },
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
	{  786 * 64UL, 0 },  // align_up(50257) / 64
	{        64UL, 0 },
}
,range_lm_logsm
{
	{  1 * 256UL, 0 },
	{      256UL, 0 },
}
,range_lm_select
{
	{   1 * 256UL, 0 },
	{       256UL, 0 },
}
,release_opts
{
	desc->opts, send_opts, send_opts_opts,
}
,release_ctrl
{
	desc->ctrl, send_ctrl, send_ctrl_opts
}
,release_coil
{
	desc->model->decode->master[0], send_coil, send_coil_opts
}
,release_head
{
	desc->model->embed->master[0], send_head, send_head_opts
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
,lm_logsm
{
	desc->lm_logsm, range_lm_logsm, lmhead_opts
}
,lm_select
{
	desc->lm_select, range_lm_select, lmamax_opts
}
,acquire_ctrl
{
	desc->ctrl, recv_ctrl, recv_ctrl_opts
}
{
	if(release && desc->model->invalid)
		desc->model->invalid = false;
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
	" -cl-mad-enable"
	" -cl-single-precision-constant"
	//" -cl-fp32-correctly-rounded-divide-sqrt"

	" -cl-kernel-arg-info"
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
	512 * 3 * 768 * sizeof(float),
	mutable_buffer{}
}
,accum
{
	512 * 768 * sizeof(float),
	mutable_buffer{}
}
,logit
{
	65536 * sizeof(float),
	mutable_buffer{}
}
,logexp
{
	65536 * sizeof(float),
	mutable_buffer{}
}
,logsm
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
	model.embed->pos.param,
	model.embed->token.param,
}
,lm_norm
{
	code,
	"ircd_gpt_lm_norm",
	ctrl,
	opts,
	accum,
	model.decode->norm.bias.param,
	model.decode->norm.weight.param,
}
,lm_logit
{
	code,
	"ircd_gpt_lm_logit",
	ctrl,
	opts,
	logit,
	accum,
	model.embed->token.param,
}
,lm_logsm
{
	code,
	"ircd_gpt_lm_logsm",
	ctrl,
	opts,
	logsm,
	logexp,
	logit,
}
,lm_select
{
	code,
	"ircd_gpt_lm_select",
	ctrl,
	opts,
	logsm,
	logexp,
	logit,
}
,lm_norm_backprop
{
	code,
	"ircd_gpt_norm_prop",
	ctrl,
	opts,
	model.decode->norm.bias.param,
	model.decode->norm.bias.moment[0],
	model.decode->norm.bias.moment[1],
	model.decode->norm.weight.param,
	model.decode->norm.weight.moment[0],
	model.decode->norm.weight.moment[1],
}
,lm_embed_backprop
{
	code,
	"ircd_gpt_lm_embed_prop",
	ctrl,
	opts,
	model.embed->pos.param,
	model.embed->pos.moment[0],
	model.embed->pos.moment[1],
	model.embed->token.param,
	model.embed->token.moment[0],
	model.embed->token.moment[1],
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
	desc.model->decode->block[laynum].attn.norm.bias.param,
	desc.model->decode->block[laynum].attn.norm.weight.param,
	desc.model->decode->block[laynum].attn.fcon.bias.param,
	desc.model->decode->block[laynum].attn.fcon.weight.param,
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
	desc.model->decode->block[laynum].attn.proj.bias.param,
	desc.model->decode->block[laynum].attn.proj.weight.param,
	desc.model->decode->block[laynum].ffnn.norm.bias.param,
	desc.model->decode->block[laynum].ffnn.norm.weight.param,
	desc.model->decode->block[laynum].ffnn.fcon.bias.param,
	desc.model->decode->block[laynum].ffnn.fcon.weight.param,
	desc.model->decode->block[laynum].ffnn.proj.bias.param,
	desc.model->decode->block[laynum].ffnn.proj.weight.param,
}
,backattn
{
	*desc.code,
	"ircd_gpt_coil_prop_attn",
	desc.ctrl,
	desc.opts,
	desc.model->decode->block[laynum].attn.norm.bias.param,
	desc.model->decode->block[laynum].attn.norm.bias.moment[0],
	desc.model->decode->block[laynum].attn.norm.bias.moment[1],
	desc.model->decode->block[laynum].attn.norm.weight.param,
	desc.model->decode->block[laynum].attn.norm.weight.moment[0],
	desc.model->decode->block[laynum].attn.norm.weight.moment[1],
	desc.model->decode->block[laynum].attn.fcon.bias.param,
	desc.model->decode->block[laynum].attn.fcon.bias.moment[0],
	desc.model->decode->block[laynum].attn.fcon.bias.moment[1],
	desc.model->decode->block[laynum].attn.fcon.weight.param,
	desc.model->decode->block[laynum].attn.fcon.weight.moment[0],
	desc.model->decode->block[laynum].attn.fcon.weight.moment[1],
	desc.model->decode->block[laynum].attn.proj.bias.param,
	desc.model->decode->block[laynum].attn.proj.bias.moment[0],
	desc.model->decode->block[laynum].attn.proj.bias.moment[1],
	desc.model->decode->block[laynum].attn.proj.weight.param,
	desc.model->decode->block[laynum].attn.proj.weight.moment[0],
	desc.model->decode->block[laynum].attn.proj.weight.moment[1],
}
,backffnn
{
	*desc.code,
	"ircd_gpt_coil_prop_ffnn",
	desc.ctrl,
	desc.opts,
	desc.model->decode->block[laynum].ffnn.norm.bias.param,
	desc.model->decode->block[laynum].ffnn.norm.bias.moment[0],
	desc.model->decode->block[laynum].ffnn.norm.bias.moment[1],
	desc.model->decode->block[laynum].ffnn.norm.weight.param,
	desc.model->decode->block[laynum].ffnn.norm.weight.moment[0],
	desc.model->decode->block[laynum].ffnn.norm.weight.moment[1],
	desc.model->decode->block[laynum].ffnn.fcon.bias.param,
	desc.model->decode->block[laynum].ffnn.fcon.bias.moment[0],
	desc.model->decode->block[laynum].ffnn.fcon.bias.moment[1],
	desc.model->decode->block[laynum].ffnn.fcon.weight.param,
	desc.model->decode->block[laynum].ffnn.fcon.weight.moment[0],
	desc.model->decode->block[laynum].ffnn.fcon.weight.moment[1],
	desc.model->decode->block[laynum].ffnn.proj.bias.param,
	desc.model->decode->block[laynum].ffnn.proj.bias.moment[0],
	desc.model->decode->block[laynum].ffnn.proj.bias.moment[1],
	desc.model->decode->block[laynum].ffnn.proj.weight.param,
	desc.model->decode->block[laynum].ffnn.proj.weight.moment[0],
	desc.model->decode->block[laynum].ffnn.proj.weight.moment[1],
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

ircd::gpt::pipe::model::model(gpt::model::decoder &decoder,
                              gpt::model::embed &embed)
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
// pipe::model::decoder
//

ircd::gpt::pipe::model::decoder::decoder(gpt::model::decoder &decoder)
:master
{
	// params
	{
		sizeof(gpt::model::block) * 12 + sizeof(gpt::model::norm), mutable_buffer
		{
			reinterpret_cast<char *>(decoder.layer),
			sizeof(decoder.layer) + sizeof(decoder.f)
		}
	},

	// first moment
	{
		sizeof(gpt::model::block) * 12 + sizeof(gpt::model::norm),
		mutable_buffer{}
	},

	// second moment
	{
		sizeof(gpt::model::block) * 12 + sizeof(gpt::model::norm),
		mutable_buffer{}
	},
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
	mutable_buffer{decoder.f.bias},
	mutable_buffer{decoder.f.weight},
}
{
}

ircd::gpt::pipe::model::decoder::decoder(const gpt::model::decoder &decoder)
:master
{
	// params
	{
		sizeof(gpt::model::block) * 12 + sizeof(gpt::model::norm), const_buffer
		{
			reinterpret_cast<const char *>(decoder.layer),
			sizeof(decoder.layer) + sizeof(decoder.f)
		}
	},
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
// pipe::model::language
//

ircd::gpt::pipe::model::language::language(gpt::model::embed &embed)
:master
{
	// params
	{
		sizeof(embed), mutable_buffer
		{
			reinterpret_cast<char *>(&embed),
			sizeof(embed),
		}
	},

	// first moment
	{
		sizeof(embed), mutable_buffer{},
	},

	// second moment
	{
		sizeof(embed), mutable_buffer{},
	},
}
,pos
{
	master, 0, mutable_buffer{embed.pos}
}
,token
{
	master, sizeof(embed.pos), mutable_buffer{embed.token}
}
{
}

ircd::gpt::pipe::model::language::language(const gpt::model::embed &embed)
:master
{
	{
		sizeof(embed), const_buffer
		{
			reinterpret_cast<const char *>(&embed),
			sizeof(embed),
		}
	},
}
,pos
{
	master, 0, const_buffer{embed.pos}
}
,token
{
	master, sizeof(embed.pos), const_buffer{embed.token}
}
{
}

ircd::gpt::pipe::model::language::language(cl::data *const master,
                                           const off_t offset,
                                           gpt::model::embed &embed)
:pos
{
	master, offset, mutable_buffer{embed.pos}
}
,token
{
	master, offset + off_t(sizeof(embed.pos)), mutable_buffer{embed.token}
}
{
}

ircd::gpt::pipe::model::language::language(cl::data *const master,
                                           const off_t offset,
                                           const gpt::model::embed &embed)
:pos
{
	master, offset, const_buffer{embed.pos}
}
,token
{
	master, offset + off_t(sizeof(embed.pos)), const_buffer{embed.token}
}
{
}

ircd::gpt::pipe::model::language::~language()
noexcept
{
}

//
// pipe::model::block
//

ircd::gpt::pipe::model::block::block(gpt::model::block &block,
                                     const size_t layer)
:master
{
	// params
	{
		sizeof(block), mutable_buffer
		{
			reinterpret_cast<char *>(&block), sizeof(block)
		}
	},

	// first moment
	{
		sizeof(block),
		mutable_buffer{}
	},

	// second moment
	{
		sizeof(block),
		mutable_buffer{}
	},
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
	off_t(sizeof(block.ln1) + sizeof(block.attn)),
	block.ln2,
	block.ffnn,
}
{
}

ircd::gpt::pipe::model::block::block(const gpt::model::block &block,
                                     const size_t layer)
:master
{
	// params
	{
		sizeof(block), const_buffer
		{
			reinterpret_cast<const char *>(&block), sizeof(block)
		}
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
	off_t(sizeof(block.ln1) + sizeof(block.attn)),
	block.ln2,
	block.ffnn,
}
{
}

ircd::gpt::pipe::model::block::block(cl::data *const master,
                                     const off_t offset,
                                     gpt::model::block &block,
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

ircd::gpt::pipe::model::block::block(cl::data *const master,
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

ircd::gpt::pipe::model::ffnn::ffnn(cl::data *const master,
                                   const off_t offset,
                                   gpt::model::norm &norm,
                                   gpt::model::ffnn &ffnn)
:norm
{
	master,
	offset,
	mutable_buffer{norm.bias},
	mutable_buffer{norm.weight},
}
,fcon
{
	master,
	offset + off_t(sizeof(norm)),
	mutable_buffer{ffnn.fc_bias},
	mutable_buffer{ffnn.fc_weight},
}
,proj
{
	master,
	offset + off_t(sizeof(norm) + sizeof(ffnn.fc_bias) + sizeof(ffnn.fc_weight)),
	mutable_buffer{ffnn.proj_bias},
	mutable_buffer{ffnn.proj_weight},
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

ircd::gpt::pipe::model::ffnn::ffnn(cl::data *const master,
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

ircd::gpt::pipe::model::attn::attn(cl::data *const master,
                                   const off_t offset,
                                   gpt::model::norm &norm,
                                   gpt::model::attn &attn)
:norm
{
	master,
	offset,
	mutable_buffer{norm.bias},
	mutable_buffer{norm.weight},
}
,fcon
{
	master,
	offset + off_t(sizeof(norm)),
	mutable_buffer{attn.attn_bias},
	mutable_buffer{attn.attn_weight},
}
,proj
{
	master,
	offset + off_t(sizeof(norm) + sizeof(attn.attn_bias) + sizeof(attn.attn_weight) + sizeof(attn.bias)),
	mutable_buffer{attn.proj_bias},
	mutable_buffer{attn.proj_weight},
}
,mask
{
	master[0],
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

ircd::gpt::pipe::model::attn::attn(cl::data *const master,
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
	master[0],
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

ircd::gpt::pipe::model::tensor::tensor(cl::data *const master,
                                       const off_t offset,
                                       const mutable_buffer &bias,
                                       const mutable_buffer &weight)
:bias
{
	master,
	offset,
	bias,
}
,weight
{
	master,
	off_t(offset + ircd::size(bias)),
	weight,
}
{
}

ircd::gpt::pipe::model::tensor::tensor(cl::data *const master,
                                       const off_t offset,
                                       const const_buffer &bias,
                                       const const_buffer &weight)
:bias
{
	master,
	offset,
	bias,
}
,weight
{
	master,
	off_t(offset + ircd::size(bias)),
	weight,
}
{
}

//
// pipe::model::matrix
//

ircd::gpt::pipe::model::matrix::matrix(cl::data *const master,
                                       const off_t offset,
                                       const mutable_buffer &param)
:param
{
	master[0],
	{
		ircd::size(param),
		offset,
	},
}
,moment
{
	// first moment
	{
		master[1],
		{
			ircd::size(param),
			offset,
		},
	},

	// second moment
	{
		master[2],
		{
			ircd::size(param),
			offset,
		},
	},
}
{
}

ircd::gpt::pipe::model::matrix::matrix(cl::data *const master,
                                       const off_t offset,
                                       const const_buffer &param)
:param
{
	master[0],
	{
		ircd::size(param),          // size
		offset,                     // offset
	},
}
{
}
