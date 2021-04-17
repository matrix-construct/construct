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
ircd_gpt_norm_fmad(__local float4 *const restrict out,
                   __local const float4 *const restrict in,
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
               const uint i)
{
	const uint
	lanes = 4;

	float4
	acc = bias[i];

	for(uint j = 0; j < height; ++j)
		for(uint k = 0; k < lanes; ++k)
			acc += in[j][k] * weight[width * (j * lanes + k) + i];

	out[i] = acc;
}

/// Gaussian Error Linear Unit
inline void
ircd_gpt_ffnn_gelu(__local float4 *const out,
                   __local const float4 *const in_,
                   const uint i)
{
	const float4
	in = in_[i];

	float4 a;
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

inline void
__attribute__((always_inline))
ircd_gpt_ffnn_fcon(__global const struct ircd_gpt_task *const ctrl,
                   __constant const struct ircd_gpt_opts *const opts,
                   __local union ircd_gpt_ffnn_aperaturev *const restrict out,
                   __local const union ircd_gpt_tokenv *const in,
                   __global const float4 *const restrict bias,
                   __global const float4 *const restrict weight)
{
	const uint
	li = get_local_id(0),
	ln = get_local_size(0),
	width = opts->ffnn_width,
	height = opts->ffnn_height;

	for(uint i = 0; i < opts->ffnn_mult; ++i)
		ircd_gpt_sgemv(out->fcon, in->word, bias, weight, width, height, i * ln + li);

	for(uint i = 0; i < opts->ffnn_mult; ++i)
		ircd_gpt_ffnn_gelu(out->fcon, out->fcon, i * ln + li);
}

inline void
__attribute__((always_inline))
ircd_gpt_ffnn(__global const struct ircd_gpt_task *const ctrl,
              __constant const struct ircd_gpt_opts *const opts,
              __local union ircd_gpt_tokenv *const restrict token,
              __local union ircd_gpt_tokenv *const restrict tmp,
              __local union ircd_gpt_ffnn_aperaturev *const restrict buf,
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
	wn = get_num_groups(0),
	width = opts->ffnn_width,
	height = opts->ffnn_height;

	// Layer re-normalization
	ircd_simt_math_norm_f4lldr(token->word, token->word, buf->word, ln, li);
	ircd_gpt_norm_fmad(tmp->word, token->word, norm_bias, norm_weight, li);

	// ln's writes are still pending but fcon reads results across threads.
	barrier(CLK_LOCAL_MEM_FENCE);

	// Fully connected
	ircd_gpt_ffnn_fcon(ctrl, opts, buf, tmp, fcon_bias, fcon_weight);

	// fcon's writes are still pending but proj reads results across threads.
	barrier(CLK_LOCAL_MEM_FENCE);

	// Projection
	ircd_gpt_sgemv(token->word, buf->fcon, proj_bias, proj_weight, height, width, li);
}

inline void
__attribute__((always_inline))
ircd_gpt_attn_self(__global const struct ircd_gpt_task *const ctrl,
                   __constant const struct ircd_gpt_opts *const opts,
                   __local union ircd_gpt_tokenv *const restrict out,
                   __local union ircd_gpt_tokenv *const restrict tmp,
                   __global const struct ircd_gpt_attn_qkvv *const restrict token,
                   __global const struct ircd_gpt_attn_mask *const restrict mask)   // [1024][1024],
{
	const uint
	gi = get_global_id(0),
	gn = get_global_size(0),
	li = get_local_id(0),
	ln = get_local_size(0),
	wi = get_group_id(0),
	wn = get_num_groups(0);

	__local union ircd_gpt_token *const restrict self = &tmp->token;

	for(uint i = 0; i < wn; ++i)
		if(mask[wi].token[i])
			self->attn[li][i] = 0.0f;
		else
			self->attn[li][i] = -10000.0f;

	for(uint i = 0; i < wn; ++i)
		if(mask[wi].token[i])
			for(uint j = 0; j < 64/4; ++j)
			{
				float4
				qry = token[wi].qry.attn[li][j],
				key = token[i].key.attn[li][j],
				res = qry * key;
				for(uint k = 0; k < 4; ++k)
					self->attn[li][i] += res[k];
			}

	for(uint i = 0; i < wn; ++i)
		if(mask[wi].token[i])
			self->attn[li][i] /= 8.0f;

	float mu = -10000.0f;
	for(uint i = 0; i < wn; ++i)
		mu = max(mu, self->attn[li][i]);

	for(uint i = 0; i < wn; ++i)
		self->attn[li][i] = exp(self->attn[li][i] - mu);

	float sum = 0.0f;
	for(uint i = 0; i < wn; ++i)
		sum += self->attn[li][i];

	const float lambda = 1.0f / sum;
	for(uint i = 0; i < wn; ++i)
		self->attn[li][i] *= lambda;

	for(uint j = 0; j < 64/4; ++j)
		out->attn[li][j] = 0.0f;

	for(uint i = 0; i < wn; ++i)
		for(uint j = 0; j < 64/4; ++j)
			out->attn[li][j] += token[i].val.attn[li][j] * self->attn[li][i];
}

inline void
__attribute__((always_inline))
ircd_gpt_attn_proj(__global const struct ircd_gpt_task *const ctrl,
                   __constant const struct ircd_gpt_opts *const opts,
                   __local union ircd_gpt_tokenv *const out,
                   __local const union ircd_gpt_tokenv *const xattn,
                   __global const float4 *const restrict bias,
                   __global const float4 *const restrict weight)
{
	const uint
	gi = get_global_id(0),
	gn = get_global_size(0),
	li = get_local_id(0),
	ln = get_local_size(0),
	wi = get_group_id(0),
	wn = get_num_groups(0),
	height = opts->attn_height,
	width = opts->attn_height;  // same

	// Projection
	ircd_gpt_sgemv(out->word, xattn->word, bias, weight, height, width, li);
}

__kernel void
ircd_gpt_coil(__global const struct ircd_gpt_task *const ctrl,
              __constant const struct ircd_gpt_opts *const opts,
              __global union ircd_gpt_tokenv *const restrict accum,
              __global const struct ircd_gpt_attn_qkvv *const restrict state,
              __global const struct ircd_gpt_attn_mask *const restrict mask,   // [1024][1024],
              __global 	const float4 *const restrict attn_proj_bias,
              __global const float4 *const restrict attn_proj_weight,
              __global const float4 *const restrict ffnn_norm_bias,
              __global const float4 *const restrict ffnn_norm_weight,
              __global const float4 *const restrict ffnn_fcon_bias,
              __global const float4 *const restrict ffnn_fcon_weight,
              __global const float4 *const restrict ffnn_proj_bias,
              __global const float4 *const restrict ffnn_proj_weight)
{
	const uint
	li = get_local_id(0),
	wi = get_group_id(0);

	__local union ircd_gpt_ffnn_aperaturev
	ffnn_fcon;

	__local union ircd_gpt_tokenv
	buf0, buf1;

	// Self-attention backend; this computes the self-attention result now
	// that keys and values are globally visible across tokens.
	ircd_gpt_attn_self
	(
		ctrl,
		opts,
		&buf1,
		&buf0,
		state,
		mask
	);

	// Self-attention's writes are pending on each thread but each proj
	// call requires results from all threads for input to the matmul.
	barrier(CLK_LOCAL_MEM_FENCE);

	// Project result of self-attention.
	ircd_gpt_attn_proj
	(
		ctrl,
		opts,
		&buf0,
		&buf1,
		attn_proj_bias,
		attn_proj_weight
	);

	// Frontend accumulation
	{
		const float4
		attn = buf0.word[li],
		resid = accum[wi].word[li];

		buf0.word[li] += resid;
		accum[wi].word[li] += attn;
	}

	// Backend mlp; layer-norm acquires any pending writes, no fence required.
	ircd_gpt_ffnn
	(
		ctrl,
		opts,
		&buf0,
		&buf1,
		&ffnn_fcon,
		ffnn_norm_bias,
		ffnn_norm_weight,
		ffnn_fcon_bias,
		ffnn_fcon_weight,
		ffnn_proj_bias,
		ffnn_proj_weight
	);

	// Backend accumulation
	{
		const float4
		ffnn = buf0.word[li],
		resid = accum[wi].word[li],
		result = ffnn + resid;

		accum[wi].word[li] = result;
	}
}

__kernel void
ircd_gpt_attn_fcon(__global const struct ircd_gpt_task *const ctrl,
                   __constant const struct ircd_gpt_opts *const opts,
                   __global union ircd_gpt_attn_aperaturev *const restrict state,
                   __global const union ircd_gpt_tokenv *const restrict accum,
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
	wn = get_num_groups(0),
	width = opts->attn_width,
	height = opts->attn_height;

	__local union ircd_gpt_attn_aperaturev
	token;

	__local float4
	tmp[768/4];

	token.word[li] = accum[wi].word[li];

	// Layer re-normalization
	ircd_simt_math_norm_f4lldr(token.word, token.word, tmp, ln, li);
	ircd_gpt_norm_fmad(tmp, token.word, norm_bias, norm_weight, li);

	// Ln's writes are still pending; fcon requires results across threads.
	barrier(CLK_LOCAL_MEM_FENCE);

	// Fully connected
	for(uint i = 0; i < opts->attn_mult; ++i)
		ircd_gpt_sgemv(token.fcon, tmp, fcon_bias, fcon_weight, width, height, i * ln + li);

	// Export queries, keys, and values.
	for(uint i = 0; i < opts->attn_mult; ++i)
		state[wi].proj[i][li] = token.proj[i][li];
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
	ring_idx = (ctrl->head + tok_idx) % opts->buffer_tokens,
	token = ctrl->token[ring_idx];

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
	ircd_gpt_norm_fmad(tmp.word, token.word, norm_bias, norm_weight, li);

	accum[wi].word[li] = tmp.word[li];
}

__kernel void
ircd_gpt_lm_logit(__global const struct ircd_gpt_task *const ctrl,
                  __constant const struct ircd_gpt_opts *const opts,
                  __global float *const restrict logit,
                  __global const union ircd_gpt_tokenv *const restrict accum,
                  __global const union ircd_gpt_tokenv *const restrict token)
{
	const uint
	gi = get_global_id(0),
	ti = ctrl->tokens - 1,
	words = opts->embed_width;

	float4 acc = 0.0f;
	__attribute__((opencl_unroll_hint))
	for(uint j = 0; j < words; ++j)
	{
		const float4
		in = accum[ti].word[j],
		vocab = token[gi].word[j];

		acc += vocab * in;
	}

	float res = 0.0f;
	for(uint k = 0; k < 4; ++k)
		res += acc[k];

	if(gi < opts->logits)
		logit[gi] = res;
	else
		logit[gi] = -10000.0f;
}

__kernel void
ircd_gpt_lm_logsm(__global struct ircd_gpt_task *const ctrl,
                  __constant const struct ircd_gpt_opts *const opts,
                  __global float4 *const restrict logsm,
                  __global float4 *const restrict logexp,
                  __global const float4 *const restrict logit)
{
	const uint
	gi = get_global_id(0),
	li = get_local_id(0),
	ln = get_local_size(0),
	logits = opts->logits,
	logits_alignup = logits + (ln - (logits % ln)),
	tn = logits_alignup / ln / 4,
	ti = tn * li;

	__local float share[256];
	__local float4 share4[256];

	share4[li] = -10000.0f;
	for(uint i = ti; i < ti + tn; ++i)
		share4[li] = max(share4[li], logit[i]);

	share[li] = -10000.0f;
	for(uint k = 0; k < 4; ++k)
		share[li] = max(share[li], share4[li][k]);

	ircd_simt_reduce_max_flldr(share, ln, li);

	if(li == 0)
		share4[li] = ctrl->samax_mu = share[li];

	ircd_simt_broadcast_f4lldr(share4, ln, li);

	const float4
	mu = share4[li];

	share4[li] = 0.0f;
	for(uint i = ti; i < ti + tn; ++i)
	{
		const float4
		reg = logit[i] - mu,
		res = exp(reg);

		for(uint k = 0; k < 4; ++k)
			if(i * 4 + k < logits)
				share4[li][k] += res[k];

		for(uint k = 0; k < 4; ++k)
			if(i * 4 + k < logits)
				logexp[i][k] = res[k];
			else
				logexp[i][k] = 0.0f;
	}

	ircd_simt_reduce_add_f4lldr(share4, ln, li);

	if(li == 0)
	{
		float sum = 0.0f;
		for(uint k = 0; k < 4; ++k)
			sum += share4[li][k];

		share4[li][0] = ctrl->samax_sum = sum;
		share4[li][1] = ctrl->samax_lambda = 1.0f / sum;
	}

	ircd_simt_broadcast_f4lldr(share4, ln, li);

	const float4
	sum = share4[li][0],
	lambda = share4[li][1];
	for(uint i = ti; i < ti + tn; ++i)
		for(uint k = 0; k < 4; ++k)
			if(i * 4 + k < logits)
				logsm[i] = logexp[i] * lambda;
			else
				logsm[i] = 0.0f;
}

inline void
__attribute__((always_inline))
ircd_gpt_leave(__global struct ircd_gpt_task *const ctrl,
               __constant const struct ircd_gpt_opts *const opts,
               const uint li)
{
	// If the call value has been set to something other than default we
	// do nothing else here.
	if(ctrl->call != IRCD_GPT_ECOMPLETE)
		return;

	// No action for other threads right now
	if(li != 0)
		return;

	// Run debug checks and assertions.
	#ifdef RB_DEBUG
	if(li == 0 && ctrl->call == IRCD_GPT_ECOMPLETE)
		if(ctrl->tokens < 2)
			ctrl->call = IRCD_GPT_ETOKENS;
	#endif

	// On the last cycle, with no prior call or error code set, indicate
	// a nominal exit condition.
	if(ctrl->cycle + 1 >= opts->limit)
	{
		ctrl->call = IRCD_GPT_ACCEPT;
		ctrl->epoch += 1;
	}

	ctrl->cycle += 1;
	ctrl->magic = 0xC7012C70U;
}

inline void
__attribute__((always_inline))
ircd_gpt_lm_result(__global struct ircd_gpt_task *const ctrl,
                   __constant const struct ircd_gpt_opts *const opts,
                   const uint li,
                   __local const ushort *const restrict idx,
                   __global const float *const restrict logsm,
                   __global const float *const restrict logexp,
                   __global const float *const restrict logit)
{
	// When the hypercall code is already set, bail here.
	if(ctrl->call != IRCD_GPT_ECOMPLETE)
		return;

	// To read from cells other than idx[0] we need this barrier.
	if(opts->top_k > 1)
		barrier(CLK_LOCAL_MEM_FENCE);

	// Mask for write-leader
	if(li != 0)
		return;

	const bool
	buffer_full = ctrl->tokens >= opts->buffer_tokens;

	const ulong
	rnd = opts->top_k > 1?
		ircd_simt_rand_xoshiro256pg(ctrl->rand): 1UL;

	const ushort
	entro = max(opts->top_k, 1U),
	select = rnd % entro,
	token = idx[select],
	dest = (ctrl->head + ctrl->tokens) % opts->buffer_tokens,
	tokens = min(ctrl->tokens + 1, opts->buffer_tokens),
	head = buffer_full?
		(ctrl->head + 1) % opts->buffer_tokens: ctrl->head;

	const ushort
	next_select = select + 1,
	next_token = idx[next_select];

	const float
	test_lsm = logexp[opts->label] * ctrl->samax_lambda,
	loss = 0.0f - log(test_lsm * ctrl->samax_lambda),
	perp = logsm[token] * 100.0f,
	cert = ((logsm[token] - logsm[next_token]) / logsm[token]) * 100.0f,
	loss_sum = ctrl->loss_sum + loss,
	perp_sum = ctrl->perp_sum + perp,
	cert_sum = ctrl->cert_sum + cert,
	mean_div = ctrl->epoch + 1.0f,
	loss_mean = loss_sum / mean_div,
	perp_mean = perp_sum / mean_div,
	cert_mean = cert_sum / mean_div;

	ctrl->loss = loss;
	ctrl->loss_sum = loss_sum;
	ctrl->loss_mean = loss_mean;
	ctrl->perp = perp;
	ctrl->perp_sum = perp_sum;
	ctrl->perp_mean = perp_mean;
	ctrl->cert = cert;
	ctrl->cert_sum = cert_sum;
	ctrl->cert_mean = cert_mean;
	ctrl->head = head;
	ctrl->tokens = tokens;
	ctrl->token[dest] = token;
}

__kernel void
ircd_gpt_lm_select(__global struct ircd_gpt_task *const ctrl,
                   __constant const struct ircd_gpt_opts *const opts,
                   __global const float *const restrict logsm,
                   __global const float *const restrict logexp,
                   __global const float *const restrict logit)
{
	const uint
	gi = get_global_id(0),
	gn = get_global_size(0),
	li = get_local_id(0),
	ln = get_local_size(0),
	wi = get_group_id(0),
	wn = get_num_groups(0),
	tn = opts->logits / ln,
	ti = tn * li;

	__local ushort idx[256];

	idx[li] = ti;
	for(uint j = ti + 1; j < ti + tn; ++j)
		if(logsm[j] > logsm[idx[li]])
			idx[li] = j;

	ircd_simt_sort_idx16_flldr(idx, logsm);
	ircd_gpt_lm_result(ctrl, opts, li, idx, logsm, logexp, logit);
	ircd_gpt_leave(ctrl, opts, li);
}

//
// backpropagations
//

inline void
ircd_gpt_prop_elem(__global const struct ircd_gpt_task *const ctrl,
                   __constant const struct ircd_gpt_opts *const opts,
                   __global float4 *const restrict param_,
                   __global float4 *const restrict exp_avg_,
                   __global float4 *const restrict exp_avg_sqr_)
{
	const uint
	li = get_local_id(0),
	step = ctrl->step;

	const float4
	param = param_[li],
	grad = ctrl->loss_mean,
	alpha[2] = { 1.0f - opts->beta[0], 1.0f - opts->beta[1], },
	exp_avg = step? exp_avg_[li]: 0.0f,
	exp_avg_sqr = step? exp_avg_sqr_[li]: 0.0f,
	exp_avg_mul = exp_avg * opts->beta[0],
	exp_avg_dot = exp_avg_mul + alpha[0] * grad,
	exp_avg_sqr_mul = exp_avg_sqr * opts->beta[1],
	exp_avg_sqr_dot = exp_avg_sqr_mul + alpha[1] * grad * grad,
	denom = sqrt(exp_avg_sqr_dot) + opts->epsilon,
	delta = opts->alpha * (exp_avg_dot / denom),
	update = param - delta;

	param_[li] = update;
	exp_avg_[li] = exp_avg_dot;
	exp_avg_sqr_[li] = exp_avg_sqr_dot;
}

__kernel void
ircd_gpt_norm_prop(__global const struct ircd_gpt_task *const ctrl,
                   __constant const struct ircd_gpt_opts *const opts,
                   __global union ircd_gpt_tokenv *const restrict bias,
                   __global union ircd_gpt_tokenv *const restrict bias_m0,
                   __global union ircd_gpt_tokenv *const restrict bias_m1,
                   __global union ircd_gpt_tokenv *const restrict weight,
                   __global union ircd_gpt_tokenv *const restrict weight_m0,
                   __global union ircd_gpt_tokenv *const restrict weight_m1)
{
	const uint
	gi = get_global_id(0),
	gn = get_global_size(0),
	li = get_local_id(0),
	ln = get_local_size(0),
	wi = get_group_id(0),
	wn = get_num_groups(0);

	ircd_gpt_prop_elem
	(
		ctrl, opts,
		bias->word,
		bias_m0->word,
		bias_m1->word
	);

	ircd_gpt_prop_elem
	(
		ctrl, opts,
		weight->word,
		weight_m0->word,
		weight_m1->word
	);
}

__kernel void
ircd_gpt_coil_prop_attn(__global const struct ircd_gpt_task *const ctrl,
                        __constant const struct ircd_gpt_opts *const opts,
                        __global union ircd_gpt_tokenv *const restrict norm_bias,
                        __global union ircd_gpt_tokenv *const restrict norm_bias_m0,
                        __global union ircd_gpt_tokenv *const restrict norm_bias_m1,
                        __global union ircd_gpt_tokenv *const restrict norm_weight,
                        __global union ircd_gpt_tokenv *const restrict norm_weight_m0,
                        __global union ircd_gpt_tokenv *const restrict norm_weight_m1,
                        __global union ircd_gpt_attn_aperaturev *const restrict fcon_bias,
                        __global union ircd_gpt_attn_aperaturev *const restrict fcon_bias_m0,
                        __global union ircd_gpt_attn_aperaturev *const restrict fcon_bias_m1,
                        __global union ircd_gpt_attn_aperaturev *const restrict fcon_weight,
                        __global union ircd_gpt_attn_aperaturev *const restrict fcon_weight_m0,
                        __global union ircd_gpt_attn_aperaturev *const restrict fcon_weight_m1,
                        __global union ircd_gpt_tokenv *const restrict proj_bias,
                        __global union ircd_gpt_tokenv *const restrict proj_bias_m0,
                        __global union ircd_gpt_tokenv *const restrict proj_bias_m1,
                        __global union ircd_gpt_tokenv *const restrict proj_weight,
                        __global union ircd_gpt_tokenv *const restrict proj_weight_m0,
                        __global union ircd_gpt_tokenv *const restrict proj_weight_m1)
{
	const uint
	gi = get_global_id(0),
	gn = get_global_size(0),
	li = get_local_id(0),
	ln = get_local_size(0),
	wi = get_group_id(0),
	wn = get_num_groups(0);

	ircd_gpt_norm_prop
	(
		ctrl, opts,
		norm_bias,
		norm_bias_m0,
		norm_bias_m1,
		norm_weight,
		norm_weight_m0,
		norm_weight_m1
	);

	for(uint j = 0; j < 3; ++j)
		ircd_gpt_prop_elem
		(
			ctrl, opts,
			fcon_bias->proj[j],
			fcon_bias_m0->proj[j],
			fcon_bias_m1->proj[j]
		);

	for(uint i = 0; i < 768; ++i)
		for(uint j = 0; j < 3; ++j)
			ircd_gpt_prop_elem
			(
				ctrl, opts,
				fcon_weight[i].proj[j],
				fcon_weight_m0[i].proj[j],
				fcon_weight_m1[i].proj[j]
			);

	ircd_gpt_prop_elem
	(
		ctrl, opts,
		proj_bias->word,
		proj_bias_m0->word,
		proj_bias_m1->word
	);

	for(uint i = 0; i < 768; ++i)
		ircd_gpt_prop_elem
		(
			ctrl, opts,
			proj_weight[i].word,
			proj_weight_m0[i].word,
			proj_weight_m1[i].word
		);
}

__kernel void
ircd_gpt_coil_prop_ffnn(__global const struct ircd_gpt_task *const ctrl,
                        __constant const struct ircd_gpt_opts *const opts,
                        __global union ircd_gpt_tokenv *const restrict norm_bias,
                        __global union ircd_gpt_tokenv *const restrict norm_bias_m0,
                        __global union ircd_gpt_tokenv *const restrict norm_bias_m1,
                        __global union ircd_gpt_tokenv *const restrict norm_weight,
                        __global union ircd_gpt_tokenv *const restrict norm_weight_m0,
                        __global union ircd_gpt_tokenv *const restrict norm_weight_m1,
                        __global union ircd_gpt_ffnn_aperaturev *const restrict fcon_bias,
                        __global union ircd_gpt_ffnn_aperaturev *const restrict fcon_bias_m0,
                        __global union ircd_gpt_ffnn_aperaturev *const restrict fcon_bias_m1,
                        __global union ircd_gpt_ffnn_aperaturev *const restrict fcon_weight,
                        __global union ircd_gpt_ffnn_aperaturev *const restrict fcon_weight_m0,
                        __global union ircd_gpt_ffnn_aperaturev *const restrict fcon_weight_m1,
                        __global union ircd_gpt_tokenv *const restrict proj_bias,
                        __global union ircd_gpt_tokenv *const restrict proj_bias_m0,
                        __global union ircd_gpt_tokenv *const restrict proj_bias_m1,
                        __global union ircd_gpt_tokenv *const restrict proj_weight,
                        __global union ircd_gpt_tokenv *const restrict proj_weight_m0,
                        __global union ircd_gpt_tokenv *const restrict proj_weight_m1)
{
	const uint
	gi = get_global_id(0),
	gn = get_global_size(0),
	li = get_local_id(0),
	ln = get_local_size(0),
	wi = get_group_id(0),
	wn = get_num_groups(0);

	ircd_gpt_norm_prop
	(
		ctrl, opts,
		norm_bias,
		norm_bias_m0,
		norm_bias_m1,
		norm_weight,
		norm_weight_m0,
		norm_weight_m1
	);

	for(uint j = 0; j < 4; ++j)
		ircd_gpt_prop_elem
		(
			ctrl, opts,
			fcon_bias->proj[j],
			fcon_bias_m0->proj[j],
			fcon_bias_m1->proj[j]
		);

	for(uint i = 0; i < 768; ++i)
		for(uint j = 0; j < 4; ++j)
			ircd_gpt_prop_elem
			(
				ctrl, opts,
				fcon_weight[i].proj[j],
				fcon_weight_m0[i].proj[j],
				fcon_weight_m1[i].proj[j]
			);

	ircd_gpt_prop_elem
	(
		ctrl, opts,
		proj_bias->word,
		proj_bias_m0->word,
		proj_bias_m1->word
	);

	for(uint i = 0; i < 3072; ++i)
		ircd_gpt_prop_elem
		(
			ctrl, opts,
			proj_weight[i].word,
			proj_weight_m0[i].word,
			proj_weight_m1[i].word
		);
}

__kernel void
ircd_gpt_lm_embed_prop(__global const struct ircd_gpt_task *const ctrl,
                       __constant const struct ircd_gpt_opts *const opts,
                       __global union ircd_gpt_tokenv *const restrict pos,
                       __global union ircd_gpt_tokenv *const restrict pos_m0,
                       __global union ircd_gpt_tokenv *const restrict pos_m1,
                       __global union ircd_gpt_tokenv *const restrict token,
                       __global union ircd_gpt_tokenv *const restrict token_m0,
                       __global union ircd_gpt_tokenv *const restrict token_m1)
{
	const uint
	gi = get_global_id(0),
	gn = get_global_size(0),
	li = get_local_id(0),
	ln = get_local_size(0),
	wi = get_group_id(0),
	wn = get_num_groups(0),
	cn = opts->context_tokens / wn,
	ci = cn * wi,
	tn = opts->logits / wn,
	ti = tn * wi;

	for(uint i = ci; i < ci + cn; ++i)
		ircd_gpt_prop_elem
		(
			ctrl, opts,
			pos[i].word,
			pos_m0[i].word,
			pos_m1[i].word
		);

	for(uint i = ti; i < ti + tn; ++i)
		ircd_gpt_prop_elem
		(
			ctrl, opts,
			token[i].word,
			token_m0[i].word,
			token_m1[i].word
		);
}
