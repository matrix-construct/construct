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
	static void gelu(float &, const float &);
	static void gelu(float (&)[3072], const float (&)[3072]);
	static void norm(float (&)[768], const float *, const float (&)[768], const float (&)[768], const float);
	static void fmma(float (&)[768], const float (&)[3072], const float (&)[768], const float (&)[3072][768]);
	static void fmma(float (&)[3072], const float (&)[768], const float (&)[3072], const float (&)[768][3072]);
	static void fmma(float (&)[2304], const float (&)[768], const float (&)[2304], const float (&)[768][2304]);
	static void fmma(float *, const float (&)[12][1024][64], const float (&)[768], const float (&)[768][768], const size_t);
	static void vals(float (&)[12][1024][64], const float (&)[12][1024][1024], const float (&)[3][1024][12][64], const size_t);
	static void pare(float (&)[12][1024][1024], const float (&)[3][1024][12][64], const size_t);
	static void mask(float (&)[12][1024][1024], const float (&)[12][1024][1024], const bool (&)[1024][1024], const size_t);
	static void smax(float (&)[12][1024][1024], const float (&)[12][1024][1024], const size_t);
	static void ctrl(float (&)[3][1024][12][64], const float *const, const size_t, const model::block &);
	static void ffnn(float (&)[768], const float (&)[768], const model::block &);
	static void transform(float *, const size_t, const model::decoder &);
	static void logitsmax(float *, const float *);
	static void logits(float *, const float (&)[768], const model::decoder &);
	static void tail(float *, const float *, const model::decoder &);
	static u16 argmax(const float *, const opts &);
	static void embed(float *, const u16 token, const u16 position, const opts &);

	static f32
	logit alignas(64) [65536],
	scratch alignas(64) [1024 * 768];
}

decltype(ircd::gpt::log)
ircd::gpt::log
{
	"gpt"
};

decltype(ircd::gpt::default_opts)
ircd::gpt::default_opts;

namespace ircd::gpt::model
{
	constexpr float embed_pdrop
	{
		0.1
	};

	constexpr float ln1_epsilon
	{
		0.00001
	};

	constexpr float ln2_epsilon
	{
		0.00001
	};

	constexpr float lnf_epsilon
	{
		0.00001
	};

	constexpr float attn_pdrop
	{
		0.1
	};

	constexpr float resid_pdrop
	{
		0.1
	};
}

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
	size_t ret(0);
	bool halt(false);
	uint errc[3] {0}, accc[3] {0};
	for(uint i(0); !halt && i < out.size() && ret < opts->limit; ++i)
	{
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

			embed(data(dst), in[j], j, *opts);
		}

		for(uint j(0); j < ret; ++j)
		{
			const vector_view<f32> dst
			{
				data(scratch) + (in.size() + j) * 768, 768
			};

			embed(data(dst), out[j], in.size() + j, *opts);
		}

		transform(data(scratch), tokens, *opts->model);

		const vector_view<f32> last_embed
		{
			data(scratch) + ((tokens - 1) * 768), 768
		};

		tail(logit, data(last_embed), *opts->model);
		out[i] = argmax(logit, *opts);

		for(uint j(0); j < 3; ++j)
		{
			errc[j] = out[i] == opts->error_code[j][errc[j]]? errc[j] + 1: 0;
			accc[j] = out[i] == opts->accept_code[j][accc[j]]? accc[j] + 1: 0;
		}

		for(uint j(0); j < 3; ++j)
		{
			halt |= errc[j] >= 3 || (errc[j] && opts->error_code[j][errc[j] + 1] == -1U);
			halt |= accc[j] >= 3 || (accc[j] && opts->accept_code[j][accc[j] + 1] == -1U);
		}

		++ret;
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
	static float
	buf alignas(64) [768];

	norm(buf, state, d.f.bias, d.f.weight, model::lnf_epsilon);
	logits(logit, buf, d);
	//logitsmax(logit, logit);
}

void
ircd::gpt::logits(float *const __restrict__ out,
                  const float (&__restrict__ in)[768],
                  const model::decoder &d)
{
	for(uint j(0); j < vocab::tokens; ++j)
		out[j] = 0;

	for(uint j(0); j < vocab::tokens; ++j)
		for(uint k(0); k < 768; ++k)
			out[j] += in[k] * d.wte[j][k];
}

void
ircd::gpt::logitsmax(float *const out,
                     const float *const in)
{
	static float
	exps alignas(64) [65536];

	for(uint j(0); j < vocab::tokens; ++j)
		exps[j] = exp(in[j]);

	for(uint j(0); j < vocab::tokens; ++j)
		out[j] = 0;

	for(uint j(0); j < vocab::tokens; ++j)
		for(uint k(0); k < vocab::tokens; ++k)
			out[k] += exps[j];

	for(uint j(0); j < vocab::tokens; ++j)
		out[j] = exps[j] / out[j];
}

[[gnu::noinline]]
void
ircd::gpt::transform(float *__restrict__ accum,
                     const size_t tokens,
                     const model::decoder &decoder)
{
	static float
	qkv alignas(64) [3][1024][12][64],
	state alignas(64) [12][1024][1024],
	attns alignas(64) [12][1024][64],
	buf alignas(64) [768];

	for(uint i(0); i < 12; ++i)
	{
		const auto &layer
		{
			decoder.layer[i]
		};

		ctrl(qkv, accum, tokens, layer);
		pare(state, qkv, tokens);
		mask(state, state, layer.attn.bias, tokens);
		smax(state, state, tokens);
		vals(attns, state, qkv, tokens);
		fmma(accum, attns, layer.attn.proj_bias, layer.attn.proj_weight, tokens);

		for(uint j(0); j < tokens; ++j)
		{
			for(uint k(0); k < 768; ++k)
				buf[k] = accum[j * 768 + k];

			ffnn(buf, buf, layer);
			for(uint k(0); k < 768; ++k)
				accum[j * 768 + k] += buf[k];
		}
	}
}

void
ircd::gpt::ffnn(float (&__restrict__ out)[768],
                const float (&__restrict__ in)[768],
                const model::block &layer)
{
	static float
	proj alignas(64) [3072];

	norm(out, in, layer.ln2.bias, layer.ln2.weight, model::ln2_epsilon);
	fmma(proj, out, layer.ffnn.fc_bias, layer.ffnn.fc_weight);
	gelu(proj, proj);
	fmma(out, proj, layer.ffnn.proj_bias, layer.ffnn.proj_weight);
}

void
ircd::gpt::ctrl(float (&__restrict__ out)[3][1024][12][64],
                const float *const __restrict__ in,
                const size_t num,
                const model::block &layer)
{
	float
	(&__restrict__ qry)[1024][12][64] { out[0] },
	(&__restrict__ key)[1024][12][64] { out[1] },
	(&__restrict__ val)[1024][12][64] { out[2] };

	for(uint i(0); i < num; ++i)
	{
		static float
		buf alignas(64) [768],
		proj alignas(64) [2304];

		for(uint j(0); j < 768; ++j)
			buf[j] = in[i * 768 + j];

		norm(buf, buf, layer.ln1.bias, layer.ln1.weight, model::ln1_epsilon);
		fmma(proj, buf, layer.attn.attn_bias, layer.attn.attn_weight);

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
ircd::gpt::smax(float (&__restrict__ out)[12][1024][1024],
                const float (&__restrict__ in)[12][1024][1024],
                const size_t num)
{
	static float
	exps alignas(64) [12][1024][1024];

	#pragma clang loop unroll (disable)
	for(uint j(0); j < 12; ++j)
		for(uint k(0); k < num; ++k)
			for(uint m(0); m < num; ++m)
				exps[j][k][m] = exp(in[j][k][m]);

	#pragma clang loop unroll (disable)
	for(uint j(0); j < 12; ++j)
		for(uint k(0); k < num; ++k)
			for(uint m(0); m < num; ++m)
				out[j][k][m] = 0;

	#pragma clang loop unroll (disable)
	for(uint j(0); j < 12; ++j)
		for(uint k(0); k < num; ++k)
			for(uint m(0); m < num; ++m)
				for(uint l(0); l < num; ++l)
					out[j][k][l] += exps[j][k][m];

	#pragma clang loop unroll (disable)
	for(uint j(0); j < 12; ++j)
		for(uint k(0); k < num; ++k)
			for(uint l(0); l < num; ++l)
				out[j][k][l] = exps[j][k][l] / out[j][k][l];
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
ircd::gpt::norm(float (&__restrict__ out)[768],
                const float *const in,
                const float (&__restrict__ bias)[768],
                const float (&__restrict__ weight)[768],
                const float epsilon)
{
	static float
	tmp alignas(64) [768];

	const float mean
	{
		math::mean<float>(vector_view<const float>{in, 768})
	};

	for(uint j(0); j < 768; ++j)
		tmp[j] = pow(in[j] - mean, 2);

	const float s
	{
		math::mean<float>(tmp)
	};

	for(uint j(0); j < 768; ++j)
		out[j] = (in[j] - mean) / sqrt(s + epsilon),
		out[j] = out[j] * weight[j] + bias[j];
}

void
ircd::gpt::fmma(float *const __restrict__ out,
                const float (&__restrict__ attn)[12][1024][64],
                const float (&__restrict__ bias)[768],
                const float (&__restrict__ weight)[768][768],
                const size_t num)
{
	static float
	a alignas(64) [1024][768],
	b alignas(64) [1024][768];

	for(uint k(0); k < 12; k++)
		for(uint j(0); j < num; j++)
			for(uint l(0); l < 64; l++)
				a[j][k * 64 + l] = attn[k][j][l];

	for(uint i(0); i < num; i++)
		for(uint j(0); j < 768; j++)
			b[i][j] = bias[j];

	for(uint i(0); i < num; i++)
		for(uint k(0); k < 768; k++)
			for(uint j(0); j < 768; j++)
				b[i][k] += a[i][j] * weight[j][k];

	for(uint i(0); i < num; i++)
		for(uint j(0); j < 768; j++)
			out[i * 768 + j] += b[i][j];
}

void
ircd::gpt::fmma(float (&__restrict__ out)[2304],
                const float (&__restrict__ in)[768],
                const float (&__restrict__ bias)[2304],
                const float (&__restrict__ weight)[768][2304])
{
	for(uint j(0); j < 2304; ++j)
		out[j] = bias[j];

	for(uint k(0); k < 768; ++k)
		for(uint j(0); j < 2304; ++j)
			out[j] += in[k] * weight[k][j];
}

void
ircd::gpt::fmma(float (&__restrict__ out)[768],
                const float (&__restrict__ in)[3072],
                const float (&__restrict__ bias)[768],
                const float (&__restrict__ weight)[3072][768])
{
	for(uint j(0); j < 768; ++j)
		out[j] = bias[j];

	for(uint k(0); k < 3072; k++)
		for(uint j(0); j < 768; j++)
			out[j] += in[k] * weight[k][j];
}

void
ircd::gpt::fmma(float (&__restrict__ out)[3072],
                const float (&__restrict__ in)[768],
                const float (&__restrict__ bias)[3072],
                const float (&__restrict__ weight)[768][3072])
{
	for(uint j(0); j < 3072; ++j)
		out[j] = bias[j];

	for(uint k(0); k < 768; ++k)
		for(uint j(0); j < 3072; ++j)
			out[j] += in[k] * weight[k][j];
}

void
ircd::gpt::gelu(float (&__restrict__ out)[3072],
                const float (&__restrict__ in)[3072])
{
	for(uint j(0); j < 3072; ++j)
		gelu(out[j], in[j]);
}

void
ircd::gpt::gelu(float &out,
                const float &in)
{
	out = 0.5 * in * (1.0 + tanh(in * 0.7978845608 * (1.0 + 0.044715 * in * in)));
}
