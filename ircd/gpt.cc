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
	static size_t adamw(f32x4 &, f32x4 &, f32x4 &, const f32, const f32, const f32, const f32, const u32, size_t);
	static size_t adamw(task &, const f32, f32 *, const size_t, f32 *const (&)[2], const size_t);

	static size_t backprop(task &, const f32, model::norm &, f32 *const (&)[2], size_t);
	static size_t backprop(task &, const f32, model::attn &, f32 *const (&)[2], size_t);
	static size_t backprop(task &, const f32, model::ffnn &, f32 *const (&)[2], size_t);
	static size_t backprop(task &, const f32, model::block &, f32 *const (&)[2], size_t);
	static size_t backprop(task &, const f32, model::embed &, f32 *const (&)[2], size_t);
	static size_t backprop(task &, const f32, model::decoder &, f32 *const (&)[2], size_t = 0);

	template<class T>
	static void fmma(T *out, const T *in, const T *bias, const T *weight, const math::fmma_opts &);

	static void gelu(f32x4 &, const f32x4 &);
	static void gelu(f32x4 *, const f32x4 *);
	static void norm(f32x4 *, const f32x4 *, const f32x4 *, const f32x4 *, const f32);
	static void vals(float (&)[12][1024][64], const float (&)[12][1024][1024], const float (&)[3][1024][12][64], const size_t);
	static void pare(float (&)[12][1024][1024], const float (&)[3][1024][12][64], const size_t);
	static void mask(float (&)[12][1024][1024], const float (&)[12][1024][1024], const bool (&)[1024][1024], const size_t);
	static void smax(float (&)[12][1024][1024], const float (&)[12][1024][1024], const size_t);
	static void ctrl(float (&)[3][1024][12][64], const float *const, const size_t, const model::decoder &, const uint layer);
	static void ffnn(float *, const float *, const model::decoder &, const uint layer);
	static void coil(float *, const size_t, const model::decoder &);
	static void logitsmax(float *, const float *, const size_t);
	static void logits(float *, const float *, const model::decoder &);
	static void tail(float *, const float *, const model::decoder &);
	static u16 argmax(const float *, const opts &);
	static void embed(float *, const u16 token, const u16 position, const opts &);

	static void generate_debug(task &, const uint &, const uint &);

	static f32
	logit alignas(64) [65536],
	embeds alignas(64) [1024 * 768],
	scratch alignas(64) [1024 * 768];
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

void
ircd::gpt::embed(float *const out,
                 const u16 token,
                 const u16 position,
                 const opts &opts)
{
	assert(opts.model);
	const auto &wpe
	{
		opts.model->word.pos[position]
	};

	const auto &wte
	{
		opts.model->word.token[token]
	};

	for(uint j(0); j < 768; ++j)
		out[j] = wte[j] + wpe[j];
}

uint16_t
ircd::gpt::argmax(const float *const __restrict__ logit,
                  const opts &opts)
{
	static const auto max
	{
		32U
	};

	const auto top
	{
		std::clamp(opts.top_k, 1U, max - 1)
	};

	u16 best[max] {0};
	for(uint j(0); j < vocab::tokens; ++j)
	{
		best[top] = j;
		std::sort(begin(best), begin(best) + top + 1, [&logit]
		(const auto &a, const auto &b)
		{
			return logit[a] > logit[b];
		});
	}

	const auto x
	{
		top > 1?
			rand::integer(0, top - 1):
			0
	};

	return best[x];
}

[[gnu::noinline]]
void
ircd::gpt::tail(float *const __restrict__ logit,
                const float *const __restrict__ state,
                const model::decoder &d)
{
	constexpr float lnf_epsilon
	{
		0.00001
	};

	static float
	buf alignas(64) [1][768];
	for(uint i(0); i < 768; ++i)
		buf[0][i] = state[i];

	norm((f32x4 *)buf[0], (const f32x4 *)state, (const f32x4 *)d.f.bias, (const f32x4 *)d.f.weight, lnf_epsilon);
	logits(logit, buf[0], d);
	//logitsmax(logit, logit, vocab::tokens);
}

void
ircd::gpt::logits(float *const __restrict__ out,
                  const float *const __restrict__ in,
                  const model::decoder &d)
{
	for(uint j(0); j < vocab::tokens; ++j)
		out[j] = 0;

	for(uint j(0); j < vocab::tokens; ++j)
		for(uint k(0); k < 768; ++k)
			out[j] += in[k] * d.word.token[j][k];
}

[[gnu::noinline]]
void
ircd::gpt::logitsmax(float *const out,
                     const float *const in,
                     const size_t num)
{
	static f64x4
	exps alignas(4096) [2][65536 / 4];

	math::smax<f32x4, f64x4>
	(
		{(f32x4 *)out,       num / 4},
		{(const f32x4 *)in,  num / 4},
		exps[0],
		exps[1]
	);
}

[[gnu::noinline]]
void
ircd::gpt::coil(float *__restrict__ accum,
                const size_t tokens,
                const model::decoder &decoder)
{
	static float
	qkv alignas(4096) [3][1024][12][64],
	state alignas(4096) [12][1024][1024],
	attns alignas(4096) [12][1024][64];

	for(uint i(0); i < 12; ++i)
	{
		const auto &layer
		{
			decoder.layer[i]
		};

		ctrl(qkv, accum, tokens, decoder, i);
		pare(state, qkv, tokens);
		mask(state, state, layer.attn.bias, tokens);
		smax(state, state, tokens);
		vals(attns, state, qkv, tokens);

		static f32 a alignas(64) [1024][768];
		memset(a, 0x0, 768 * tokens * sizeof(float));
		for(uint j(0); j < tokens; j++)
		{
			for(uint k(0); k < 12; k++)
				for(uint l(0); l < 64; l++)
					a[j][k * 64 + l] = attns[k][j][l];
		}

		static const math::fmma_opts fmma_opts
		{
			768, 768, 2U
		};

		for(uint j(0); j < tokens; ++j)
			fmma((f32x4 *)(accum + j * 768), (const f32x4 *)(a[j]), (const f32x4 *)layer.attn.proj_bias, (const f32x4 *)layer.attn.proj_weight, fmma_opts);

		for(uint j(0); j < tokens; ++j)
			ffnn(accum + j * 768, accum + j * 768, decoder, i);
	}
}

void
ircd::gpt::ctrl(float (&__restrict__ out)[3][1024][12][64],
                const float *const __restrict__ in,
                const size_t num,
                const model::decoder &decoder,
                const uint laynum)
{
	constexpr float ln1_epsilon
	{
		0.00001
	};

	const auto &layer
	{
		decoder.layer[laynum]
	};

	float
	(&__restrict__ qry)[1024][12][64] { out[0] },
	(&__restrict__ key)[1024][12][64] { out[1] },
	(&__restrict__ val)[1024][12][64] { out[2] };

	for(uint i(0); i < num; ++i)
	{
		static float
		buf alignas(64) [768],
		proj alignas(64) [2304];

		norm((f32x4 *)buf, (const f32x4 *)(in + i * 768), (const f32x4 *)layer.ln1.bias, (const f32x4 *)layer.ln1.weight, ln1_epsilon);

		static const math::fmma_opts fmma_opts
		{
			768, 2304, 2U,
		};

		memset(proj, 0x0, sizeof(proj));
		fmma((f32x4 *)proj, (const f32x4 *)buf, (const f32x4 *)layer.attn.attn_bias, (const f32x4 *)layer.attn.attn_weight, fmma_opts);

		#pragma clang loop unroll (disable)
		for(uint j(0); j < 12; ++j)
			for(uint k(0); k < 64; ++k)
				qry[i][j][k] = proj[768 * 0 + j * 64 + k];

		#pragma clang loop unroll (disable)
		for(uint j(0); j < 12; ++j)
			for(uint k(0); k < 64; ++k)
				key[i][j][k] = proj[768 * 1 + j * 64 + k];

		#pragma clang loop unroll (disable)
		for(uint j(0); j < 12; ++j)
			for(uint k(0); k < 64; ++k)
				val[i][j][k] = proj[768 * 2 + j * 64 + k];
	}
}

void
ircd::gpt::pare(float (&__restrict__ out)[12][1024][1024],
                const float (&__restrict__ qkv)[3][1024][12][64],
                const size_t num)
{
	const float
	(&__restrict__ qry)[1024][12][64] { qkv[0] },
	(&__restrict__ key)[1024][12][64] { qkv[1] },
	(&__restrict__ val)[1024][12][64] { qkv[2] };

	#pragma clang loop unroll (disable)
	for(uint j(0); j < 12; ++j)
		for(uint k(0); k < num; ++k)
			for(uint l(0); l < num; ++l)
				out[j][k][l] = 0;

	#pragma clang loop unroll (disable)
	for(uint j(0); j < 12; ++j)
		for(uint k(0); k < num; ++k)
			for(uint l(0); l < num; ++l)
				for(uint m(0); m < 64; ++m)
					out[j][k][l] += qry[k][j][m] * key[l][j][m];

	#pragma clang loop unroll (disable)
	for(uint j(0); j < 12; ++j)
		for(uint k(0); k < num; ++k)
			for(uint l(0); l < num; ++l)
				out[j][k][l] /= 8.0;
}

void
ircd::gpt::mask(float (&__restrict__ out)[12][1024][1024],
                const float (&__restrict__ in)[12][1024][1024],
                const bool (&__restrict__ bias)[1024][1024],
                const size_t num)
{
	static const float masked
	{
		-10000.0
	};

	#pragma clang loop unroll (disable)
	for(uint j(0); j < 12; ++j)
		for(uint k(0); k < num; ++k)
			for(uint l(0); l < num; ++l)
				out[j][k][l] = bias[k][l]? in[j][k][l]: masked;
}

void
ircd::gpt::smax(float (&__restrict__ out)[12][1024][1024],
                const float (&__restrict__ in)[12][1024][1024],
                const size_t num)
{
	static f64
	tmp alignas(4096) [2][1024];

	#pragma clang loop unroll (disable)
	for(uint j(0); j < 12; ++j)
		for(uint k(0); k < num; ++k)
			math::smax<f32, f64>
			(
				out[j][k], { in[j][k], num }, tmp[0], tmp[1]
			);
}

void
ircd::gpt::vals(float (&__restrict__ out)[12][1024][64],
                const float (&__restrict__ in)[12][1024][1024],
                const float (&__restrict__ qkv)[3][1024][12][64],
                const size_t num)
{
	const float
	(&__restrict__ val)[1024][12][64] { qkv[2] };

	#pragma clang loop unroll (disable)
	for(uint j(0); j < 12; ++j)
		for(uint k(0); k < num; ++k)
			for(uint l(0); l < 64; ++l)
				out[j][k][l] = 0;

	#pragma clang loop unroll (disable)
	for(uint j(0); j < 12; ++j)
		for(uint k(0); k < num; ++k)
			for(uint l(0); l < num; ++l)
				for(uint m(0); m < 64; ++m)
					out[j][k][m] += in[j][k][l] * val[l][j][m];
}

void
ircd::gpt::ffnn(float *const out,
                const float *const in,
                const model::decoder &decoder,
                const uint laynum)
{
	static const math::fmma_opts fmma3_opts
	{
		768, 3072, 2U,
	};

	static const math::fmma_opts fmma4_opts
	{
		3072, 768, 2U,
	};

	constexpr float ln2_epsilon
	{
		0.00001
	};

	const auto &layer
	{
		decoder.layer[laynum]
	};

	static float
	buf alignas(64) [768],
	buf2 alignas(64) [3072];

	memset(buf2, 0x0, sizeof(buf2));
	norm((f32x4 *)buf, (const f32x4 *)in, (const f32x4 *)layer.ln2.bias, (const f32x4 *)layer.ln2.weight, ln2_epsilon);
	fmma((f32x4 *)buf2, (const f32x4 *)buf, (const f32x4 *)layer.ffnn.fc_bias, (const f32x4 *)layer.ffnn.fc_weight, fmma3_opts);
	gelu((f32x4 *)buf2, (const f32x4 *)buf2);
	fmma((f32x4 *)out, (const f32x4 *)buf2, (const f32x4 *)layer.ffnn.proj_bias, (const f32x4 *)layer.ffnn.proj_weight, fmma4_opts);
}

void
ircd::gpt::norm(f32x4 *const __restrict__ out,
                const f32x4 *const __restrict__ in,
                const f32x4 *const __restrict__ bias,
                const f32x4 *const __restrict__ weight,
                const float epsilon)
{
	static f64x4
	tmp alignas(64) [768 / 4];

	math::norm<f32x4, f64x4>
	(
		{out, 192}, {in, 192}, epsilon, tmp
	);

	for(uint j(0); j < 768 / 4; ++j)
		out[j] = out[j] * weight[j] + bias[j];
}

template<class T>
void
ircd::gpt::fmma(T *const __restrict__ out,
                const T *const __restrict__ in,
                const T *const __restrict__ bias,
                const T *const __restrict__ weight,
                const math::fmma_opts &opts)
{
	for(uint i(0); i < opts.rows / simd::lanes<T>(); ++i)
		out[i] += bias[i];

	math::fmma(out, in, weight, opts);
}

void
ircd::gpt::gelu(f32x4 *const out,
                const f32x4 *const in)
{
	for(uint j(0); j < 3072 / 4; ++j)
		gelu(out[j], in[j]);
}

void
ircd::gpt::gelu(f32x4 &out,
                const f32x4 &in)
{
	out = 0.5 * in * (1.0 + tanh(in * f32(0.7978845608) * (1.0 + f32(0.044715) * in * in)));
}

//
// backside
//

[[gnu::noinline]]
size_t
ircd::gpt::backprop(task &task,
                    const f32 grad,
                    model::decoder &param,
                    f32 *const (&moment)[2],
                    size_t off)
{
	for(uint i(0); i < 12; ++i)
		off = backprop(task, grad, param.layer[i], moment, off);

	off = backprop(task, grad, param.f, moment, off);
	off = backprop(task, grad, param.word, moment, off);
	return off;
}

size_t
ircd::gpt::backprop(task &task,
                    const f32 grad,
                    model::embed &param,
                    f32 *const (&moment)[2],
                    size_t off)
{
	assert(task.opts);
	const auto &opts
	{
		*task.opts
	};

	for(uint i(0); i < opts.context_tokens; ++i)
		off = adamw(task, grad, param.pos[i], 768, moment, off);

	for(uint i(0); i < opts.logits; ++i)
		off = adamw(task, grad, param.token[i], 768, moment, off);

	return off;
}

size_t
ircd::gpt::backprop(task &task,
                    const f32 grad,
                    model::block &param,
                    f32 *const (&moment)[2],
                    size_t off)
{
	off = backprop(task, grad, param.ln1, moment, off);
	off = backprop(task, grad, param.attn, moment, off);
	off = backprop(task, grad, param.ln2, moment, off);
	off = backprop(task, grad, param.ffnn, moment, off);
	return off;
}

size_t
ircd::gpt::backprop(task &task,
                    const f32 grad,
                    model::attn &param,
                    f32 *const (&moment)[2],
                    size_t off)
{
	off = adamw(task, grad, param.attn_bias, 2304, moment, off);

	for(uint i(0); i < 768; ++i)
		off = adamw(task, grad, param.attn_weight[i], 2304, moment, off);

	off = adamw(task, grad, param.proj_bias, 768, moment, off);

	for(uint i(0); i < 768; ++i)
		off = adamw(task, grad, param.proj_weight[i], 768, moment, off);

	return off;
}

size_t
ircd::gpt::backprop(task &task,
                    const f32 grad,
                    model::ffnn &param,
                    f32 *const (&moment)[2],
                    size_t off)
{
	off = adamw(task, grad, param.fc_bias, 3072, moment, off);

	for(uint i(0); i < 768; ++i)
		off = adamw(task, grad, param.fc_weight[i], 3072, moment, off);

	off = adamw(task, grad, param.proj_bias, 768, moment, off);

	for(uint i(0); i < 3072; ++i)
		off = adamw(task, grad, param.proj_weight[i], 768, moment, off);

	return off;
}

size_t
ircd::gpt::backprop(task &task,
                    const f32 grad,
                    model::norm &param,
                    f32 *const (&moment)[2],
                    size_t off)
{
	off = adamw(task, grad, param.bias, 768, moment, off);
	off = adamw(task, grad, param.weight, 768, moment, off);
	return off;
}

[[gnu::noinline]]
size_t
ircd::gpt::adamw(task &task,
                 const f32 grad,
                 f32 *const p_,
                 const size_t num,
                 f32 *const (&__restrict__ m_)[2],
                 size_t off)
{
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

	f32x4 *const p[3]
	{
		reinterpret_cast<f32x4 *>(p_),
		reinterpret_cast<f32x4 *>(m_[0]) + off,
		reinterpret_cast<f32x4 *>(m_[1]) + off,
	};

	assert(num >= 4);
	const uint n
	{
		uint(num) / 4
	};

	// Assume loop body always taken w/o soundness; otherwise extra branch.
	assert(n > 0);
	uint i(0); do
	{
		off = adamw
		(
			p[0][i],
			p[1][i],
			p[2][i],
			grad,
			opts.alpha,
			opts.beta[0],
			opts.beta[1],
			ctrl.epic.step,
			off
		);
	}
	while(++i < n);

	return off;
}

size_t
ircd::gpt::adamw(f32x4 &__restrict__ param,
                 f32x4 &__restrict__ moment0,
                 f32x4 &__restrict__ moment1,
                 const f32 grad,
                 const f32 alpha,
                 const f32 beta0,
                 const f32 beta1,
                 const u32 step,
                 const size_t off)
{
	const f32x4 one
	{
		1.0f, 1.0f, 1.0f, 1.0f,
	};

	const f32x4 a[2]
	{
		{ one - beta0 },
		{ one - beta1 },
	};

	const f32x4 avg_mul[2]
	{
		{ moment0 * beta0 },
		{ moment1 * beta1 },
	};

	const f32x4 avg_dot[2]
	{
		{ avg_mul[0] + a[0] * grad         },
		{ avg_mul[1] + a[1] * grad * grad  },
	};

	const f32x4 bias[2]
	{
		{ avg_dot[0] / (one - powf(beta0, step + 1)) },
		{ avg_dot[1] / (one - powf(beta1, step + 1)) },
	};

	const f32x4 denom
	{
		sqrtf(bias[1]) + 0.000001f // epsilon
	};

	const f32x4 delta
	{
		alpha * (bias[0] / denom)
	};

	const f32x4 update
	{
		param - delta
	};

	moment0 = avg_dot[0];
	moment1 = avg_dot[1];
	param = update;
	return off + 1;
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
