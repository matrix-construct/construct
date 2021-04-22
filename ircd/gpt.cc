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
	auto &errc(ctrl.error_seq);
	auto &accc(ctrl.accept_seq);
	ctrl.tokens = in.size();
	ctrl.head = 0;

	const size_t tmax
	{
		in.size() + opts.limit
	};

	const vector_view<f32> accum
	{
		gpt::scratch, tmax * 768
	};

	const vector_view<f32> embeds
	{
		gpt::embeds, tmax * 768
	};

	for(uint j(0); j < in.size(); ++j)
	{
		const vector_view<f32> dst
		{
			data(embeds) + j * 768, 768
		};

		if(ircd::cl::enable)
			ctrl.token[j] = in[j];
		else
			embed(data(dst), in[j], j, opts);

		#if 0 // RB_DEBUG
		static char dbuf[512] {0};
		char report[1536] {0};
		char tmbuf[1][64] {{0}};
		const size_t report_size = snprintf
		(
			report, sizeof(report),
			"%-4u %4u %4u:%-4u %1u%1u  [ %6.2fL %6.2f%% ] %6.2fL %5.1f%%  %s",
			ctrl.epoch,
			ctrl.cycle,
			j,
			ctrl.tokens,
			0,
			0,
			0.0,
			0.0,
			0.0,
			0.0,
			vocab::debug(dbuf, in[j]).c_str()
		);

		log::logf
		{
			log, log::level::DEBUG,
			"%s",
			string_view{report, report_size}
		};
		#endif
	}

	uint64_t cycles(0);
	milliseconds last_time {0};
	util::timer stopwatch;
	{
		const prof::scope_cycles task_cycles
		{
			cycles
		};

		generate(task);
	}
	last_time = stopwatch.at<milliseconds>();
	ctrl.elapsed += last_time.count();

	/*
		coil(data(scratch), tokens, *opts.model);
		tail(logit, data(last_embed), *opts.model);
		out[i] = argmax(logit, *opts);
	*/

	uint accc_thresh[3] {3, 3, 3};
	for(uint i(0); i < 3; ++i)
		for(uint j(3); j > 0; --j)
			if(opts.accept_code[i][j - 1] == -1U)
				--accc_thresh[i];
			else
				break;

	uint errc_thresh[3] {3, 3, 3};
	for(uint i(0); i < 3; ++i)
		for(uint j(3); j > 0; --j)
			if(opts.error_code[i][j - 1] == -1U)
				--errc_thresh[i];
			else
				break;

	for(auto &j(ret); j + in.size() < ctrl.tokens && j < out.size() && !halt; ++j)
	{
		out[j] = ctrl.token[(in.size() + j + ctrl.head) % opts.buffer_tokens];

		for(uint j(0); j < 3; ++j)
			errc[j] = opts.error_code[j][errc[j]] == out[j]?
				errc[j] + 1: 0;

		for(uint j(0); j < 3; ++j)
			accc[j] = opts.accept_code[j][accc[j]] == out[j]?
				accc[j] + 1: 0;

		for(uint j(0); j < 3; ++j)
			halt |= accc_thresh[j] && accc[j] >= accc_thresh[j],
			halt |= errc_thresh[j] && errc[j] >= errc_thresh[j];

		static char dbuf[512] {0};
		char report[1536] {0};
		char tmbuf[4][64] {0};
		const size_t bsz(ctrl.tokens - in.size());
		const size_t report_size = snprintf
		(
			report, sizeof(report),
			"%4lu:%-4u %4lu:%-4lu %6.1f%% %5.1fP %6.3fL [%c%c%c] %5u %6.3fL %6.2fP  %5.1f%% %s %04x  %8s %8s | %8s",
			j + in.size(),
			ctrl.tokens,
			ctrl.epoch,
			ctrl.cycle,
			std::clamp(ctrl.cert_mean * 100.0f, 0.0f, 100.0f),
			std::clamp(ctrl.perp_mean, 0.0f, 100.0f),
			std::clamp(ctrl.loss_mean, 0.0f, 99.99f),
			opts.label == out[j]? '+': ' ',
			accc[0] + accc[1] + accc[2] >= 3? 'A': ' ',
			errc[0] + errc[1] + errc[2] >= 3? 'E': ' ',
			opts.label,
			std::clamp(ctrl.loss, 0.0f, 99.99f),
			std::clamp(ctrl.perp, 0.0f, 100.0f),
			std::clamp(ctrl.cert * 100.0f, 0.0f, 100.0f),
			vocab::debug(dbuf, out[j]).c_str(),
			out[j],
			pretty(tmbuf[0], milliseconds(last_time / bsz), 1).c_str(),
			pretty(tmbuf[1], si(cycles / bsz), 1).c_str(),
			pretty(tmbuf[2], milliseconds(ctrl.elapsed), 1).c_str()
		);

		log::logf
		{
			log, log::level::DEBUG,
			"%s",
			string_view{report, report_size}
		};
	}

	ret = ctrl.tokens - in.size();
	if ((false)) for(uint i(0); i < 3; ++i)
		if(accc_thresh[i] && ctrl.accept_seq[i] >= accc_thresh[i])
		{
			ret -= (3 - accc_thresh[i]);
			break;
		}
		else if(errc_thresh[i] && ctrl.error_seq[i] >= errc_thresh[i])
		{
			ret -= (3 - errc_thresh[i]);
			break;
		}

	ctx::interruption_point();
	return vector_view<u16>
	{
		out, ret
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
		case IRCD_GPT_ETOKENS:     return "ETOKENS";
	}

	return "??????";
}
