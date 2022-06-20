// Matrix Construct Is All You Need Is All You Need Is AllĊĊĊĊĊĊĊĊ
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma clang fp exceptions(ignore)
#pragma clang fp reassociate(on)
#pragma clang fp contract(fast)

namespace ircd::gpt
{
	static void adamw(const opts &, const u32, const f32, const uint, f32 *, f32 *, f32 *);

	static void backprop(const opts &, const u32, const f32, model::norm &, model::norm &, model::norm &);
	static void backprop(const opts &, const u32, const f32, model::attn &, model::attn &, model::attn &);
	static void backprop(const opts &, const u32, const f32, model::ffnn &, model::ffnn &, model::ffnn &);
	static void backprop(const opts &, const u32, const f32, model::block &, model::block &, model::block &);
	static void backprop(const opts &, const u32, const f32, model::embed &, model::embed &, model::embed &);
	static void backprop(const opts &, const u32, const f32, model::decoder &, model::decoder &, model::decoder &);
	extern void backprop(const opts &, const u32, const f32, model::decoder &, f32 *const __restrict__ [2]) noexcept;

	template<class T>
	static void fmma(T *out, const T *in, const T *bias, const T *weight, const math::fmma_opts &);

	static void gelu(f32x4 &, const f32x4 &);
	static void gelu(f32x4 *, const f32x4 *);
	static void norm(f32x4 *, const f32x4 *, const f32x4 *, const f32x4 *, const f32);
	static void vals(float (&)[12][1024][64], const float (&)[12][1024][1024], const float (&)[3][1024][12][64], const size_t);
	static void pare(float (&)[12][1024][1024], const float (&)[3][1024][12][64], const size_t);
	static void mask(float (&)[12][1024][1024], const float (&)[12][1024][1024], const size_t);
	static void smax(float (&)[12][1024][1024], const float (&)[12][1024][1024], const size_t);
	static void attn(float (&)[3][1024][12][64], const float *const, const size_t, const model::decoder &, const uint layer);
	static void ffnn(float *, const float *, const model::decoder &, const uint layer);
	static void coil(float *, const size_t, const model::decoder &);
	static void logitsmax(float *, const float *, const size_t);
	static void logits(float *, const float *, const model::decoder &);
	static void tail(float *, const float *, const model::decoder &);
	static u16 argmax(const float *, const opts &);
	static void embed(float *, const u16 token, const u16 position, const model::decoder &);

	static f32
	logit alignas(64) [65536],
	embeds alignas(64) [1024 * 768],
	scratch alignas(64) [1024 * 768];
}

void
ircd::gpt::embed(float *const out,
                 const u16 token,
                 const u16 position,
                 const model::decoder &model)
{
	const auto &wpe
	{
		model.embed.pos[position]
	};

	const auto &wte
	{
		model.embed.token[token]
	};

	for(uint j(0); j < 768; ++j)
		out[j] = wte.elem[j] + wpe.elem[j];
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

	norm((f32x4 *)buf[0], (const f32x4 *)state, (const f32x4 *)d.embed.norm.bias.elem, (const f32x4 *)d.embed.norm.weight.elem, lnf_epsilon);
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
			out[j] += in[k] * d.embed.token[j].elem[k];
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

		attn(qkv, accum, tokens, decoder, i);
		pare(state, qkv, tokens);
		mask(state, state, tokens);
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
			fmma((f32x4 *)(accum + j * 768), (const f32x4 *)(a[j]), (const f32x4 *)layer.attn.proj_bias.elem, (const f32x4 *)layer.attn.proj_weight, fmma_opts);

		for(uint j(0); j < tokens; ++j)
			ffnn(accum + j * 768, accum + j * 768, decoder, i);
	}
}

void
ircd::gpt::attn(float (&__restrict__ out)[3][1024][12][64],
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

		norm((f32x4 *)buf, (const f32x4 *)(in + i * 768), (const f32x4 *)layer.attn.norm.bias.elem, (const f32x4 *)layer.attn.norm.weight.elem, ln1_epsilon);

		static const math::fmma_opts fmma_opts
		{
			768, 2304, 2U,
		};

		memset(proj, 0x0, sizeof(proj));
		fmma((f32x4 *)proj, (const f32x4 *)buf, (const f32x4 *)layer.attn.fcon_bias.fcon, (const f32x4 *)layer.attn.fcon_weight, fmma_opts);

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
				out[j][k][l] = (k < l)? in[j][k][l]: masked;
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
	norm((f32x4 *)buf, (const f32x4 *)in, (const f32x4 *)layer.ffnn.norm.bias.elem, (const f32x4 *)layer.ffnn.norm.weight.elem, ln2_epsilon);
	fmma((f32x4 *)buf2, (const f32x4 *)buf, (const f32x4 *)layer.ffnn.fcon_bias.fcon, (const f32x4 *)layer.ffnn.fcon_weight, fmma3_opts);
	gelu((f32x4 *)buf2, (const f32x4 *)buf2);
	fmma((f32x4 *)out, (const f32x4 *)buf2, (const f32x4 *)layer.ffnn.proj_bias.elem, (const f32x4 *)layer.ffnn.proj_weight, fmma4_opts);
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
void
ircd::gpt::backprop(const opts &opts,
                    const u32 step,
                    const f32 grad,
                    model::decoder &__restrict__ param,
                    f32 *const __restrict__ buf[2])
noexcept
{
	model::decoder *const __restrict__ moment[2]
	{
		reinterpret_cast<model::decoder *>(buf[0]),
		reinterpret_cast<model::decoder *>(buf[1]),
	};

	backprop(opts, step, grad, param, *moment[0], *moment[1]);
}

void
ircd::gpt::backprop(const opts &opts,
                    const u32 step,
                    const f32 grad,
                    model::decoder &__restrict__ param,
                    model::decoder &__restrict__ moment0,
                    model::decoder &__restrict__ moment1)
{
	fpe::errors_handle eh;

	assume(opts.attn_rank > 0);
	assume(opts.layers > 0);
	const auto eln
	{
		opts.layers - 1 // step % opts.layers
	};

	for(int i(opts.layers - 1); i >= int(opts.layers - eln - 1); --i)
	{
		assert(i >= 0 && i < int(opts.layers));
		backprop(opts, step, grad, param.layer[i], moment0.layer[i], moment1.layer[i]);
	}

	backprop(opts, step, grad, param.embed, moment0.embed, moment1.embed);

	auto pending(eh.pending());
	eh.clear_pending();
	pending &= ~pending & FE_INEXACT;
	if(unlikely(pending))
		fpe::throw_errors(pending);
}

void
ircd::gpt::backprop(const opts &opts,
                    const u32 step,
                    const f32 grad,
                    model::embed &__restrict__ param,
                    model::embed &__restrict__ moment0,
                    model::embed &__restrict__ moment1)
{
	backprop(opts, step, grad, param.norm, moment0.norm, moment1.norm);

	assume(opts.context_tokens > 0);
	for(uint i(0); i < opts.context_tokens; ++i)
		adamw(opts, step, grad, 768, param.pos[i].elem, moment0.pos[i].elem, moment1.pos[i].elem);

	assume(opts.logits > 0);
	for(uint i(0); i < opts.logits; ++i)
		adamw(opts, step, grad, 768, param.token[i].elem, moment0.token[i].elem, moment1.token[i].elem);
}

void
ircd::gpt::backprop(const opts &opts,
                    const u32 step,
                    const f32 grad,
                    model::block &__restrict__ param,
                    model::block &__restrict__ moment0,
                    model::block &__restrict__ moment1)
{
	backprop(opts, step, grad, param.attn.norm, moment0.attn.norm, moment1.attn.norm);
	backprop(opts, step, grad, param.attn, moment0.attn, moment1.attn);

	backprop(opts, step, grad, param.ffnn.norm, moment0.ffnn.norm, moment1.ffnn.norm);
	backprop(opts, step, grad, param.ffnn, moment0.ffnn, moment1.ffnn);
}

void
ircd::gpt::backprop(const opts &opts,
                    const u32 step,
                    const f32 grad,
                    model::attn &__restrict__ param,
                    model::attn &__restrict__ moment0,
                    model::attn &__restrict__ moment1)
{
	adamw(opts, step, grad, 2304, param.fcon_bias.fcon, moment0.fcon_bias.fcon, moment1.fcon_bias.fcon);

	for(uint i(0); i < 768; ++i)
		adamw(opts, step, grad, 2304, param.fcon_weight[i].fcon, moment0.fcon_weight[i].fcon, moment1.fcon_weight[i].fcon);

	adamw(opts, step, grad, 768, param.proj_bias.elem, moment0.proj_bias.elem, moment1.proj_bias.elem);

	for(uint i(0); i < 768; ++i)
		adamw(opts, step, grad, 768, param.proj_weight[i].elem, moment0.proj_weight[i].elem, moment1.proj_weight[i].elem);
}

void
ircd::gpt::backprop(const opts &opts,
                    const u32 step,
                    const f32 grad,
                    model::ffnn &__restrict__ param,
                    model::ffnn &__restrict__ moment0,
                    model::ffnn &__restrict__ moment1)
{
	adamw(opts, step, grad, 3072, param.fcon_bias.fcon, moment0.fcon_bias.fcon, moment1.fcon_bias.fcon);

	for(uint i(0); i < 768; ++i)
		adamw(opts, step, grad, 3072, param.fcon_weight[i].fcon, moment0.fcon_weight[i].fcon, moment1.fcon_weight[i].fcon);

	adamw(opts, step, grad, 768, param.proj_bias.elem, moment0.proj_bias.elem, moment1.proj_bias.elem);

	for(uint i(0); i < 3072; ++i)
		adamw(opts, step, grad, 768, param.proj_weight[i].elem, moment0.proj_weight[i].elem, moment1.proj_weight[i].elem);
}

void
ircd::gpt::backprop(const opts &opts,
                    const u32 step,
                    const f32 grad,
                    model::norm &__restrict__ param,
                    model::norm &__restrict__ moment0,
                    model::norm &__restrict__ moment1)
{
	adamw(opts, step, grad, 768, param.bias.elem, moment0.bias.elem, moment1.bias.elem);
	adamw(opts, step, grad, 768, param.weight.elem, moment0.weight.elem, moment1.weight.elem);
}

namespace ircd::gpt
{
	static f32x4 adamw_moment(const f32x4, const f32, const f32);
	static f32x4 adamw_numer(const f32x4, const f32, const u32);
	static f32x4 adamw_denom(const f32x4, const f32, const u32);
	static f32x4 adamw_delta(const f32x4, const f32x4, const f32, const f32, const f32, const u32);
	static void adamw(f32x4 &, f32x4 &, f32x4 &, const f32, const f32, const f32, const f32, const u32);
	static void adamw(const opts &, const u32, const f32, const u32, f32 *, f32 *, f32 *);
}

void
ircd::gpt::adamw(const opts &opts,
                 const u32 step,
                 const f32 grad,
                 const u32 num,
                 f32 *const __restrict__ param,
                 f32 *const __restrict__ moment0,
                 f32 *const __restrict__ moment1)
{
	f32x4 *const __restrict__ val[3]
	{
		reinterpret_cast<f32x4 *>(param),
		reinterpret_cast<f32x4 *>(moment0),
		reinterpret_cast<f32x4 *>(moment1),
	};

	const auto n
	{
		num / 4
	};

	assume(0 < n);
	for(uint i(0); i < n; ++i)
		adamw
		(
			val[0][i],
			val[1][i],
			val[2][i],
			grad,
			opts.alpha,
			opts.beta[0],
			opts.beta[1],
			step + 1
		);
}

void
ircd::gpt::adamw(f32x4 &__restrict__ param,
                 f32x4 &__restrict__ moment0,
                 f32x4 &__restrict__ moment1,
                 const f32 grad_,
                 const f32 alpha_,
                 const f32 beta0,
                 const f32 beta1,
                 const u32 step)
{
	const f32 alpha
	{
		grad_ < 0? -alpha_ : alpha_
	};

	const f32 grad
	{
		grad_ < 0? -grad_ : grad_
	};

	const f32 grad_grad
	{
		grad * grad
	};

	const f32x4 moment[]
	{
		adamw_moment(moment0, grad, beta0),
		adamw_moment(moment1, grad_grad, beta1)
	};

	const f32x4 delta
	{
		adamw_delta(moment[0], moment[1], alpha, beta0, beta1, step)
	};

	const f32x4 update
	{
		param - delta
	};

	if((false))
	for(uint i(0); i < 4; ++i)
		printf("%-15p p[%11.8lf] m[%11.8lf][%11.8lf] g[%11.8lf] d[%11.8lf] p[%11.8lf] m[%11.8lf][%11.8lf]\n",
		((f32 *)&param) + i,
		param[i],
		moment0[i],
		moment1[i],
		grad,
		delta[i],
		update[i],
		moment[0][i],
		moment[1][i]);

	assert(std::isnormal(update[0]));
	assert(std::isnormal(update[1]));
	assert(std::isnormal(update[2]));
	assert(std::isnormal(update[3]));

	assert(std::isnormal(moment[0][0]));
	assert(std::isnormal(moment[0][1]));
	assert(std::isnormal(moment[0][2]));
	assert(std::isnormal(moment[0][3]));

	assert(std::isnormal(moment[1][0]));
	assert(std::isnormal(moment[1][1]));
	assert(std::isnormal(moment[1][2]));
	assert(std::isnormal(moment[1][3]));

	param = update;
	//__builtin_nontemporal_store(update, &param);

	moment0 = moment[0];
	moment1 = moment[1];
	//__builtin_nontemporal_store(moment[0], &moment0);
	//__builtin_nontemporal_store(moment[1], &moment1);
}

ircd::f32x4
ircd::gpt::adamw_delta(const f32x4 moment0,
                       const f32x4 moment1,
                       const f32 alpha,
                       const f32 beta0,
                       const f32 beta1,
                       const u32 step)
{
	static const f32 epsilon
	{
		FLT_EPSILON
	};

	const f32x4 denom
	{
		adamw_denom(moment1, beta1, step) + epsilon
	};

	const f32x4 decay
	{
		adamw_numer(moment0, beta0, step)
	};

	const f32x4 smooth
	{
		alpha * decay
	};

	assert(std::isnormal(denom[0]));
	assert(std::isnormal(denom[1]));
	assert(std::isnormal(denom[2]));
	assert(std::isnormal(denom[3]));
	const f32x4 delta
	{
		smooth / denom
	};

	return delta;
}

ircd::f32x4
ircd::gpt::adamw_denom(const f32x4 moment,
                       const f32 beta,
                       const u32 step)
{
	static const f32x4 one
	{
		1.0f, 1.0f, 1.0f, 1.0f,
	};

	assert(step > 0);
	const f32x4 decay
	{
		one - powf(beta, step)
	};

	assert(std::isnormal(decay[0]));
	assert(std::isnormal(decay[1]));
	assert(std::isnormal(decay[2]));
	assert(std::isnormal(decay[3]));
	const f32x4 bias
	{
		moment / decay
	};

	const f32x4 denom
	{
		sqrtf(bias)
	};

	return denom;
}

ircd::f32x4
ircd::gpt::adamw_numer(const f32x4 moment,
                       const f32 beta,
                       const u32 step)
{
	static const f32x4 one
	{
		1.0f, 1.0f, 1.0f, 1.0f,
	};

	assert(step > 0);
	const f32x4 decay
	{
		one - powf(beta, step)
	};

	assert(std::isnormal(decay[0]));
	assert(std::isnormal(decay[1]));
	assert(std::isnormal(decay[2]));
	assert(std::isnormal(decay[3]));
	const f32x4 bias
	{
		moment / decay
	};

	return bias;
}

ircd::f32x4
ircd::gpt::adamw_moment(const f32x4 moment,
                        const f32 grad,
                        const f32 beta)
{
	static const f32x4 one
	{
		1.0f, 1.0f, 1.0f, 1.0f,
	};

	const f32x4 rate
	{
		one - beta
	};

	const f32x4 avg
	{
		moment * beta
	};

	const f32x4 dot
	{
		rate * grad + avg
	};

	return dot;
}
