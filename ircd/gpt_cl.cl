// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.


inline void
ircd_gpt_norm_fmad(__local float4 *const out,
                   __local const float4 *const in,
                   __global const float4 *const restrict bias,
                   __global const float4 *const restrict weight,
                   const uint i)
{
	out[i] = in[i] * weight[i] + bias[i];
}

// Matrix * Vector Multiply/Accumulate
inline void
ircd_gpt_sgemv(__local float4 *const restrict out,
               __local const float4 *const restrict in,
               __global const float4 *const restrict bias,
               __global const float4 *const restrict weight,
               const uint width,
               const uint height,
               const uint tiles,
               const uint i)
{
	const uint seg = height / tiles;

	float4 acc = bias[i];
	for(uint j = 0; j < seg; ++j)
		for(uint t = 0; t < tiles; ++t)
			for(uint k = 0; k < 4; ++k)
			{
				const uint
				jidx = t * seg + j,
				kidx = jidx * 4 + k,
				widx = kidx * width + i;

				acc += weight[widx] * in[jidx][k];
			}

	out[i] = acc;
}

inline void
ircd_gpt_gelu(__local float4 *const out,
              __local const float4 *const in_,
              const uint i)
{
	float4 a,
	in = in_[i];

	a = 0.044715f;
	a *= in;
	a *= in;
	a += 1.0f;
	a *= 0.7978845608f;
	a *= in;

	a = tanh(a);
	a += 1.0f;
	a *= in;
	a *= 0.5f;

	out[i] = a;
}

//
// core
//

__kernel void
ircd_gpt_ffnn(__global const struct ircd_gpt_task *const ctrl,
              __constant const struct ircd_gpt_opts *const opts,
              __global union ircd_gpt_tokenv *const restrict accum,
              __global const float4 *const restrict norm_bias,
              __global const float4 *const restrict norm_weight,
              __global const float4 *const restrict fcon_bias,
              __global const float4 *const restrict fcon_weight,
              __global const float4 *const restrict proj_bias,
              __global const float4 *const restrict proj_weight)
{
	const uint
	gi = get_global_id(0),
	gn = get_global_size(0),
	li = get_local_id(0),
	ln = get_local_size(0),
	wi = get_group_id(0),
	wn = get_num_groups(0);

	__local union ircd_gpt_aperaturev token;
	__local float4 tmp[768/4];

	// Fetch local copy of the global accumulator. We operate on a cached
	// copy as input, and add our output to the global upon completion.
	token.word[li] = accum[wi].word[li];

	// Layer re-normalization
	ircd_simt_math_norm_f4lldr(token.word, token.word, tmp, ln, li);
	ircd_gpt_norm_fmad(tmp, token.word, norm_bias, norm_weight, li);

	// Fully connected
	for(uint i = 0; i < 4; ++i)
		ircd_gpt_sgemv(token.fcon, tmp, fcon_bias, fcon_weight, 3072/4, 768/4, 4, i * ln + li);

	// Gaussian Error Linear Unit
	for(uint i = 0; i < 4; ++i)
		ircd_gpt_gelu(token.fcon, token.fcon, i * ln + li);

	// Projection
	ircd_gpt_sgemv(tmp, token.fcon, proj_bias, proj_weight, 768/4, 3072/4, 4, li);

	// Accumulation; end of layer
	accum[wi].word[li] += tmp[li];
}

__kernel void
ircd_gpt_attn_proj(__global const struct ircd_gpt_task *const ctrl,
                   __constant const struct ircd_gpt_opts *const opts,
                   __global union ircd_gpt_tokenv *const restrict accum,
                   __local const union ircd_gpt_tokenv *const restrict xattn,
                   __global const float4 *const restrict proj_bias,
                   __global const float4 *const restrict proj_weight)
{
	const uint
	gi = get_global_id(0),
	gn = get_global_size(0),
	li = get_local_id(0),
	ln = get_local_size(0),
	wi = get_group_id(0),
	wn = get_num_groups(0);

	__local float4
	in[768/4],
	out[768/4];

	// Fetch
	in[li] = xattn->word[li];

	// Need this here if xattn is __local
	barrier(CLK_LOCAL_MEM_FENCE);

	// Projection
	ircd_gpt_sgemv(out, in, proj_bias, proj_weight, 768/4, 768/4, 1, li);

	// Accumulation; end of layer
	accum[wi].word[li] += out[li];
}

__kernel void
ircd_gpt_attn_self(__global const struct ircd_gpt_task *const ctrl,
                   __constant const struct ircd_gpt_opts *const opts,
                   __local union ircd_gpt_tokenv *const restrict out,
                   __global const struct ircd_gpt_qkvv *const restrict token,
                   __global const struct ircd_gpt_attn_mask *const restrict mask)   // [1024][1024],
{
	const uint
	gi = get_global_id(0),
	gn = get_global_size(0),
	li = get_local_id(0),
	ln = get_local_size(0),
	wi = get_group_id(0),
	wn = get_num_groups(0);

	__local union
	{
		float
		attn[12][96];
	}
	self;

	for(uint i = 0; i < wn; ++i)
		if(mask[wi].token[i])
			self.attn[li][i] = 0.0f;
		else
			self.attn[li][i] = -10000.0f;

	for(uint i = 0; i < wn; ++i)
		if(mask[wi].token[i])
			for(uint j = 0; j < 64/4; ++j)
			{
				float4
				qry = token[wi].qry.attn[li][j],
				key = token[i].key.attn[li][j],
				res = qry * key;
				for(uint k = 0; k < 4; ++k)
					self.attn[li][i] += res[k];
			}

	for(uint i = 0; i < wn; ++i)
		if(mask[wi].token[i])
			self.attn[li][i] /= 8.0f;

	for(uint i = 0; i < wn; ++i)
		self.attn[li][i] = exp(self.attn[li][i]);

	float4 vacc = 0.0f;
	for(uint i = 0; i < wn; ++i)
		vacc[i % 4] += self.attn[li][i];

	float acc = 0.0f;
	for(uint i = 0; i < 4; ++i)
		acc += vacc[i];

	for(uint i = 0; i < wn; ++i)
		self.attn[li][i] /= acc;

	for(uint j = 0; j < 64/4; ++j)
		out->attn[li][j] = 0.0f;

	for(uint i = 0; i < wn; ++i)
		for(uint j = 0; j < 64/4; ++j)
			out->attn[li][j] += token[i].val.attn[li][j] * self.attn[li][i];
}

__kernel void
ircd_gpt_attn_fcon(__global const struct ircd_gpt_task *const ctrl,
                   __constant const struct ircd_gpt_opts *const opts,
                   __global union ircd_gpt_aperaturev *const restrict out,
                   __global const union ircd_gpt_tokenv *const restrict in,
                   __global const float4 *const restrict norm_bias,
                   __global const float4 *const restrict norm_weight,
                   __global const float4 *const restrict fcon_bias,
                   __global const float4 *const restrict fcon_weight)
{
	const uint
	gi = get_global_id(0),
	gn = get_global_size(0),
	li = get_local_id(0),
	ln = get_local_size(0),
	wi = get_group_id(0),
	wn = get_num_groups(0);

	__local union ircd_gpt_aperaturev token;
	__local float4 tmp[768/4];

	token.word[li] = in[wi].word[li];

	// Layer re-normalization
	ircd_simt_math_norm_f4lldr(token.word, token.word, tmp, ln, li);
	ircd_gpt_norm_fmad(tmp, token.word, norm_bias, norm_weight, li);

	// Fully connected
	for(uint i = 0; i < 3; ++i)
		ircd_gpt_sgemv(token.fcon, tmp, fcon_bias, fcon_weight, 2304/4, 768/4, 4, i * ln + li);

	// Export queries, keys, and values.
	for(uint i = 0; i < 3; ++i)
		out[wi].proj[i][li] = token.proj[i][li];
}

__kernel void
ircd_gpt_coil(__global const struct ircd_gpt_task *const ctrl,
              __constant const struct ircd_gpt_opts *const opts,
              __global union ircd_gpt_tokenv *const restrict accum,
              __global const struct ircd_gpt_qkvv *const restrict state,
              __global const struct ircd_gpt_attn_mask *const restrict mask,   // [1024][1024],
              __global const float4 *const restrict attn_proj_bias,
              __global const float4 *const restrict attn_proj_weight,
              __global const float4 *const restrict ffnn_norm_bias,
              __global const float4 *const restrict ffnn_norm_weight,
              __global const float4 *const restrict ffnn_fcon_bias,
              __global const float4 *const restrict ffnn_fcon_weight,
              __global const float4 *const restrict ffnn_proj_bias,
              __global const float4 *const restrict ffnn_proj_weight)
{
	__local union ircd_gpt_tokenv value;

	ircd_gpt_attn_self
	(
		ctrl,
		opts,
		&value,
		state,
		mask
	);

	ircd_gpt_attn_proj
	(
		ctrl,
		opts,
		accum,
		&value,
		attn_proj_bias,
		attn_proj_weight
	);

	ircd_gpt_ffnn
	(
		ctrl,
		opts,
		accum,
		ffnn_norm_bias,
		ffnn_norm_weight,
		ffnn_fcon_bias,
		ffnn_fcon_weight,
		ffnn_proj_bias,
		ffnn_proj_weight
	);
}

//
// frontend
//

inline void
_ircd_gpt_lm_embed(__global const struct ircd_gpt_task *const ctrl,
                   __constant const struct ircd_gpt_opts *const opts,
                   __global union ircd_gpt_tokenv *const restrict out,
                   __global const union ircd_gpt_tokenv *const restrict pos,
                   __global const union ircd_gpt_tokenv *const restrict vocab,
                   const uint out_idx,
                   const uint tok_idx,
                   const uint word_idx)
{
	const ushort
	token = ctrl->token[(ctrl->head + tok_idx) % opts->buffer_tokens];

	const float4
	wte = vocab[token].word[word_idx],
	wpe = pos[tok_idx].word[word_idx];

	out[out_idx].word[word_idx] = wte + wpe;
}

__kernel void
ircd_gpt_lm_embed(__global const struct ircd_gpt_task *const ctrl,
                  __constant const struct ircd_gpt_opts *const opts,
                  __global union ircd_gpt_tokenv *const restrict accum,
                  __global const union ircd_gpt_tokenv *const restrict pos,
                  __global const union ircd_gpt_tokenv *const restrict vocab)
{
	const uint
	li = get_local_id(0);

	for(uint i = 0; i < ctrl->tokens; ++i)
		_ircd_gpt_lm_embed(ctrl, opts, accum, pos, vocab, i, i, li);
}

__kernel void
ircd_gpt_lm_norm(__global const struct ircd_gpt_task *const ctrl,
                 __constant const struct ircd_gpt_opts *const opts,
                 __global union ircd_gpt_tokenv *const restrict accum,
                 __global const float4 *const restrict norm_bias,
                 __global const float4 *const restrict norm_weight)
{
	const uint
	li = get_local_id(0),
	ln = get_local_size(0),
	wi = get_global_offset(0) / ln + get_group_id(0);

	__local union ircd_gpt_tokenv
	token, tmp;

	token.word[li] = accum[wi].word[li];

	// Final re-normalization
	ircd_simt_math_norm_f4lldr(token.word, token.word, tmp.word, ln, li);
	ircd_gpt_norm_fmad(token.word, token.word, norm_bias, norm_weight, li);

	accum[0].word[li] = token.word[li];
}

__kernel void
ircd_gpt_lm_logit(__global const struct ircd_gpt_task *const ctrl,
                  __constant const struct ircd_gpt_opts *const opts,
                  __global float *const restrict logit,
                  __global const union ircd_gpt_tokenv *const restrict accum,
                  __global const union ircd_gpt_tokenv *const restrict token)
{
	const uint
	gi = get_global_id(0);

	float4 acc = 0.0f;
	for(uint j = 0; j < 768/4; ++j)
	{
		const float4
		in = accum[0].word[j],
		vocab = token[gi].word[j],
		res = vocab * in;

		acc += res;
	}

	float res = 0.0f;
	for(uint k = 0; k < 4; ++k)
		res += acc[k];

	logit[gi] = res;
}

inline void
ircd_gpt_leave(__global struct ircd_gpt_task *const ctrl,
               __constant const struct ircd_gpt_opts *const opts,
               const uint li)
{
	// No action for other threads right now
	if(li != 0)
		return;

	// Run debug checks and assertions.
	#ifdef RB_DEBUG
	if(li == 0 && ctrl->call == IRCD_GPT_ECOMPLETE)
		if(ctrl->tokens < 2)
			ctrl->call = IRCD_GPT_ETOKENS;
	#endif

	// If the call value has been set to something other than default we
	// do nothing else here.
	if(ctrl->call != IRCD_GPT_ECOMPLETE)
		return;

	// On the last cycle, with no prior call or error code set, indicate
	// a nominal exit condition.
	if(ctrl->cycle + 1 >= opts->limit)
	{
		ctrl->call = IRCD_GPT_ACCEPT;
		ctrl->epoch += 1;
	}

	ctrl->cycle += 1;
}

inline void
ircd_gpt_lm_result(__global struct ircd_gpt_task *const ctrl,
                   __constant const struct ircd_gpt_opts *const opts,
                   const uint li,
                   __local const ushort *const restrict idx)
{
	// To read from cells other than idx[0] we need this barrier.
	if(opts->top_k > 1)
		barrier(CLK_LOCAL_MEM_FENCE);

	// No action for other threads right now
	if(li != 0)
		return;

	// When the hypercall code is already set, bail here.
	if(ctrl->call != IRCD_GPT_ECOMPLETE)
		return;

	const bool
	buffer_full = ctrl->tokens >= opts->buffer_tokens;

	const ulong
	rnd = ircd_simt_rand_xoshiro256pg(ctrl->rand),
	sel = rnd % max(opts->top_k, 1U);

	const ushort
	token = idx[sel],
	token_idx = (ctrl->head + ctrl->tokens) % opts->buffer_tokens;

	ctrl->token[token_idx] = token;

	if(buffer_full)
		ctrl->head = (ctrl->head + 1) % opts->buffer_tokens;
	else
		ctrl->tokens++;
}

__kernel void
ircd_gpt_lm_select(__global struct ircd_gpt_task *const ctrl,
                   __constant const struct ircd_gpt_opts *const opts,
                   __global const float *const restrict logit)
{
	const uint
	gi = get_global_id(0),
	gn = get_global_size(0),
	li = get_local_id(0),
	ln = get_local_size(0),
	wi = get_group_id(0),
	wn = get_num_groups(0),
	tn = 262,
	ti = tn * li;

	__local ushort idx[192];

	idx[li] = ti;
	for(uint j = ti + 1; j < ti + tn && j < 50257; ++j)
		if(logit[j] > logit[idx[li]])
			idx[li] = j;

	ircd_simt_sort_idx16_flldr(idx, logit, ln, li);
	ircd_gpt_lm_result(ctrl, opts, li, idx);
	ircd_gpt_leave(ctrl, opts, li);
}
