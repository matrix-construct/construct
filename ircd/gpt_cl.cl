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
ctor_local_bcast_ldr(__local float4 *const out,
                     const uint ln,
                     const uint li)
{
	for(uint stride = 1; stride < ln; stride <<= 1)
	{
		if(li < stride)
			out[li + stride] = out[li];

		barrier(CLK_LOCAL_MEM_FENCE);
	}
}

inline void
ctor_local_reduce_add_ldr(__local float4 *const out,
                          const uint ln,
                          const uint li)
{
	for(uint stride = ln >> 1; stride > 0; stride >>= 1)
	{
		barrier(CLK_LOCAL_MEM_FENCE);

		if(li < stride)
			out[li] += out[li + stride];
	}
}

inline void
ctor_local_reduce_max_ldr(__local float *const out,
                          const uint ln,
                          const uint li)
{
	for(uint stride = ln >> 1; stride > 0; stride >>= 1)
	{
		barrier(CLK_LOCAL_MEM_FENCE);

		if(li < stride)
			out[li] = max(out[li], out[li + stride]);
	}
}

inline void
ctor_local_reduce_tournament_ldr(__local float *const best,
                                 __local ushort *const idx,
                                 const uint ln,
                                 const uint li)
{
	for(uint stride = ln >> 1; stride > 0; stride >>= 1)
	{
		barrier(CLK_LOCAL_MEM_FENCE);

		if(li < stride && best[li] < best[li + stride])
		{
			best[li] = best[li + stride];
			idx[li] = idx[li + stride];
		}
	}
}

inline void
ctor_mean(__local float4 *const restrict out,
          __local const float4 *const restrict in,
          const uint num,
          const uint i)
{
	out[i] = in[i];
	ctor_local_reduce_add_ldr(out, num, i);

	float numerator = 0.0f;
	float4 numeratorv = out[i];
	for(uint k = 0; k < 4; ++k)
		numerator += numeratorv[k];

	out[i] = numerator;
	ctor_local_bcast_ldr(out, num, i);

	numeratorv = out[i];
	out[i] = numeratorv / (num * 4);
}

inline void
ctor_norm(__local float4 *const out,
          __local const float4 *const in,
          __local float4 *const restrict tmp,
          const uint num,
          const uint i)
{
	ctor_mean(tmp, in, num, i);

	const float4
	sub_mean = in[i] - tmp[i];

	tmp[i] = pow(sub_mean, 2);
	ctor_mean(out, tmp, num, i);

	const float4
	epsilon = 0.00001f,
	s = sqrt(out[i] + epsilon);

	out[i] = sub_mean / s;
}

inline void
ctor_norm_fmad(__local float4 *const out,
               __local const float4 *const in,
               __global const float4 *const restrict bias,
               __global const float4 *const restrict weight,
               const uint i)
{
	out[i] = in[i] * weight[i] + bias[i];
}

// Matrix * Vector Multiply/Accumulate
inline void
ctor_sgemv(__local float4 *const restrict out,
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
ctor_gelu(__local float4 *const out,
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
ctor_attn_fcon(__global const struct ctor_ctrl *const ctrl,
               __constant const struct ctor_opts *const opts,
               __global union aperaturev *const restrict out,
               __global const union tokenv *const restrict in,
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

	__local union aperaturev token;
	__local float4 tmp[768/4];

	token.word[li] = in[wi].word[li];

	// Layer re-normalization
	ctor_norm(token.word, token.word, tmp, ln, li);
	ctor_norm_fmad(tmp, token.word, norm_bias, norm_weight, li);

	// Fully connected
	for(uint i = 0; i < 3; ++i)
		ctor_sgemv(token.fcon, tmp, fcon_bias, fcon_weight, 2304/4, 768/4, 4, i * ln + li);

	// Export queries, keys, and values.
	for(uint i = 0; i < 3; ++i)
		out[wi].proj[i][li] = token.proj[i][li];
}

__kernel void
ctor_attn_proj(__global const struct ctor_ctrl *const ctrl,
               __constant const struct ctor_opts *const opts,
               __global union tokenv *const restrict accum,
               __global const union tokenv *const restrict xattn,
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
	in[li] = xattn[wi].word[li];

	// Projection
	ctor_sgemv(out, in, proj_bias, proj_weight, 768/4, 768/4, 1, li);

	// Accumulation; end of layer
	accum[wi].word[li] += out[li];
}

__kernel void
ctor_ffnn(__global const struct ctor_ctrl *const ctrl,
          __constant const struct ctor_opts *const opts,
          __global union tokenv *const restrict accum,
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

	__local union aperaturev token;
	__local float4 tmp[768/4];

	// Fetch local copy of the global accumulator. We operate on a cached
	// copy as input, and add our output to the global upon completion.
	token.word[li] = accum[wi].word[li];

	// Layer re-normalization
	ctor_norm(token.word, token.word, tmp, ln, li);
	ctor_norm_fmad(tmp, token.word, norm_bias, norm_weight, li);

	// Fully connected
	for(uint i = 0; i < 4; ++i)
		ctor_sgemv(token.fcon, tmp, fcon_bias, fcon_weight, 3072/4, 768/4, 4, i * ln + li);

	// Gaussian Error Linear Unit
	for(uint i = 0; i < 4; ++i)
		ctor_gelu(token.fcon, token.fcon, i * ln + li);

	// Projection
	ctor_sgemv(tmp, token.fcon, proj_bias, proj_weight, 768/4, 3072/4, 4, li);

	// Accumulation; end of layer
	accum[wi].word[li] += tmp[li];
}

__kernel void
ctor_backend(__global const struct ctor_ctrl *const ctrl,
             __constant const struct ctor_opts *const opts,
             __global union tokenv *const restrict accum,
             __global const union tokenv *const restrict xattn,
             __global const float4 *const restrict attn_proj_bias,
             __global const float4 *const restrict attn_proj_weight,
             __global const float4 *const restrict ffnn_norm_bias,
             __global const float4 *const restrict ffnn_norm_weight,
             __global const float4 *const restrict ffnn_fcon_bias,
             __global const float4 *const restrict ffnn_fcon_weight,
             __global const float4 *const restrict ffnn_proj_bias,
             __global const float4 *const restrict ffnn_proj_weight)
{
	ctor_attn_proj
	(
		ctrl,
		opts,
		accum,
		xattn,
		attn_proj_bias,
		attn_proj_weight
	);

	ctor_ffnn
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
// ctrl
//

__kernel void
ctor_attn_self(__global const struct ctor_ctrl *const ctrl,
               __constant const struct ctor_opts *const opts,
               __global union tokenv *const restrict out,
               __global const struct qkvv *const restrict token,
               __global const struct attn_mask *const restrict mask)   // [1024][1024],
{
	__local struct
	{
		float
		attn[12][32];
	}
	self;

	const uint
	gi = get_global_id(0),
	gn = get_global_size(0),
	li = get_local_id(0),
	ln = get_local_size(0),
	wi = get_group_id(0),
	wn = get_num_groups(0);

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
		out[wi].attn[li][j] = 0.0f;

	for(uint i = 0; i < wn; ++i)
		for(uint j = 0; j < 64/4; ++j)
			out[wi].attn[li][j] += token[i].val.attn[li][j] * self.attn[li][i];
}

//
// leads
//

__kernel void
ctor_anode0(__global const struct ctor_ctrl *const ctrl,
            __constant const struct ctor_opts *const opts,
            __global union tokenv *const restrict accum,
            __global const union tokenv *const restrict pos,
            __global const union tokenv *const restrict vocab)
{
	const uint
	li = get_local_id(0),
	wi = get_group_id(0);

	const ushort
	token = ctrl->body.token[wi];

	const float4
	wte = vocab[token].word[li],
	wpe = pos[wi].word[li];

	accum[wi].word[li] = wte + wpe;
}

__kernel void
ctor_anode1(__global const struct ctor_ctrl *const ctrl,
            __constant const struct ctor_opts *const opts,
            __global union tokenv *const restrict accum,
            __global const union tokenv *const restrict pos,
            __global const union tokenv *const restrict vocab)
{
	const uint
	li = get_local_id(0);

	for(uint i = 0; i < ctrl->tokens; ++i)
	{
		const ushort
		token = ctrl->body.token[i];

		const float4
		wte = vocab[token].word[li],
		wpe = pos[i].word[li];

		accum[i].word[li] = wte + wpe;
	}
}

__kernel void
ctor_anode2(__global const struct ctor_ctrl *const ctrl,
            __constant const struct ctor_opts *const opts,
            __global union tokenv *const restrict accum,
            __global const union tokenv *const restrict pos,
            __global const union tokenv *const restrict vocab)
{
	const uint
	gi = get_global_id(0);

	const ushort
	token = ctrl->body.token[gi];

	for(uint i = 0; i < 768/4; ++i)
	{
		const float4
		wte = vocab[token].word[i],
		wpe = pos[gi].word[i];

		accum[gi].word[i] = wte + wpe;
	}
}

__kernel void
ctor_cathode(__global const struct ctor_ctrl *const ctrl,
             __constant const struct ctor_opts *const opts,
             __global union tokenv *const restrict accum,
             __global const float4 *const restrict norm_bias,
             __global const float4 *const restrict norm_weight)
{
	const uint
	li = get_local_id(0),
	ln = get_local_size(0),
	wi = get_global_offset(0) / ln + get_group_id(0);

	__local union tokenv
	token, tmp;

	token.word[li] = accum[wi].word[li];

	// Final re-normalization
	ctor_norm(token.word, token.word, tmp.word, ln, li);
	ctor_norm_fmad(token.word, token.word, norm_bias, norm_weight, li);

	accum[0].word[li] = token.word[li];
}

__kernel void
ctor_lmhead(__global const struct ctor_ctrl *const ctrl,
            __constant const struct ctor_opts *const opts,
            __global float *const restrict logit,
            __global const union tokenv *const restrict accum,
            __global const union tokenv *const restrict token)
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

__kernel void
ctor_lmamax(__global struct ctor_ctrl *const ctrl,
            __constant const struct ctor_opts *const opts,
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
	__local float best[192];

	idx[li] = ti;
	for(uint j = ti + 1; j < ti + tn && j < 50257; ++j)
		if(logit[j] > logit[idx[li]])
			idx[li] = j;

	best[li] = logit[idx[li]];
	ctor_local_reduce_tournament_ldr(best, idx, ln, li);

	if(li == 0 && ctrl->call == -1)
		ctrl->body.token[ctrl->tokens++] = idx[li];

	if(li == 0 && ctrl->call == -1)
		ctrl->call = 1;

	#ifdef RB_DEBUG
	if(li == 0 && ctrl->call == 1)
		if(ctrl->tokens < 2)
			ctrl->call = -2;
	#endif
}
