// Matrix Construct Is All You Need Is All You Need Is AllĊĊĊĊĊĊĊĊ
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::gpt
{
	size_t backprop(task &, const f32, model::decoder &, f32 *const (&)[2], size_t = 0);

	void generate_debug(task &, const uint &, const uint &);
}

decltype(ircd::gpt::log)
ircd::gpt::log
{
	"gpt"
};

ircd::string_view
ircd::gpt::generate(const mutable_buffer &out,
                    const string_view &in,
                    task &task)
{
	u16 buf[2][1024];
	const auto input_tokens
	{
		vocab::tokenize(buf[0], in)
	};

	const auto output_tokens
	{
		generate(buf[1], input_tokens, task)
	};

	const auto output
	{
		vocab::detokenize(out, output_tokens)
	};

	return output;
}

ircd::vector_view<ircd::u16>
ircd::gpt::generate(const vector_view<u16> &out,
                    const vector_view<const u16> &in,
                    task &task)
{
	assert(task.ctrl);
	assert(task.opts);

	uint ret(0);
	bool halt(false);

	const auto &opts(*task.opts);
	auto &ctrl(*task.ctrl);
	ctrl.tokens.count = 0;
	ctrl.tokens.head = 0;

	uint j(0);
	for(uint i(0); i < opts.gates; ++i)
	{
		const auto &gate
		{
			opts.gate[i]
		};

		while(j < in.size() && j < gate.offset && ctrl.tokens.count < opts.buffer_tokens)
			ctrl.token[ctrl.tokens.count++] = in[j++];

		for(uint k(0); k < 7; ++k)
		{
			if(ctrl.tokens.count >= opts.buffer_tokens)
				break;

			if(gate.code[k] == 0)
				break;

			ctrl.token[ctrl.tokens.count++] = gate.code[k];
		}
	}

	while(j < in.size() && ctrl.tokens.count < opts.buffer_tokens)
		ctrl.token[ctrl.tokens.count++] = in[j++];

	const size_t in_size
	{
		ctrl.tokens.count
	};

	generate(task);

	for(uint i(0); i < ctrl.tokens.count && ret < out.size() && !halt; ++i)
	{
		const auto j
		{
			(i + ctrl.tokens.head) % opts.buffer_tokens
		};

		const auto tok
		{
			ctrl.token[j]
		};

		if(j >= in_size)
			out[ret++] = tok;

		if(likely(~opts.debug & 0x01))
			continue;

		if(likely(~opts.debug & 0x02))
			if(j < in_size)
				continue;

		generate_debug(task, j, in_size);
	}

	ctx::interruption_point();
	return vector_view<u16>
	{
		out, ret
	};
}

void
ircd::gpt::generate(task &task)
{
	const auto &opts(*task.opts);
	auto &ctrl(*task.ctrl);

	const size_t in_size
	{
		ctrl.tokens.count
	};

	uint64_t cycles(0);
	if(ctrl.prop)
	{
		static f32 *_momentum[2];
		if(!_momentum[0])
		{
			_momentum[0] = new f32[sizeof(model::decoder) / 4] {0.0f};
			_momentum[1] = new f32[sizeof(model::decoder) / 4] {0.0f};
		}

		f32 *const momentum[2]
		{
			_momentum[0], _momentum[1],
		};

		const prof::scope_cycles task_cycles
		{
			cycles
		};

		backprop(task, ctrl.loss.mean, *model::default_model, momentum);
	}

	if(ctrl.prop)
	{
		log::debug
		{
			log, "Backpropagation of %2.6f in %lu cycles.",
			ctrl.loss.mean,
			cycles,
		};

		ctrl.epic.epoch = 0;
		ctrl.loss.mean = 0;
		ctrl.loss.last = ctrl.loss.mean;
		ctrl.perp.mean = 0;
		ctrl.perp.last = ctrl.perp.mean;
		ctrl.cert.mean = 0;
		ctrl.cert.last = ctrl.cert.mean;
		ctrl.prop = false;
		pipe::default_model->invalid = true;
		return;
	}

	cycles = 0;
	util::timer stopwatch;
	{
		const prof::scope_cycles task_cycles
		{
			cycles
		};

		pipe::generate(task);
	}

	const milliseconds last_time
	{
		stopwatch.at<milliseconds>()
	};

	ctrl.epic.elapsed += last_time.count();
}

void
ircd::gpt::generate_debug(task &task,
                          const uint &i,
                          const uint &in_size)
{
	const auto &opts(*task.opts);
	auto &ctrl(*task.ctrl);

	const auto j
	{
		(i + ctrl.tokens.head) % opts.buffer_tokens
	};

	const auto tok
	{
		ctrl.token[j]
	};

	static char dbuf[512];
	static char report[1536];
	static char tmbuf[4][64];
	const size_t bsz(ctrl.tokens.count - in_size);
	const size_t report_size = snprintf
	(
		report, sizeof(report),
		"%-3u %-4u %4lu:%-4lu %6.1f%% %5.1fP %6.3fL [%c%c%c] %5u %6.3fL %6.2fP  %5.1f%% %s %04x  %8s %8s | %8s",
		j,
		ctrl.tokens.count,
		ctrl.epic.epoch,
		ctrl.epic.cycle,
		std::clamp(ctrl.cert.mean * 100.0f, 0.0f, 100.0f),
		std::clamp(ctrl.perp.mean, 0.0f, 100.0f),
		std::clamp(ctrl.loss.mean, 0.0f, 99.99f),
		opts.label == tok? '+': ' ',
		' ', // flag place
		' ', // flag place
		opts.label,
		std::clamp(ctrl.loss.last, 0.0f, 99.99f),
		std::clamp(ctrl.perp.last, 0.0f, 100.0f),
		std::clamp(ctrl.cert.last * 100.0f, 0.0f, 100.0f),
		vocab::debug(dbuf, tok).c_str(),
		tok,
		pretty(tmbuf[0], milliseconds(0ms / bsz), 1).c_str(),
		pretty(tmbuf[1], si(0UL / bsz), 1).c_str(),
		pretty(tmbuf[2], milliseconds(ctrl.epic.elapsed), 1).c_str()
	);

	log::logf
	{
		log, log::level::DEBUG,
		"%s",
		string_view{report, report_size}
	};
}

//
// gpt::task
//

ircd::gpt::task::task(const gpt::opts *const opts,
                      gpt::ctrl *const ctrl)
:opts
{
	opts
}
,ctrl
{
	ctrl
}
{
	memset(this->ctrl, 0x0, sizeof(gpt::ctrl));

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
// gpt::opts
//

ircd_gpt_opts::ircd_gpt_opts(const ircd::gpt::model::decoder *const model)
noexcept
:model
{
	model?: ircd::gpt::model::default_model
}
,seed
{
	1234567890UL
}
,limit
{
	-1U
}
,top_k
{
	2U
}
,context_tokens
{
	1024U
}
,buffer_tokens
{
	1024U
}
,embed_elems
{
	768U
}
,attn_rank
{
	12U
}
,attn_mult
{
	3U
}
,ffnn_mult
{
	4U
}
,attn_elems
{
	embed_elems * attn_mult
}
,ffnn_elems
{
	embed_elems * ffnn_mult
}
,lanes
{
	4U
}
,embed_width
{
	embed_elems / lanes
}
,attn_width
{
	attn_elems / lanes
}
,attn_height
{
	embed_elems / lanes
}
,ffnn_width
{
	ffnn_elems / lanes
}
,ffnn_height
{
	embed_elems / lanes
}
,logits
{
	50257
}
,training_steps
{
	250000
}
,validation_steps
{
	5000
}
,testing_steps
{
	5000
}
,label
{
	198
}
,debug
{
	0x01
}
,alpha
{
	0.001f
}
,beta
{
	0.9f,
	0.999f,
}
,epsilon
{
	0.000001
}
,gates
{
	0
}
{
}
