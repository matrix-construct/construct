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
	static void handle_quit() noexcept;

	extern const ircd::run::changed quit_handler;
}

decltype(ircd::gpt::pipe::default_code)
ircd::gpt::pipe::default_code;

[[gnu::visibility("hidden")]]
decltype(ircd::gpt::pipe::quit_handler)
ircd::gpt::pipe::quit_handler
{
	run::level::QUIT, handle_quit
};

[[gnu::cold]]
void
ircd::gpt::pipe::handle_quit()
noexcept
{
	if constexpr(!IRCD_USE_OPENCL)
		return;

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
	ctx::yield();
	pipe::default_code.reset();
}

//
// pipe::prof
//

ircd::string_view
ircd::gpt::pipe::debug(const mutable_buffer &buf,
                       const prof &p)
{
	window_buffer window(buf);
	for(uint i(0); i < p.stages; ++i)
		window([&p, &i](auto buf)
		{
			size_t ret(0);
			ret += consume(buf, size(debug(buf, p, i)));
			ret += consume(buf, copy(buf, '\n'));
			return ret;
		});

	return window.completed();
}

ircd::string_view
ircd::gpt::pipe::debug(const mutable_buffer &buf,
                       const prof &p,
                       const size_t &i)
{
	using phase = prof::phase;

	assert(i < p.info.size());
	assert(i < p.ts.size());

	char tbuf[5][32];
	return fmt::sprintf
	{
		buf, "%-20s %04x [ %10s %10s %10s %10s %10s ]",
		std::get<0>(p.info[i]),
		std::get<1>(p.info[i]),
		pretty(tbuf[0], p.ts[i][phase::QUEUE], 1),
		pretty(tbuf[1], p.ts[i][phase::SUBMIT], 1),
		pretty(tbuf[2], p.ts[i][phase::START], 1),
		pretty(tbuf[3], p.ts[i][phase::END], 1),
		pretty(tbuf[4], p.ts[i][phase::COMPLETE], 1),
	};
}

//
// prof::prof
//

decltype(ircd::gpt::pipe::prof::info)
ircd::gpt::pipe::prof::info;

decltype(ircd::gpt::pipe::prof::name)
ircd::gpt::pipe::prof::name;

[[gnu::visibility("hidden")]]
decltype(ircd::gpt::pipe::prof::init)
ircd::gpt::pipe::prof::init;

ircd::gpt::pipe::prof::prof()
noexcept
{
	for(uint i(0); i < stages; ++i)
		for(uint j(0); j < phases; ++j)
			ts[i][j] = 0ns;
}

ircd::gpt::pipe::prof::prof(const cycle &c)
{
	if(!std::exchange(init, true))
		init_info(c);

	if(!cl::profile_queue)
		return;

	for(uint i(0); i < stages; ++i)
	{
		const cl::work::prof p
		{
			c.stage[i]
		};

		ts[i][phase::QUEUE] = p[phase::SUBMIT] > p[phase::QUEUE]?
			p[phase::SUBMIT] - p[phase::QUEUE]: 0ns;

		ts[i][phase::SUBMIT] = p[phase::START] > p[phase::SUBMIT]?
			p[phase::START] - p[phase::SUBMIT]: 0ns;

		ts[i][phase::START] = p[phase::END] > p[phase::START]?
			p[phase::END] - p[phase::START]: 0ns;

		ts[i][phase::END] = p[phase::END] > p[phase::QUEUE]?
			p[phase::END] - p[phase::QUEUE]: 0ns;

		ts[i][phase::COMPLETE] = p[phase::COMPLETE] > p[phase::QUEUE]?
			p[phase::COMPLETE] - p[phase::QUEUE]: 0ns;
	}
}

[[gnu::visibility("hidden")]]
void
ircd::gpt::pipe::prof::init_info(const cycle &c)
{
	static_assert
	(
		name.size() >= stages
	);

	for(uint i(0); i < stages; ++i)
		info[i] = info_type
		{
			c.stage[i].name(name[i]),
			c.stage[i].type(),
		};
}

///////////////////////////////////////////////////////////////////////////////
//
// pipe::cycle
//

const ircd::gpt::ctrl &
ircd::gpt::pipe::acquire(cycle &cycle)
{
	// Some tail stages may not be active each cycle
	const auto last_exec
	{
		std::find_if(std::rbegin(cycle.stage), std::rend(cycle.stage), []
		(const auto &work)
		{
			return work.handle;
		})
	};
	assert(last_exec != std::rend(cycle.stage));

	// Block here for results; the ircd::ctx will yield.
	last_exec->wait();

	// Get the pointer to the output buffer.
	const auto ctrl
	{
		reinterpret_cast<const gpt::ctrl *>(cycle.desc.frame[cycle.frame].ptr())
	};

	// Check the output is a valid control page and return to user.
	assert(ctrl);
	assert(ctrl->magic != 0xDEADBEEF);
	assert(ctrl->magic == 0xC7012C70UL);
	return *ctrl;
}

//
// pipe::cycle::cycle
//

ircd::gpt::pipe::cycle::cycle(gpt::samp &samp)
:desc
{
	samp.desc
}
,tick
{
	samp.cycle
}
,count
{
	samp.count
}
,tokens
{
	samp.tokens
}
,cached
{
	desc.cached
}
,frame
{
	tick % samp.opts.frames
}
,range
{
	samp.opts,
	tick,
	count,
	tokens,
	cached,
	true,
	((false) && gpt::model::cache_shared)
}
,stage
{
	cl::exec // data
	{
		desc.opts, std::memory_order_release
	},
	cl::exec // data
	{
		desc.ctrl, std::memory_order_release
	},
	cl::exec // data
	{
		desc.frame[frame], std::memory_order_release
	},
	cl::exec // data
	{
		desc.model->decode->master[0], std::memory_order_release
	},
	cl::exec // Initial kernel
	{
		desc.alloc, range.alloc,
	},
	cl::exec // Initial cycle kernel
	{
		desc.enter, range.select,
	},
	cl::exec // Compute token and positional embeddings.
	{
		desc.lm_embed, range.embed,
	},
	// Forward Pass
	cl::exec { desc.layer[0x00]->attn, range.attn },
	cl::exec { desc.layer[0x00]->ffnn, range.ffnn },
	cl::exec { desc.layer[0x01]->attn, range.attn },
	cl::exec { desc.layer[0x01]->ffnn, range.ffnn },
	cl::exec { desc.layer[0x02]->attn, range.attn },
	cl::exec { desc.layer[0x02]->ffnn, range.ffnn },
	cl::exec { desc.layer[0x03]->attn, range.attn },
	cl::exec { desc.layer[0x03]->ffnn, range.ffnn },
	cl::exec { desc.layer[0x04]->attn, range.attn },
	cl::exec { desc.layer[0x04]->ffnn, range.ffnn },
	cl::exec { desc.layer[0x05]->attn, range.attn },
	cl::exec { desc.layer[0x05]->ffnn, range.ffnn },
	cl::exec { desc.layer[0x06]->attn, range.attn },
	cl::exec { desc.layer[0x06]->ffnn, range.ffnn },
	cl::exec { desc.layer[0x07]->attn, range.attn },
	cl::exec { desc.layer[0x07]->ffnn, range.ffnn },
	cl::exec { desc.layer[0x08]->attn, range.attn },
	cl::exec { desc.layer[0x08]->ffnn, range.ffnn },
	cl::exec { desc.layer[0x09]->attn, range.attn },
	cl::exec { desc.layer[0x09]->ffnn, range.ffnn },
	cl::exec { desc.layer[0x0a]->attn, range.attn },
	cl::exec { desc.layer[0x0a]->ffnn, range.ffnn },
	cl::exec { desc.layer[0x0b]->attn, range.attn },
	cl::exec { desc.layer[0x0b]->ffnn, range.fffnn },
	cl::exec // Final normalization.
	{
		desc.lm_norm, range.fnorm
	},
	cl::exec // Compute language logits.
	{
		desc.lm_logit, range.logit
	},
	cl::exec // Statistics on the logits.
	{
		desc.lm_logsm, range.logsm
	},
	cl::exec // Select next token.
	{
		desc.lm_select, range.select
	},
	cl::exec // Backpropagate
	{
		desc.lm_prop_embed, range.prop_embed
	},
	cl::exec // Backpropagate
	{
		desc.lm_prop_norm, range.prop_norm
	},
	// Backward Pass
	cl::exec { desc.layer[0x0b]->prop_ffnn, range.prop_ffnn },
	cl::exec { desc.layer[0x0b]->prop_attn, range.prop_attn },
	cl::exec { desc.layer[0x0a]->prop_ffnn, range.prop_ffnn },
	cl::exec { desc.layer[0x0a]->prop_attn, range.prop_attn },
	cl::exec { desc.layer[0x09]->prop_ffnn, range.prop_ffnn },
	cl::exec { desc.layer[0x09]->prop_attn, range.prop_attn },
	cl::exec { desc.layer[0x08]->prop_ffnn, range.prop_ffnn },
	cl::exec { desc.layer[0x08]->prop_attn, range.prop_attn },
	cl::exec { desc.layer[0x07]->prop_ffnn, range.prop_ffnn },
	cl::exec { desc.layer[0x07]->prop_attn, range.prop_attn },
	cl::exec { desc.layer[0x06]->prop_ffnn, range.prop_ffnn },
	cl::exec { desc.layer[0x06]->prop_attn, range.prop_attn },
	cl::exec { desc.layer[0x05]->prop_ffnn, range.prop_ffnn },
	cl::exec { desc.layer[0x05]->prop_attn, range.prop_attn },
	cl::exec { desc.layer[0x04]->prop_ffnn, range.prop_ffnn },
	cl::exec { desc.layer[0x04]->prop_attn, range.prop_attn },
	cl::exec { desc.layer[0x03]->prop_ffnn, range.prop_ffnn },
	cl::exec { desc.layer[0x03]->prop_attn, range.prop_attn },
	cl::exec { desc.layer[0x02]->prop_ffnn, range.prop_ffnn },
	cl::exec { desc.layer[0x02]->prop_attn, range.prop_attn },
	cl::exec { desc.layer[0x01]->prop_ffnn, range.prop_ffnn },
	cl::exec { desc.layer[0x01]->prop_attn, range.prop_attn },
	cl::exec { desc.layer[0x00]->prop_ffnn, range.prop_ffnn },
	cl::exec { desc.layer[0x00]->prop_attn, range.prop_attn },
	cl::exec // Final kernel
	{
		desc.leave[frame], range.select
	},
	cl::exec // Frame out
	{
		desc.frame[frame], std::memory_order_consume
	},
}
{
}

ircd::gpt::pipe::cycle::~cycle()
noexcept
{
}

//////////////////////////////////////////////////////////////////////////////
//
// pipe::range
//

ircd::gpt::pipe::range::range(const opts &opts,
                              const uint tick,
                              const uint count,
                              const uint tokens,
                              const uint cached,
                              const bool fwd,
                              const bool rev)
noexcept
:_full
{
	{ opts.embed_width * (tokens - cached)  },
	{ opts.embed_width                      },
	{ opts.embed_width * cached             },
}
,_last
{
	{ opts.embed_width * 1            },
	{ opts.embed_width                },
	{ opts.embed_width * (count - 1)  },
}
,alloc
{
	{ opts.embed_width * (tick == 0)  },
	{ opts.embed_width                },
}
,embed
{
	fwd?
		_full:
		cl::kern::range{},
}
,attn
{
	fwd?
		_full:
		cl::kern::range{},
}
,ffnn
{
	fwd?
		_full:
		cl::kern::range{},
}
,fffnn
{
	fwd && tokens > count?
		_full:
	fwd?
		_last:
		cl::kern::range{},
}
,fnorm
{
	fwd?
		_last:
		cl::kern::range{},
}
,logit
{
	{ pad_to(opts.logits, 64L) * int(fwd)  },
	{ 64L                                  },
}
,logsm
{
	{ 256UL * int(fwd)  },
	{ 256UL             },
}
,select
{
	{ 256UL * int(fwd)  },
	{ 256UL             },
}
,prop_embed
{
	{ opts.embed_width * int(rev)  },
	{ opts.embed_width             },
}
,prop_norm
{
	{ opts.embed_width * int(rev)  },
	{ opts.embed_width             },
}
,prop_attn
{
	{ opts.embed_width * int(rev)  },
	{ opts.embed_width             },
}
,prop_ffnn
{
	{ opts.embed_width * int(rev)  },
	{ opts.embed_width             },
}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// pipe::desc
//

ircd::gpt::pipe::desc::desc(const gpt::opts *const &opt,
                            gpt::ctrl *const &ctrl_,
                            pipe::model &model,
                            pipe::code &code)
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
	const_buffer
	{
		reinterpret_cast<const char *>(opt),
		sizeof(gpt::opts)
	},
}
,ctrl
{
	mutable_buffer
	{
		reinterpret_cast<char *>(ctrl_),
		sizeof(gpt::ctrl)
	},
}
,master
{
	0
	+ opt->layers * opt->context_tokens * opt->attn_elems * sizeof(float)
	+ opt->context_tokens * opt->embed_elems * sizeof(float)
	+ 65536 * sizeof(float)
	+ opt->layers * opt->attn_self_elems * sizeof(float)
}
,state
{
	master,
	{
		opt->layers * opt->context_tokens * opt->attn_elems * sizeof(float),
		off_t(0),
	}
}
,accum
{
	master,
	{
		opt->context_tokens * opt->embed_elems * sizeof(float),
		state.offset() + off_t(state.size()),
	},
}
,logit
{
	master,
	{
		65536 * sizeof(float),
		accum.offset() + off_t(accum.size()),
	},
}
,attns
{
	master,
	{
		opt->layers * opt->attn_self_elems * sizeof(float),
		logit.offset() + off_t(logit.size())
	}
}
,frame
{
	//             size, read, write, }, // idx
	{ sizeof(gpt::ctrl), true, false, }, // 0
	{ sizeof(gpt::ctrl), true, false, }, // 1
	{ sizeof(gpt::ctrl), true, false, }, // 2
	{ sizeof(gpt::ctrl), true, false, }, // 3
	{ sizeof(gpt::ctrl), true, false, }, // 4
	{ sizeof(gpt::ctrl), true, false, }, // 5
	{ sizeof(gpt::ctrl), true, false, }, // 6
	{ sizeof(gpt::ctrl), true, false, }, // 7
}
,alloc
{
	code,
	"ircd_gpt_alloc",
	model.decode->master[0],
	master,
	opts,
	ctrl,
	frame[0],
	frame[1],
	frame[2],
	frame[3],
	frame[4],
	frame[5],
	frame[6],
	frame[7],
}
,enter
{
	code,
	"ircd_gpt_enter",
	model.decode->master[0],
	state,
	master,
	opts,
	ctrl,
}
,lm_embed
{
	code,
	"ircd_gpt_lm_embed",
	ctrl,
	opts,
	accum,
	model.decode->embed.pos.param,
	model.decode->embed.token.param,
}
,lm_norm
{
	code,
	"ircd_gpt_lm_norm",
	ctrl,
	opts,
	accum,
	model.decode->embed.norm.bias.param,
	model.decode->embed.norm.weight.param,
}
,lm_logit
{
	code,
	"ircd_gpt_lm_logit",
	ctrl,
	opts,
	logit,
	accum,
	model.decode->embed.pos.param,
	model.decode->embed.token.param,
}
,lm_logsm
{
	code,
	"ircd_gpt_lm_logsm",
	ctrl,
	opts,
	logit,
}
,lm_select
{
	code,
	"ircd_gpt_lm_select",
	ctrl,
	opts,
	logit,
	attns,
}
,lm_prop_embed
{
	code,
	"ircd_gpt_lm_embed_prop",
	ctrl,
	opts,
	model.decode->embed.pos.param,
	model.decode->embed.pos.moment[0],
	model.decode->embed.pos.moment[1],
	model.decode->embed.token.param,
	model.decode->embed.token.moment[0],
	model.decode->embed.token.moment[1],
}
,lm_prop_norm
{
	code,
	"ircd_gpt_norm_prop",
	ctrl,
	opts,
	model.decode->embed.norm.bias.param,
	model.decode->embed.norm.bias.moment[0],
	model.decode->embed.norm.bias.moment[1],
	model.decode->embed.norm.weight.param,
	model.decode->embed.norm.weight.moment[0],
	model.decode->embed.norm.weight.moment[1],
}
,leave
{
	{
		code,
		"ircd_gpt_leave",
		model.decode->master[0],
		state,
		master,
		opts,
		ctrl,
		frame[0],
	},
	{
		code,
		"ircd_gpt_leave",
		model.decode->master[0],
		state,
		master,
		opts,
		ctrl,
		frame[1],
	},
	{
		code,
		"ircd_gpt_leave",
		model.decode->master[0],
		state,
		master,
		opts,
		ctrl,
		frame[2],
	},
	{
		code,
		"ircd_gpt_leave",
		model.decode->master[0],
		state,
		master,
		opts,
		ctrl,
		frame[3],
	},
	{
		code,
		"ircd_gpt_leave",
		model.decode->master[0],
		state,
		master,
		opts,
		ctrl,
		frame[4],
	},
	{
		code,
		"ircd_gpt_leave",
		model.decode->master[0],
		state,
		master,
		opts,
		ctrl,
		frame[5],
	},
	{
		code,
		"ircd_gpt_leave",
		model.decode->master[0],
		state,
		master,
		opts,
		ctrl,
		frame[6],
	},
	{
		code,
		"ircd_gpt_leave",
		model.decode->master[0],
		state,
		master,
		opts,
		ctrl,
		frame[7],
	},
}
,layer
{
	std::make_unique<struct desc::layer>(*this, opt, 0x00),
	std::make_unique<struct desc::layer>(*this, opt, 0x01),
	std::make_unique<struct desc::layer>(*this, opt, 0x02),
	std::make_unique<struct desc::layer>(*this, opt, 0x03),
	std::make_unique<struct desc::layer>(*this, opt, 0x04),
	std::make_unique<struct desc::layer>(*this, opt, 0x05),
	std::make_unique<struct desc::layer>(*this, opt, 0x06),
	std::make_unique<struct desc::layer>(*this, opt, 0x07),
	std::make_unique<struct desc::layer>(*this, opt, 0x08),
	std::make_unique<struct desc::layer>(*this, opt, 0x09),
	std::make_unique<struct desc::layer>(*this, opt, 0x0a),
	std::make_unique<struct desc::layer>(*this, opt, 0x0b),
}
{
}

//
// pipe::desc::layer
//

ircd::gpt::pipe::desc::layer::layer(pipe::desc &desc,
                                    const gpt::opts *const &opts,
                                    const uint laynum)
:state
{
	desc.state,
	{
		opts->context_tokens * opts->attn_elems * sizeof(float),
		laynum * opts->context_tokens * opts->attn_elems * sizeof(float),
	}
}
,attns
{
	desc.attns,
	{
		opts->attn_self_elems * sizeof(float),
		laynum * opts->attn_self_elems * sizeof(float),
	}
}
,attn
{
	*desc.code,
	"ircd_gpt_attn_fcon",
	desc.ctrl,
	desc.opts,
	laynum,
	state,
	desc.accum,
	desc.model->decode->layer[laynum].attn.norm.bias.param,
	desc.model->decode->layer[laynum].attn.norm.weight.param,
	desc.model->decode->layer[laynum].attn.fcon.bias.param,
	desc.model->decode->layer[laynum].attn.fcon.weight.param,
}
,ffnn
{
	*desc.code,
	"ircd_gpt_coil",
	desc.ctrl,
	desc.opts,
	laynum,
	desc.accum,
	attns,
	state,
	desc.model->decode->layer[laynum].attn.proj.bias.param,
	desc.model->decode->layer[laynum].attn.proj.weight.param,
	desc.model->decode->layer[laynum].ffnn.norm.bias.param,
	desc.model->decode->layer[laynum].ffnn.norm.weight.param,
	desc.model->decode->layer[laynum].ffnn.fcon.bias.param,
	desc.model->decode->layer[laynum].ffnn.fcon.weight.param,
	desc.model->decode->layer[laynum].ffnn.proj.bias.param,
	desc.model->decode->layer[laynum].ffnn.proj.weight.param,
}
,prop_attn
{
	*desc.code,
	"ircd_gpt_coil_prop_attn",
	desc.ctrl,
	desc.opts,
	desc.model->decode->layer[laynum].attn.norm.bias.param,
	desc.model->decode->layer[laynum].attn.norm.bias.moment[0],
	desc.model->decode->layer[laynum].attn.norm.bias.moment[1],
	desc.model->decode->layer[laynum].attn.norm.weight.param,
	desc.model->decode->layer[laynum].attn.norm.weight.moment[0],
	desc.model->decode->layer[laynum].attn.norm.weight.moment[1],
	desc.model->decode->layer[laynum].attn.fcon.bias.param,
	desc.model->decode->layer[laynum].attn.fcon.bias.moment[0],
	desc.model->decode->layer[laynum].attn.fcon.bias.moment[1],
	desc.model->decode->layer[laynum].attn.fcon.weight.param,
	desc.model->decode->layer[laynum].attn.fcon.weight.moment[0],
	desc.model->decode->layer[laynum].attn.fcon.weight.moment[1],
	desc.model->decode->layer[laynum].attn.proj.bias.param,
	desc.model->decode->layer[laynum].attn.proj.bias.moment[0],
	desc.model->decode->layer[laynum].attn.proj.bias.moment[1],
	desc.model->decode->layer[laynum].attn.proj.weight.param,
	desc.model->decode->layer[laynum].attn.proj.weight.moment[0],
	desc.model->decode->layer[laynum].attn.proj.weight.moment[1],
}
,prop_ffnn
{
	*desc.code,
	"ircd_gpt_coil_prop_ffnn",
	desc.ctrl,
	desc.opts,
	desc.model->decode->layer[laynum].ffnn.norm.bias.param,
	desc.model->decode->layer[laynum].ffnn.norm.bias.moment[0],
	desc.model->decode->layer[laynum].ffnn.norm.bias.moment[1],
	desc.model->decode->layer[laynum].ffnn.norm.weight.param,
	desc.model->decode->layer[laynum].ffnn.norm.weight.moment[0],
	desc.model->decode->layer[laynum].ffnn.norm.weight.moment[1],
	desc.model->decode->layer[laynum].ffnn.fcon.bias.param,
	desc.model->decode->layer[laynum].ffnn.fcon.bias.moment[0],
	desc.model->decode->layer[laynum].ffnn.fcon.bias.moment[1],
	desc.model->decode->layer[laynum].ffnn.fcon.weight.param,
	desc.model->decode->layer[laynum].ffnn.fcon.weight.moment[0],
	desc.model->decode->layer[laynum].ffnn.fcon.weight.moment[1],
	desc.model->decode->layer[laynum].ffnn.proj.bias.param,
	desc.model->decode->layer[laynum].ffnn.proj.bias.moment[0],
	desc.model->decode->layer[laynum].ffnn.proj.bias.moment[1],
	desc.model->decode->layer[laynum].ffnn.proj.weight.param,
	desc.model->decode->layer[laynum].ffnn.proj.weight.moment[0],
	desc.model->decode->layer[laynum].ffnn.proj.weight.moment[1],
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

ircd::gpt::pipe::model::model(gpt::model::decoder &decoder)
:decode_const
{
	std::addressof(decoder)
}
,decode_mutable
{
	std::addressof(decoder)
}
,decode
{
	std::make_unique<model::decoder>(decoder)
}
{
}

ircd::gpt::pipe::model::model(const gpt::model::decoder &decoder)
:decode_const
{
	std::addressof(decoder)
}
,decode
{
	std::make_unique<model::decoder>(decoder)
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
		mutable_buffer
		{
			reinterpret_cast<char *>(&decoder) + sizeof(gpt::model::decoder) * 0,
			sizeof(gpt::model::decoder)
		}
	},
	// first moment
	{
		mutable_buffer
		{
			reinterpret_cast<char *>(&decoder) + sizeof(gpt::model::decoder) * 1,
			sizeof(gpt::model::decoder)
		}
	},
	// second moment
	{
		mutable_buffer
		{
			reinterpret_cast<char *>(&decoder) + sizeof(gpt::model::decoder) * 2,
			sizeof(gpt::model::decoder)
		}
	},
}
,layer
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
,embed
{
	master,
	off_t(offsetof(gpt::model::decoder, embed)),
	decoder.embed,
}
{
}

ircd::gpt::pipe::model::decoder::decoder(const gpt::model::decoder &decoder)
:master
{
	// params
	{
		const_buffer
		{
			reinterpret_cast<const char *>(&decoder),
			sizeof(gpt::model::decoder)
		}
	},
}
,layer
{
	{ master, off_t(offsetof(gpt::model::decoder, layer[0x00])), decoder.layer[0x00], 0x00, },
	{ master, off_t(offsetof(gpt::model::decoder, layer[0x01])), decoder.layer[0x01], 0x01, },
	{ master, off_t(offsetof(gpt::model::decoder, layer[0x02])), decoder.layer[0x02], 0x02, },
	{ master, off_t(offsetof(gpt::model::decoder, layer[0x03])), decoder.layer[0x03], 0x03, },
	{ master, off_t(offsetof(gpt::model::decoder, layer[0x04])), decoder.layer[0x04], 0x04, },
	{ master, off_t(offsetof(gpt::model::decoder, layer[0x05])), decoder.layer[0x05], 0x05, },
	{ master, off_t(offsetof(gpt::model::decoder, layer[0x06])), decoder.layer[0x06], 0x06, },
	{ master, off_t(offsetof(gpt::model::decoder, layer[0x07])), decoder.layer[0x07], 0x07, },
	{ master, off_t(offsetof(gpt::model::decoder, layer[0x08])), decoder.layer[0x08], 0x08, },
	{ master, off_t(offsetof(gpt::model::decoder, layer[0x09])), decoder.layer[0x09], 0x09, },
	{ master, off_t(offsetof(gpt::model::decoder, layer[0x0a])), decoder.layer[0x0a], 0x0a, },
	{ master, off_t(offsetof(gpt::model::decoder, layer[0x0b])), decoder.layer[0x0b], 0x0b, },
}
,embed
{
	master,
	off_t(offsetof(gpt::model::decoder, embed)),
	decoder.embed,
}
{
}

ircd::gpt::pipe::model::decoder::~decoder()
noexcept
{
}

//
// pipe::model::embed
//

ircd::gpt::pipe::model::embed::embed(cl::data *const master,
                                     const off_t offset,
                                     gpt::model::embed &embed)
:norm
{
	master,
	offset + off_t(offsetof(gpt::model::embed, norm)) + off_t(offsetof(gpt::model::norm, bias)),
	mutable_buffer{embed.norm.bias.elem},
	offset + off_t(offsetof(gpt::model::embed, norm)) + off_t(offsetof(gpt::model::norm, weight)),
	mutable_buffer{embed.norm.weight.elem},
}
,pos
{
	master,
	offset + off_t(offsetof(gpt::model::embed, pos)),
	mutable_buffer{embed.pos}
}
,token
{
	master,
	offset + off_t(offsetof(gpt::model::embed, token)),
	mutable_buffer{embed.token}
}
{
}

ircd::gpt::pipe::model::embed::embed(cl::data *const master,
                                     const off_t offset,
                                     const gpt::model::embed &embed)
:norm
{
	master,
	offset + off_t(offsetof(gpt::model::embed, norm)) + off_t(offsetof(gpt::model::norm, bias)),
	const_buffer{embed.norm.bias.elem},
	offset + off_t(offsetof(gpt::model::embed, norm)) + off_t(offsetof(gpt::model::norm, weight)),
	const_buffer{embed.norm.weight.elem},
}
,pos
{
	master,
	offset + off_t(offsetof(gpt::model::embed, pos)),
	const_buffer{embed.pos}
}
,token
{
	master,
	offset + off_t(offsetof(gpt::model::embed, token)),
	const_buffer{embed.token}
}
{
}

//
// pipe::model::block
//

ircd::gpt::pipe::model::block::block(cl::data *const master,
                                     const off_t offset,
                                     gpt::model::block &block,
                                     const size_t layer)
:attn
{
	master,
	offset + off_t(offsetof(gpt::model::block, attn)),
	block.attn,
}
,ffnn
{
	master,
	offset + off_t(offsetof(gpt::model::block, ffnn)),
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
	offset + off_t(offsetof(gpt::model::block, attn)),
	block.attn,
}
,ffnn
{
	master,
	offset + off_t(offsetof(gpt::model::block, ffnn)),
	block.ffnn,
}
{
}

//
// pipe::model::ffnn
//

ircd::gpt::pipe::model::ffnn::ffnn(cl::data *const master,
                                   const off_t offset,
                                   gpt::model::ffnn &ffnn)
:norm
{
	master,
	offset + off_t(offsetof(gpt::model::ffnn, norm)) + off_t(offsetof(gpt::model::norm, bias)),
	mutable_buffer{ffnn.norm.bias.elem},
	offset + off_t(offsetof(gpt::model::ffnn, norm)) + off_t(offsetof(gpt::model::norm, weight)),
	mutable_buffer{ffnn.norm.weight.elem},
}
,fcon
{
	master,
	offset + off_t(offsetof(gpt::model::ffnn, fcon_bias)),
	mutable_buffer{ffnn.fcon_bias.fcon},
	offset + off_t(offsetof(gpt::model::ffnn, fcon_weight)),
	mutable_buffer{ffnn.fcon_weight},
}
,proj
{
	master,
	offset + off_t(offsetof(gpt::model::ffnn, proj_bias)),
	mutable_buffer{ffnn.proj_bias.elem},
	offset + off_t(offsetof(gpt::model::ffnn, proj_weight)),
	mutable_buffer{ffnn.proj_weight},
}
{
}

ircd::gpt::pipe::model::ffnn::ffnn(cl::data *const master,
                                   const off_t offset,
                                   const gpt::model::ffnn &ffnn)
:norm
{
	master,
	offset + off_t(offsetof(gpt::model::ffnn, norm)) + off_t(offsetof(gpt::model::norm, bias)),
	const_buffer{ffnn.norm.bias.elem},
	offset + off_t(offsetof(gpt::model::ffnn, norm)) + off_t(offsetof(gpt::model::norm, weight)),
	const_buffer{ffnn.norm.weight.elem},
}
,fcon
{
	master,
	offset + off_t(offsetof(gpt::model::ffnn, fcon_bias)),
	const_buffer{ffnn.fcon_bias.fcon},
	offset + off_t(offsetof(gpt::model::ffnn, fcon_weight)),
	const_buffer{ffnn.fcon_weight},
}
,proj
{
	master,
	offset + off_t(offsetof(gpt::model::ffnn, proj_bias)),
	const_buffer{ffnn.proj_bias.elem},
	offset + off_t(offsetof(gpt::model::ffnn, proj_weight)),
	const_buffer{ffnn.proj_weight},
}
{
}

//
// pipe::model::attn
//

ircd::gpt::pipe::model::attn::attn(cl::data *const master,
                                   const off_t offset,
                                   gpt::model::attn &attn)
:norm
{
	master,
	offset + off_t(offsetof(gpt::model::attn, norm)) + off_t(offsetof(gpt::model::norm, bias)),
	mutable_buffer{attn.norm.bias.elem},
	offset + off_t(offsetof(gpt::model::attn, norm)) + off_t(offsetof(gpt::model::norm, weight)),
	mutable_buffer{attn.norm.weight.elem},
}
,fcon
{
	master,
	offset + off_t(offsetof(gpt::model::attn, fcon_bias)),
	mutable_buffer{attn.fcon_bias.fcon},
	offset + off_t(offsetof(gpt::model::attn, fcon_weight)),
	mutable_buffer{attn.fcon_weight},
}
,proj
{
	master,
	offset + off_t(offsetof(gpt::model::attn, proj_bias)),
	mutable_buffer{attn.proj_bias.elem},
	offset + off_t(offsetof(gpt::model::attn, proj_weight)),
	mutable_buffer{attn.proj_weight},
}
{
}

ircd::gpt::pipe::model::attn::attn(cl::data *const master,
                                   const off_t offset,
                                   const gpt::model::attn &attn)
:norm
{
	master,
	offset + off_t(offsetof(gpt::model::attn, norm)) + off_t(offsetof(gpt::model::norm, bias)),
	const_buffer{attn.norm.bias.elem},
	offset + off_t(offsetof(gpt::model::attn, norm)) + off_t(offsetof(gpt::model::norm, weight)),
	const_buffer{attn.norm.weight.elem},
}
,fcon
{
	master,
	offset + off_t(offsetof(gpt::model::attn, fcon_bias)),
	const_buffer{attn.fcon_bias.fcon},
	offset + off_t(offsetof(gpt::model::attn, fcon_weight)),
	const_buffer{attn.fcon_weight},
}
,proj
{
	master,
	offset + off_t(offsetof(gpt::model::attn, proj_bias)),
	const_buffer{attn.proj_bias.elem},
	offset + off_t(offsetof(gpt::model::attn, proj_weight)),
	const_buffer{attn.proj_weight},
}
{
}

//
// pipe::model::tensor
//

ircd::gpt::pipe::model::tensor::tensor(cl::data *const master,
                                       const off_t bias_offset,
                                       const mutable_buffer &bias,
                                       const off_t weight_offset,
                                       const mutable_buffer &weight)
:bias
{
	master,
	bias_offset,
	bias,
}
,weight
{
	master,
	weight_offset,
	weight,
}
{
}

ircd::gpt::pipe::model::tensor::tensor(cl::data *const master,
                                       const off_t bias_offset,
                                       const const_buffer &bias,
                                       const off_t weight_offset,
                                       const const_buffer &weight)
:bias
{
	master,
	bias_offset,
	bias,
}
,weight
{
	master,
	weight_offset,
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
		pad_to(ircd::size(param), 4096),
		offset,
	},
}
,moment
{
	// first moment
	{
		master[1],
		{
			pad_to(ircd::size(param), 4096),
			offset,
		},
	},

	// second moment
	{
		master[2],
		{
			pad_to(ircd::size(param), 4096),
			offset,
		},
	},
}
{
	assert(aligned(offset, 4096));
}

ircd::gpt::pipe::model::matrix::matrix(cl::data *const master,
                                       const off_t offset,
                                       const const_buffer &param)
:param
{
	master[0],
	{
		pad_to(ircd::size(param), 4096),
		offset,
	},
}
{
	assert(aligned(offset, 4096));
}
