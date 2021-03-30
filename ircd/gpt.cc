// Matrix Construct Is All You Need Is All You Need Is AllĊĊĊĊĊĊĊĊ
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
	static void gelu(f32x4 &, const f32x4 &);
	static void gelu(f32x4 *, const f32x4 *);
	static void norm(f32x4 *, const f32x4 *, const f32x4 *, const f32x4 *, const f32);
	static void fmma4(f32x4 *, const f32x4 *, const f32x4 *, const f32x4 *);
	static void fmma3(f32x4 *, const f32x4 *, const f32x4 *, const f32x4 *);
	static void fmma2(f32x4 *, const f32x4 *, const f32x4 *, const f32x4 *, const size_t);
	static void fmma1(f32x4 *, const f32x4 *, const f32x4 *, const f32x4 *);
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
	scratch alignas(64) [1024 * 768];
}

namespace ircd::gpt
{
	extern void transform(ctor_ctrl &, const ctor_opts &);
}

decltype(ircd::gpt::log)
ircd::gpt::log
{
	"gpt"
};

decltype(ircd::gpt::default_opts)
ircd::gpt::default_opts;

ircd::string_view
ircd::gpt::generate(const mutable_buffer &out,
                    const string_view &in,
                    const opts *opts,
                    task *task)
{
	u16 buf[2][256];
	const auto input_tokens
	{
		vocab::tokenize(buf[0], in)
	};

	const auto output_tokens
	{
		generate(buf[1], input_tokens, opts, task)
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
                    const opts *opts,
                    task *task)
{
	uint accc_thresh[3] {3, 3, 3};
	for(uint i(0); i < 3; ++i)
		for(uint j(3); j > 0; --j)
			if(opts->accept_code[i][j - 1] == -1U)
				--accc_thresh[i];
			else
				break;

	uint errc_thresh[3] {3, 3, 3};
	for(uint i(0); i < 3; ++i)
		for(uint j(3); j > 0; --j)
			if(opts->error_code[i][j - 1] == -1U)
				--errc_thresh[i];
			else
				break;

	uint ret(0);
	bool halt(false);
	auto &errc(task->error_seq);
	auto &accc(task->accept_seq);
	for(uint i(0); !halt && i < out.size() && ret < opts->limit; ++i)
	{
		ctor_ctrl ctrl alignas(4096) {0};
		ctrl.pc = 1;

		const size_t tokens
		{
			in.size() + i
		};

		const vector_view<f32> scratch
		{
			gpt::scratch, tokens * 768
		};

		for(uint j(0); j < in.size(); ++j)
		{
			const vector_view<f32> dst
			{
				data(scratch) + j * 768, 768
			};

			if(ircd::cl::enable)
				ctrl.body.token[ctrl.tokens++] = in[j];
			else
				embed(data(dst), in[j], j, *opts);
		}

		for(uint j(0); j < i; ++j)
		{
			const vector_view<f32> dst
			{
				data(scratch) + (in.size() + j) * 768, 768
			};

			if(ircd::cl::enable)
				ctrl.body.token[ctrl.tokens++] = out[j];
			else
				embed(data(dst), out[j], in.size() + j, *opts);
		}

		assert(!ircd::cl::enable || ctrl.tokens == tokens);
		const vector_view<f32> last_embed
		{
			data(scratch) + (tokens - 1) * 768, 768
		};

		const auto last_cycl(task->cycles);
		milliseconds last_time {0};
		{
			util::timer stopwatch;
			const prof::scope_cycles task_cycles
			{
				task->cycles
			};

			if(ircd::cl::enable)
			{
				static const ctor_opts opts alignas(4096) {0};

				transform(ctrl, opts);
				out[i] = ctrl.body.token[ctrl.tokens - 1];
				assert(ctrl.tokens == tokens + 1);
			} else {
				coil(data(scratch), tokens, *opts->model);
				tail(logit, data(last_embed), *opts->model);
				out[i] = argmax(logit, *opts);
			}

			last_time = stopwatch.at<milliseconds>();
			task->time += last_time;
		}

		for(uint j(0); j < 3; ++j)
			errc[j] =
				opts->error_code[j][errc[j]] == out[i]?
					errc[j] + 1:
					0;

		for(uint j(0); j < 3; ++j)
			accc[j] =
				opts->accept_code[j][accc[j]] == out[i]?
					accc[j] + 1:
					0;

		for(uint j(0); j < 3; ++j)
			halt |= accc_thresh[j] && accc[j] >= accc_thresh[j],
			halt |= errc_thresh[j] && errc[j] >= errc_thresh[j];

		static char dbuf[512] {0};
		char report[1536] {0};
		char tmbuf[4][64] {0};
		size_t report_size;
		report_size = snprintf
		(
			report, sizeof(report),
			"%-2u %-3u %-3u [%5u] a:%u e:%u %s %8s %8s  |  %8s",
			i,
			ctrl.tokens,
			ret,
			out[i],
			accc[0] + accc[1] + accc[2],
			errc[0] + errc[1] + errc[2],
			vocab::debug(dbuf, out[i]).c_str(),
			pretty(tmbuf[0], last_time, 1).c_str(),
			pretty(tmbuf[1], si(last_cycl), 1).c_str(),
			pretty(tmbuf[2], task->time, 1).c_str()
		);

		log::info
		{
			log, "%s",
			string_view{report, report_size}
		};

		++ret;
		ctx::yield();
		ctx::interruption_point();
	}

	for(uint i(0); i < 3; ++i)
		if(accc_thresh[i] && task->accept_seq[i] >= accc_thresh[i])
		{
			ret -= (3 - accc_thresh[i]);
			break;
		}
		else if(errc_thresh[i] && task->error_seq[i] >= errc_thresh[i])
		{
			ret -= (3 - errc_thresh[i]);
			break;
		}

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

		for(uint j(0); j < tokens; ++j)
			fmma2((f32x4 *)(accum + j * 768), (const f32x4 *)(a[j]), (const f32x4 *)layer.attn.proj_bias, (const f32x4 *)layer.attn.proj_weight, tokens);

		for(uint j(0); j < tokens; ++j)
			ffnn(accum + j * 768, accum + j * 768, decoder, i);
	}
}

void
ircd::gpt::ffnn(float *const out,
                const float *const in,
                const model::decoder &decoder,
                const uint laynum)
{
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
	fmma3((f32x4 *)buf2, (const f32x4 *)buf, (const f32x4 *)layer.ffnn.fc_bias, (const f32x4 *)layer.ffnn.fc_weight);
	gelu((f32x4 *)buf2, (const f32x4 *)buf2);
	fmma4((f32x4 *)out, (const f32x4 *)buf2, (const f32x4 *)layer.ffnn.proj_bias, (const f32x4 *)layer.ffnn.proj_weight);
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

		memset(proj, 0x0, sizeof(proj));
		fmma1((f32x4 *)proj, (const f32x4 *)buf, (const f32x4 *)layer.attn.attn_bias, (const f32x4 *)layer.attn.attn_weight);

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

void
ircd::gpt::fmma1(f32x4 *const __restrict__ out,
                 const f32x4 *const __restrict__ in,
                 const f32x4 *const __restrict__ bias,
                 const f32x4 *const __restrict__ weight)
{
	constexpr uint width
	{
		2304
	};

	constexpr uint height
	{
		768
	};

	constexpr uint lanes
	{
		simd::lanes<f32x4>()
	};

	for(uint j(0); j < width / lanes; ++j)
		out[j] += bias[j];

	static const math::fmma_opts opts
	{
		width,
		height,
		2U,
		'y',
	};

	math::fmma<opts>(out, in, weight);
}

void
ircd::gpt::fmma2(f32x4 *const __restrict__ out,
                 const f32x4 *const __restrict__ in,
                 const f32x4 *const __restrict__ bias,
                 const f32x4 *const __restrict__ weight,
                 const size_t num)
{
	constexpr uint width
	{
		768
	};

	constexpr uint height
	{
		768
	};

	constexpr uint lanes
	{
		simd::lanes<f32x4>()
	};

	for(uint j(0); j < width / lanes; ++j)
		out[j] += bias[j];

	static const math::fmma_opts opts
	{
		width,
		height,
		2U,
	};

	math::fmma<opts>(out, in, weight);
}

void
ircd::gpt::fmma3(f32x4 *const __restrict__ out,
                 const f32x4 *const __restrict__ in,
                 const f32x4 *const __restrict__ bias,
                 const f32x4 *const __restrict__ weight)
{
	constexpr uint width
	{
		3072
	};

	constexpr uint height
	{
		768
	};

	constexpr uint lanes
	{
		simd::lanes<f32x4>()
	};

	for(uint j(0); j < width / lanes; ++j)
		out[j] += bias[j];

	static const math::fmma_opts opts
	{
		width,
		height,
		2U,
		'y',
	};

	math::fmma<opts>(out, in, weight);
}

void
ircd::gpt::fmma4(f32x4 *const __restrict__ out,
                 const f32x4 *const __restrict__ in,
                 const f32x4 *const __restrict__ bias,
                 const f32x4 *const __restrict__ weight)
{
	constexpr uint width
	{
		3072
	};

	constexpr uint height
	{
		768
	};

	constexpr uint lanes
	{
		simd::lanes<f32x4>()
	};

	for(uint j(0); j < height / lanes; ++j)
		out[j] += bias[j];

	static const math::fmma_opts opts
	{
		width,
		height,
		2U,
	};

	math::fmma<opts>(out, in, weight);
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
