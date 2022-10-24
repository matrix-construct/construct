// Matrix Construct Is All You Need Is All You Need Is AllĊĊĊĊĊĊĊĊ
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::gpt::log)
ircd::gpt::log
{
	"gpt"
};

//
// debug
//

void
ircd::gpt::log_debug_prof(const opts &opts,
                          const ctrl &ctrl,
                          const pipe::prof &prof)
{
	static char
	buf[2][512];

	const auto head
	{
		debug_head(buf[0], opts, ctrl)
	};

	for(uint i(0); i < prof.stages; ++i)
	{
		if(!std::get<1>(prof.info[i]))
			continue;

		log::logf
		{
			log, log::level::DEBUG,
			"%s %2u: %s",
			head,
			i,
			pipe::debug(buf[1], prof, i),
		};
	}
}

void
ircd::gpt::log_debug_topn(const opts &opts,
                          const ctrl &ctrl)
{
	static char
	buf[2][512];

	const auto head
	{
		debug_head(buf[0], opts, ctrl)
	};

	for(uint i(0); i < opts.top_n; ++i)
		log::logf
		{
			log, log::level::DEBUG,
			"%s %s",
			head,
			debug_top(buf[1], opts, ctrl, i),
		};
}

void
ircd::gpt::log_debug_labels(const opts &opts,
                            const ctrl &ctrl)
{
	static char
	buf[2][512];

	const auto head
	{
		debug_head(buf[0], opts, ctrl)
	};

	for(uint i(0); i < opts.labels; ++i)
		log::logf
		{
			log, log::level::DEBUG,
			"%s %s",
			head,
			debug_label(buf[1], opts, ctrl, i, 1),
		};
}

void
ircd::gpt::log_debug_attns_top(const opts &opts,
                               const ctrl &ctrl)
{
	static char
	buf[8][512];

	const auto head
	{
		debug_head(buf[0], opts, ctrl)
	};

	std::map<uint, uint> tokm;
	for(uint i(0); i < opts.layers; ++i)
		for(uint j(0); j < opts.attn_rank; ++j)
			tokm[ctrl.attn[i][j]]++;

	std::vector<std::pair<uint, uint>> tok(begin(tokm), end(tokm));
	std::sort(begin(tok), end(tok), [&tokm]
	(const auto &a, const auto &b)
	{
		return b.second < a.second;
	});

	for(const auto &[idx, score] : tok)
	{
		const auto barsz
		{
			std::min(score, std::min(80U, uint(sizeof(buf[2]) - 1)))
		};

		memset(buf[2], '|', barsz);
		buf[2][barsz] = '\0';

		log::logf
		{
			log, log::level::DEBUG,
			"%s %s [%3u] %s %-3u",
			head,
			vocab::debug(buf[1], ctrl.token[idx], 1),
			idx,
			buf[2],
			score,
		};
	}
}

void
ircd::gpt::log_debug_attns(const opts &opts,
                           const ctrl &ctrl)
{
	static char
	buf[2][512];

	const auto head
	{
		debug_head(buf[0], opts, ctrl)
	};

	for(uint i(0); i < ctrl.count; ++i)
		log::logf
		{
			log, log::level::DEBUG,
			"%s %s",
			head,
			debug_attn(buf[1], opts, ctrl, i),
		};
}

void
ircd::gpt::log_debug_token(const opts &opts,
                           const ctrl &ctrl,
                           const uint i)
{
	static char
	buf[2][512];

	log::logf
	{
		log, log::level::DEBUG,
		"%s %s",
		debug_head(buf[0], opts, ctrl),
		debug_token_at(buf[1], opts, ctrl, i),
	};
}

void
ircd::gpt::log_debug(const opts &opts,
                     const ctrl &ctrl)
{
	static char
	buf[2][512];

	log::logf
	{
		log, log::level::DEBUG,
		"%s %s",
		debug_head(buf[0], opts, ctrl),
		debug(buf[1], opts, ctrl),
	};
}

///////////////////////////////////////////////////////////////////////////////
//
// gpt::task
//

void
ircd::gpt::reset(task &task)
noexcept
{
	clear(task);
	seed(task);
}

void
ircd::gpt::clear(task &task)
noexcept
{
	assert(task.ctrl);
	memset(task.ctrl, 0x0, sizeof(gpt::ctrl));
}

void
ircd::gpt::seed(task &task)
noexcept
{
	assert(task.opts);
	seed(task, task.opts->seed);
}

void
ircd::gpt::seed(task &task,
                const uint64_t &val)
noexcept
{
	assert(task.ctrl);
    task.ctrl->rand[0] = val;
    task.ctrl->rand[1] = val;
    task.ctrl->rand[2] = 65537;
    task.ctrl->rand[3] = -1UL;
}

//
// gpt::task::task
//

ircd::gpt::task::task(const gpt::opts *const opts,
                      gpt::ctrl *const ctrl)
try
:opts
{
	opts
}
,ctrl
{
	ctrl
}
,code
{
	pipe::default_code?:
		(pipe::default_code = std::make_shared<pipe::code>())
}
,model
{
	!gpt::model::cache_shared?
		std::make_unique<pipe::model>
		(
			*const_cast<const gpt::model::decoder *>(gpt::model::default_model)
		):
		std::make_unique<pipe::model>
		(
			*const_cast<gpt::model::decoder *>(gpt::model::default_model)
		)
}
,desc
{
	this->opts,
	this->ctrl,
	*this->model,
	*this->code,
}
{
	assert(aligned(opts, size_t(cl::data::gart_page_size)));
	assert(aligned(ctrl, size_t(cl::data::gart_page_size)));

	seed(*this, this->opts->seed);
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Task ctor :%s", e.what()
	};

	throw;
}

ircd::gpt::task::~task()
noexcept
{
}

ircd::string_view
ircd::gpt::task::operator()(const mutable_buffer &out,
                            const string_view &in)
{
	u16 input_buf[1024];
	const auto input_tokens
	{
		gpt::vocab::tokenize(input_buf, in)
	};

	u16 output_buf[1024];
	const auto output_tokens
	{
		operator()(output_buf, input_tokens)
	};

	const auto output
	{
		gpt::vocab::detokenize(out, output_tokens)
	};

	return output;
}

ircd::vector_view<ircd::u16>
ircd::gpt::task::operator()(const vector_view<u16> &out,
                            const vector_view<const u16> &in)
{
	assert(this->opts);
	const auto &opts{*this->opts};

	assert(this->ctrl);
	auto &ctrl{*this->ctrl};

	size_t in_i(0);
	for(; in_i < in.size() && ctrl.count < opts.buffer_tokens; in_i++)
		if(in[in_i] == 628)
		{
			ctrl.token[ctrl.count++] = 198;
			ctrl.token[ctrl.count++] = 198;
		}
		else ctrl.token[ctrl.count++] = in[in_i];

	const auto in_count
	{
		ctrl.count
	};

	gpt::epoch epoch
	{
		*this,
	};

	gpt::step step
	{
		epoch
	};

	gpt::samp samp
	{
		step
	};

	bool halt {false}; do
	{
		halt = samp();
	}
	while(!halt);

	size_t out_i(0);
	for(; out_i < out.size() && in_count + out_i < ctrl.count; out_i++)
		out[out_i] = ctrl.token[in_count + out_i];

	return vector_view<u16>
	{
		out, out_i
	};
}

bool
ircd::gpt::task::operator()()
{
	gpt::epoch epoch
	{
		*this
	};

	while(!epoch())
		ctx::interruption_point();

	return done();
}

bool
ircd::gpt::task::done()
const noexcept
{
	return false;
}

///////////////////////////////////////////////////////////////////////////////
//
// epoch
//

//
// epoch::epoch
//

ircd::gpt::epoch::epoch(gpt::task &task)
:task
{
	task
}
,desc
{
	task.desc
}
,opts
{
	*task.opts
}
,ctrl
{
	*task.ctrl
}
,id
{
	ctrl.clk.epoch
}
,start
{
	0
}
,stop
{
	std::min(start + uint(opts.batch_size), gpt::model::default_data.size())
}
,moment
{
	gpt::model::default_moment[0],
	gpt::model::default_moment[1],
}
{
	assert(task.opts);
	assert(task.ctrl);

	ctrl.clk.step = 0;
}

ircd::gpt::epoch::~epoch()
noexcept
{
	if(opts.debug & 0x80000000U)
		log_debug_prof(opts, ctrl, this->profile);
}

bool
ircd::gpt::epoch::operator()()
{
	gpt::step step
	{
		*this
	};

	while(!step())
		ctx::interruption_point();

	return done();
}

bool
ircd::gpt::epoch::done()
const noexcept
{
	return ctrl.clk.epoch != id;
}

void
ircd::gpt::epoch::profile_accumulate(const pipe::prof &profile)
{
	for(size_t i(0); i < profile.ts.size(); ++i)
		for(size_t j(0); j < profile.phases; ++j)
			this->profile.ts[i][j] += profile.ts[i][j];
}

///////////////////////////////////////////////////////////////////////////////
//
// step::step
//

ircd::gpt::step::step(gpt::epoch &epoch)
:epoch
{
	epoch
}
,desc
{
	epoch.desc
}
,opts
{
	epoch.opts
}
,ctrl
{
	epoch.ctrl
}
,id
{
	ctrl.clk.step
}
,start
{
	ctrl.clk.step * opts.batch_size
}
{
	assert(opts.batch_size > 0);

	ctrl.clk.samp = 0;
	ctrl.hit = 0;
	ctrl.miss = 0;
	ctrl.target.ppl = {{0}};
	ctrl.target.loss = {{0}};
	ctrl.select.ppl = {{0}};
	ctrl.select.loss = {{0}};

	for(uint i(0); i < opts.labels; ++i)
	{
		ctrl.label[i].ppl = {{0}};
		ctrl.label[i].loss = {{0}};
	}
}

ircd::gpt::step::~step()
noexcept
{
	if(opts.debug & 0x40000000U)
		log_debug_prof(opts, ctrl, this->profile);
}

bool
ircd::gpt::step::operator()()
{
	gpt::samp samp
	{
		*this
	};

	while(!samp())
		ctx::interruption_point();

	return done();
}

bool
ircd::gpt::step::done()
const noexcept
{
	return ctrl.clk.step != id;
}

void
ircd::gpt::step::profile_accumulate(const pipe::prof &profile)
{
	for(size_t i(0); i < profile.ts.size(); ++i)
		for(size_t j(0); j < profile.phases; ++j)
			this->profile.ts[i][j] += profile.ts[i][j];

	epoch.profile_accumulate(profile);
}

///////////////////////////////////////////////////////////////////////////////
//
// samp::samp
//

ircd::gpt::samp::samp(gpt::step &step)
:step
{
	step
}
,desc
{
	step.desc
}
,opts
{
	step.opts
}
,ctrl
{
	step.ctrl
}
,id
{
	ctrl.clk.step * opts.batch_size + ctrl.clk.samp
}
,accept
{
	-1
}
,dispatch
{
	1
}
,cycle
{
	0
}
,tokens
{
	ctrl.count?:
		tokenize()
}
,count
{
	opts.limit < 0?
		std::min(std::abs(opts.limit), int(tokens)):
	opts.limit > 0?
		tokens:
		1U
}
{
	desc.cached = 0;

	ctrl.clk.cycle = cycle;
	ctrl.dispatch = dispatch;
	ctrl.accept = accept;
	ctrl.count = count;
	ctrl.tokens = tokens;
	ctrl.magic = 0xDEADBEEF;

	for(uint i(0); i < opts.labels; ++i)
	{
		ctrl.label[i].ppl = {{0}};
		ctrl.label[i].loss = {{0}};
	}

	assert(ctrl.count > 0);
	assert(ctrl.count < opts.context_tokens);
	assert(ctrl.count <= ctrl.tokens);

	if(opts.debug & 0x01)
		for(uint j(0); j < ctrl.count; ++j)
			log_debug_token(opts, ctrl, j);
}

ircd::gpt::samp::~samp()
noexcept
{
	if(run::level != run::level::RUN)
		return;

	if(!desc.ctrl.mapped)
	{
		cl::exec
		{
			desc.ctrl, std::memory_order_acq_rel
		};

		assert(ctrl.magic != 0xC7012C70UL);
		assert(ctrl.magic == 0xDEADBEEF);
	}

	if(opts.debug & 0x04)
		log_debug(opts, ctrl);

	if(opts.debug & 0x40)
		log_debug_labels(opts, ctrl);

	if(opts.debug & 0x20000000U)
		log_debug_prof(opts, ctrl, this->profile);
}

bool
ircd::gpt::samp::operator()()
{
	ctx::interruption_point();

	if(dispatche())
		return false;

	while(!queue.empty())
	{
		const unwind pop{[this]
		{
			queue.pop_front();
		}};

		if(evaluate(queue.front()))
			break;
	}

	return done();
}

bool
ircd::gpt::samp::done()
const noexcept
{
	return accept >= 0 || !dispatch;
}

uint
ircd::gpt::samp::tokenize()
{
	const auto idx
	{
		id
	};

	const gpt::model::text text
	{
		gpt::model::default_data.at(idx)
	};

	const json::string input
	{
		json::get<"text"_>(text)
	};

	thread_local char str_buf[16_KiB];
	const string_view str
	{
		json::unescape(str_buf, input)
	};

	const vector_view<u16> buf
	{
		ctrl.token, opts.buffer_tokens
	};

	const auto in
	{
		gpt::vocab::tokenize(buf, str)
	};

	const auto count
	{
		size(in)
	};

	assert(count > 0);
	assert(count <= opts.context_tokens);
	return count;
}

bool
ircd::gpt::samp::dispatche()
{
	assert(accept < 0);
	if(queue.size() >= dispatch)
		return false;

	if(cycle == 0)
	{
		ctrl.prof.entered = 0;
		ctrl.prof.finished = 0;
		ctrl.prof.acquired = 0;
		ctrl.prof.released = prof::cycles();
	}

	assert(queue.size() < opts.frames);
	queue.emplace_back(*this);

	assert(tokens >= count);
	desc.cached = tokens;
	tokens += count >= tokens;

	assert(count > 0);
	count += 1;

	assert(cycle < count);
	cycle += 1;

	assert(dispatch > 0);
	dispatch -= 1;
	return true;
}

bool
ircd::gpt::samp::evaluate(pipe::cycle &cycle)
{
	const auto &frame
	{
		acquire(cycle)
	};

	if(!retire(cycle, frame))
		return false;

	const uint
	batch_size = opts.batch_size,
	samps = opts.training_steps + opts.validation_steps + opts.testing_steps,
	steps = samps / batch_size;

	const bool
	accepting = accept >= 0,
	cycling = !accepting,
	sampling = accepting,
	stepping = sampling && (frame.clk.samp + 1) >= batch_size,
	epoching = stepping && (frame.clk.step + 1) >= steps;

	if(!accepting)
		return true;

	cl::exec
	{
		desc.ctrl, std::memory_order_acq_rel
	};

	// Workaround buggy drivers which flake on write-back to user ptrs.
	// We manually copy the last frame out to ctrl. On working systems ctrl
	// can be acquired by changing the fence to std::memory_order_acquire.
	memcpy(&ctrl, &frame, sizeof(gpt::ctrl));

	assert(ctrl.magic != 0xDEADBEEF);
	assert(ctrl.magic == 0xC7012C70UL);

	ctrl.prof.acquired = prof::cycles();
	ctrl.clk.cycle += cycling;
	ctrl.clk.samp += sampling;
	ctrl.clk.step += stepping;
	ctrl.clk.epoch += epoching;
	return true;
}

bool
ircd::gpt::samp::retire(pipe::cycle &cycle,
                        const gpt::ctrl &frame)
{
	assert(accept < 0);
	accept = frame.accept;
	dispatch = frame.dispatch;

	if(cl::profile_queue)
	{
		const pipe::prof profile
		{
			cycle
		};

		if(opts.debug & 0x10000000U)
			log_debug_prof(opts, frame, profile);

		profile_accumulate(profile);
	}

	if(opts.debug & 0x02)
		log_debug(opts, frame);

	if(opts.debug & 0x20)
		log_debug_labels(opts, frame);

	if(opts.debug & 0x10)
		log_debug_topn(opts, frame);

	if(opts.debug & 0x200)
		log_debug_attns_top(opts, frame);

	dispatch &= boolmask<uint>(ircd::run::level == run::level::RUN);
	dispatch &= boolmask<uint>(!ctx::interruption_requested());
	dispatch &= boolmask<uint>(accept < 0);
	const bool finished
	{
		dispatch == 0
	};

	return finished;
}

void
ircd::gpt::samp::profile_accumulate(const pipe::prof &profile)
{
	for(size_t i(0); i < profile.ts.size(); ++i)
		for(size_t j(0); j < profile.phases; ++j)
			this->profile.ts[i][j] += profile.ts[i][j];

	step.profile_accumulate(profile);
}

///////////////////////////////////////////////////////////////////////////////
//
// ctrl
//

ircd::string_view
ircd::gpt::debug_top(const mutable_buffer &out,
                     const opts &opts,
                     const ctrl &ctrl,
                     const uint i)
{
	thread_local char buf[2][256];

	assert(opts.top_n > i);
	const auto &top
	{
		ctrl.top[i]
	};

	return fmt::sprintf
	{
		out, "%s T%02d %s",
		vocab::debug(buf[0], top.token, 1),
		i,
		debug(buf[1], opts, top),
	};
}

ircd::string_view
ircd::gpt::debug_label(const mutable_buffer &out,
                       const opts &opts,
                       const ctrl &ctrl,
                       const uint i,
                       const uint fmt)
{
	thread_local char buf[2][256];

	assert(opts.labels > i);
	const auto &label
	{
		ctrl.label[i]
	};

	return fmt::sprintf
	{
		out, "%s L%02d %s",
		vocab::debug(buf[0], label.logit.token, 1),
		i,
		debug(buf[1], opts, label, fmt),
	};
}

ircd::string_view
ircd::gpt::debug_attn(const mutable_buffer &out,
                      const opts &opts,
                      const ctrl &ctrl,
                      const uint ti)
{
	thread_local char buf[4][256];
	assert(ti < ctrl.count);

	memset(buf[1], 0x0, sizeof(buf[1]));
	for(uint i(0); i < opts.layers; ++i)
	{
		const auto f{[&](const auto &a) { return a == ti; }};
		if(std::none_of(ctrl.attn[i], ctrl.attn[i] + opts.attn_rank, f))
			continue;

		strlcat{buf[1], fmt::sprintf
		{
			buf[2], "  %1x[", uint(i)
		}};

		for(uint j(0); j < opts.attn_rank; ++j)
			if(ctrl.attn[i][j] == ti)
				strlcat{buf[1], fmt::sprintf
				{
					buf[2], "%1x", uint(j)
				}};

		strlcat{buf[1], "]"_sv};
	}

	return fmt::sprintf
	{
		out, "%s [%3u] <-%s",
		vocab::debug(buf[0], ctrl.token[ti], 1),
		ti,
		string_view{buf[1]},
	};
}

ircd::string_view
ircd::gpt::debug(const mutable_buffer &out,
                 const opts &opts,
                 const ctrl &ctrl)
{
	thread_local char
	buf[8][128],
	tmbuf[4][32];

	int top_idx {-1};
	for(uint i(0); i < opts.top_n; ++i)
		if(ctrl.top[i].token == ctrl.select.logit.token)
		{
			top_idx = i;
			break;
		}

	return fmt::sprintf
	{
		out, "%s %s %c T%02d %3u %6.2f%% %10.7f$L %c %s %s %s",
		vocab::debug(buf[0], ctrl.select.logit.token, 1),
		debug(buf[1], opts, ctrl.select),
		ctrl.target.logit.token == ctrl.top[0].token? '=' : ' ',
		top_idx,
		ctrl.hit,
		(ctrl.hit / float(ctrl.hit + ctrl.miss)) * 100.0f,
		ctrl.target.loss.mean - ctrl.select.loss.mean,
		ctrl.target.logit.token == ctrl.select.logit.token? '=' : ' ',
		debug(buf[2], opts, ctrl.target),
		vocab::debug(buf[3], ctrl.target.logit.token, 1),
		debug(buf[4], opts, ctrl.prof),
	};
}

ircd::string_view
ircd::gpt::debug(const mutable_buffer &out,
                 const opts &opts,
                 const ctrl_label &label,
                 const uint fmt)
{
	thread_local char buf[64], bar[128];

	const auto diff
	{
		log2f(65536) - label.loss.mean
	};

	const auto pct
	{
		(diff / log2f(opts.logits)) * 100.0f
	};

	const auto barsz
	{
		std::min(uint(pct), std::min(66U, uint(sizeof(bar) - 1)))
	};

	memset(bar, '|', barsz);
	bar[barsz] = '\0';

	return fmt::sprintf
	{
		out,
		fmt == 1?
			"%s %10.7f$La %6.2f%% %s":
			"%s %10.7f$La",
		debug(buf, opts, label.logit, fmt),
		label.loss.mean,
		pct,
		string_view{bar},
	};
}

ircd::string_view
ircd::gpt::debug(const mutable_buffer &out,
                 const opts &opts,
                 const ctrl_logit &logit,
                 const uint fmt)
{
	return fmt::sprintf
	{
		out, "%6.2f%% %10.7f$L %4.1f$P",
		logit.samax * 100.0f,
		+0.0f - logf(logit.samax),
		(1.0f - logit.samax) * log2f(opts.logits),
	};
}

ircd::string_view
ircd::gpt::debug(const mutable_buffer &out,
                 const opts &opts,
                 const ctrl_prof &prof)
{
	thread_local char buf[1][32];

	const auto kern_cycles
	{
		prof.finished && prof.entered?
			prof.finished - prof.entered: 0
	};

	const auto host_cycles
	{
		prof.acquired && prof.released?
			prof.acquired - prof.released: 0
	};

	const auto cust_cycles
	{
		prof.custom[1] && prof.custom[0]?
			prof.custom[1] - prof.custom[0]: 0
	};

	return fmt::sprintf
	{
		out, "%s",
		cust_cycles?
			pretty(buf[0], si(cust_cycles), 1):
		kern_cycles?
			pretty(buf[0], si(kern_cycles), 1):
		host_cycles?
			pretty(buf[0], si(host_cycles), 1):
			string_view{},
	};
}

ircd::string_view
ircd::gpt::debug_head(const mutable_buffer &out,
                      const opts &opts,
                      const ctrl &ctrl)
{
	thread_local char head[64];

	assert(ctrl.count > 0);
	return fmt::sprintf
	{
		out, "%s[%4u]-%1u",
		debug_head(head, opts, ctrl.clk),
		ctrl.count,
		ctrl.dispatch,
	};
}

ircd::string_view
ircd::gpt::debug_head(const mutable_buffer &out,
                      const opts &opts,
                      const ctrl_clk &clk)
{
	return fmt::sprintf
	{
		out, "%02u:%06u|%04u|%04u|%04u",
		clk.epoch,
		clk.step * opts.batch_size + clk.samp,
		clk.step,
		clk.samp,
		clk.cycle,
	};
}

ircd::string_view
ircd::gpt::debug_token(const mutable_buffer &out,
                       const opts &opts,
                       const ctrl &ctrl,
                       const uint fmt)
{
	assert(ctrl.count > 0);
	const auto pos
	{
		ctrl.count - 1
	};

	return debug_token_at(out, opts, ctrl, pos, fmt);
}

ircd::string_view
ircd::gpt::debug_token_at(const mutable_buffer &out,
                          const opts &opts,
                          const ctrl &ctrl,
                          const uint i,
                          const uint fmt)
{
	const auto &token
	{
		ctrl.token[i]
	};

	return vocab::debug(out, token, fmt);
}

///////////////////////////////////////////////////////////////////////////////
//
// opts
//

ircd_gpt_opts::ircd_gpt_opts()
noexcept
:seed
{
	1234567890UL
}
,top_k
{
	16
}
,top_p
{
	0.90f
}
,top_n
{
	0
}
,labels
{
	0
}
,frames
{
	8
}
,limit
{
	-1
}
,debug
{
	0x00
}
,accept
{
	{ 198, 198, ushort(-1),    },
	{ 0, 0, 0, ushort(-1),     },
	{ ushort(-1),              },
	{ ushort(-1),              },
}
,batch_size
{
	32
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
,alpha
{
	0.00002
}
,beta
{
	0.9f,
	0.999f,
}
,epsilon
{
	0.00001
}
,lambda
{
	0.5
}
,logits
{
	50256
}
,buffer_tokens
{
	1024 - 16 // XXX
}
,context_tokens
{
	512 // 1024
}
,layers
{
	12
}
,lanes
{
	4
}
,embed_elems
{
	768
}
,embed_width
{
	embed_elems / lanes
}
,attn_rank
{
	12
}
,attn_mult
{
	3
}
,attn_elems
{
	embed_elems * attn_mult
}
,attn_fcon_width
{
	attn_elems / lanes
}
,attn_fcon_height
{
	embed_elems / lanes
}
,attn_proj_width
{
	embed_elems / lanes
}
,attn_proj_height
{
	embed_elems / lanes
}
,attn_self_elems
{
	(uint(powl(context_tokens, 2)) / 2) * attn_rank
}
,ffnn_mult
{
	4
}
,ffnn_elems
{
	embed_elems * ffnn_mult
}
,ffnn_fcon_width
{
	ffnn_elems / lanes
}
,ffnn_fcon_height
{
	embed_elems / lanes
}
,ffnn_proj_width
{
	embed_elems / lanes
}
,ffnn_proj_height
{
	ffnn_elems / lanes
}
{
}
