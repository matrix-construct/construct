// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

//#pragma OPENCL EXTENSION cl_amd_device_attribute_query : enable
//#pragma OPENCL EXTENSION cl_khr_byte_addressable_store :enable
//#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable
//#pragma OPENCL EXTENSION cl_khr_local_int32_base_atomics : enable
//#pragma OPENCL EXTENSION cl_khr_global_int32_extended_atomics : enable
//#pragma OPENCL EXTENSION cl_khr_local_int32_extended_atomics : enable

#pragma OPENCL EXTENSION cl_clang_storage_class_specifiers : enable
#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_global_int32_extended_atomics : enable
#pragma OPENCL EXTENSION cl_khr_local_int32_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_local_int32_extended_atomics : enable

#pragma clang fp exceptions(ignore)
#pragma clang fp reassociate(on)
#pragma clang fp contract(fast)

#include <ircd/config.h>
#include <clc/clc.h>

#define __region __attribute__((address_space(0x02)))

#if !defined(assume)
	#define assume(x) __builtin_assume(x)
#endif

#if defined(__SPIR)
	#define restrict
#elif defined(__cplusplus)
	#define restrict __restrict
#endif

#if __OPENCL_VERSION__ < 120
	#define static __attribute__((internal_linkage))
#else
	#define static __constant static
#endif

#pragma clang attribute push(__attribute__((always_inline)), apply_to = function)
#pragma clang attribute push(__attribute__((internal_linkage)), apply_to = function)
#include <ircd/simt/simt.h>
#include <ircd/gpt/vector.h>
#include <ircd/gpt/opts.h>
#include <ircd/gpt/ctrl.h>
#pragma clang attribute pop
#pragma clang attribute pop

#if __OPENCL_VERSION__ >= 120
	#undef static
#endif

#include <ircd/gpt/gpu.h>

//
// head
//

__kernel void
__attribute__((visibility("protected")))
__attribute__((reqd_work_group_size(192, 1, 1)))
__attribute__((amdgpu_flat_work_group_size(192, 192)))
ircd_gpt_alloc(__global const void *const restrict model,
               __global void *const restrict master,
               __constant const void *const opts,
               __global void *const restrict ctrl,
               __global void *const restrict frame0,
               __global void *const restrict frame1,
               __global void *const restrict frame2,
               __global void *const restrict frame3,
               __global void *const restrict frame4,
               __global void *const restrict frame5,
               __global void *const restrict frame6,
               __global void *const restrict frame7)
{
}

__kernel void
__attribute__((visibility("protected")))
__attribute__((reqd_work_group_size(192, 1, 1)))
__attribute__((amdgpu_flat_work_group_size(192, 192)))
ircd_gpt_enter(__global const void *const restrict model,
               __global void *const restrict state,
               __global void *const restrict master,
               __constant const struct ircd_gpt_opts *const opts,
               __global struct ircd_gpt_ctrl *const restrict ctrl)
{
	const ushort
	gi = get_global_id(0),
	li = get_local_id(0),
	ln = get_local_size(0),
	cycle = ctrl->clk.cycle;

	if(li == 0)
		;//ctrl->prof.entered = __builtin_readcyclecounter();
}

__kernel void
__attribute__((vec_type_hint(float4)))
__attribute__((reqd_work_group_size(192, 1, 1)))
__attribute__((amdgpu_flat_work_group_size(192, 192)))
ircd_gpt_lm_embed(__global const struct ircd_gpt_ctrl *const ctrl,
                  __constant const struct ircd_gpt_opts *const opts,
                  __global ircd_gpt_vectorv *const restrict accum,
                  __global const ircd_gpt_vectorv *const restrict pos,
                  __global const ircd_gpt_vectorv *const restrict vocab)
{
	const ushort
	li = get_local_id(0),
	ln = get_local_size(0);

	const uint
	wo = get_global_offset(0);

	assume(ln == 192);
	assume(wo % ln == 0);

	const ushort
	wi = wo / ln + get_group_id(0);

	_ircd_gpt_lm_embed(ctrl, opts, accum, pos, vocab, wi, wi, li);
}

static void
_ircd_gpt_lm_embed(__global const struct ircd_gpt_ctrl *const ctrl,
                   __constant const struct ircd_gpt_opts *const opts,
                   __global ircd_gpt_vectorv *const restrict accum,
                   __global const ircd_gpt_vectorv *const restrict pos,
                   __global const ircd_gpt_vectorv *const restrict vocab,
                   const ushort out_idx,
                   const ushort tok_idx,
                   const ushort elem_idx)
{
	const ushort
	token = ctrl->token[tok_idx];

	const float4
	wpe = pos[tok_idx].elem[elem_idx],
	wte = vocab[token].elem[elem_idx],
	res = wte + wpe;

	accum[out_idx].elem[elem_idx] = res;
}

//
// Frontside
//

void
ircd_gpt_ffnn_fcon_tmul(__constant const struct ircd_gpt_opts *const opts,
                        __local ircd_gpt_ffnn_aperaturev *const restrict out,
                        __local const ircd_gpt_vectorv *const restrict in,
                        __global const ircd_gpt_ffnn_aperaturev *const restrict bias,
                        __global const ircd_gpt_ffnn_aperaturev *const restrict weight,
                        const uint li)
{
	const uint
	lanes = 4,
	segs = ircd_gpt_ffnn_segs,
	height = ircd_gpt_vector_elems / lanes;

	assume(height > 0);
	assume(height % lanes == 0);

	for(uint x = 0; x < segs; ++x)
		out->proj[x][li] = bias->proj[x][li];

	for(uint y = 0; y < height; ++y)
		for(uint k = 0; k < lanes; ++k)
			for(uint x = 0; x < segs; ++x)
			{
				const uint
				row = y * lanes + k;

				out->proj[x][li] += in->elem[y][k] * weight[row].proj[x][li];
			}
}

void
ircd_gpt_ffnn_fcon(__global const struct ircd_gpt_ctrl *const ctrl,
                   __constant const struct ircd_gpt_opts *const opts,
                   __local ircd_gpt_ffnn_aperaturev *const restrict out,
                   __local const ircd_gpt_vectorv *const restrict in,
                   __global const ircd_gpt_ffnn_aperaturev *const restrict bias,
                   __global const ircd_gpt_ffnn_aperaturev *const restrict weight,
                   const uint ln,
                   const uint li)
{
	const uint
	segs = ircd_gpt_ffnn_segs;

	// Fully connected
	ircd_gpt_ffnn_fcon_tmul
	(
		opts,
		out,
		in,
		bias,
		weight,
		li
	);

	for(uint i = 0; i < segs; ++i)
		ircd_gpt_ffnn_gelu(out, out, i * ln + li);
}

void
ircd_gpt_ffnn_proj_tmul(__constant const struct ircd_gpt_opts *const opts,
                        __local ircd_gpt_vectorv *const restrict out,
                        __local const ircd_gpt_ffnn_aperaturev *const restrict in,
                        __global const ircd_gpt_vectorv *const restrict bias,
                        __global const ircd_gpt_vectorv *const restrict weight,
                        const uint li)
{
	const uint
	lanes = 4,
	height = ircd_gpt_ffnn_fcon_elems / lanes;

	assume(height > 0);
	assume(height % lanes == 0);

	out->elem[li] = bias->elem[li];

	for(uint y = 0; y < height; ++y)
		for(uint k = 0; k < lanes; ++k)
		{
			const uint
			row = y * lanes + k;

			out->elem[li] += in->fcon[y][k] * weight[row].elem[li];
		}
}

void
ircd_gpt_ffnn(__global const struct ircd_gpt_ctrl *const ctrl,
              __constant const struct ircd_gpt_opts *const opts,
              __local ircd_gpt_vectorv *const restrict token,
              __local ircd_gpt_ffnn_aperaturev *const restrict buf,
              __local ircd_gpt_vectorv *const restrict tmp,
              __global const ircd_gpt_vectorv *const restrict norm_bias,
              __global const ircd_gpt_vectorv *const restrict norm_weight,
              __global const ircd_gpt_ffnn_aperaturev *const restrict fcon_bias,
              __global const ircd_gpt_ffnn_aperaturev *const restrict fcon_weight,
              __global const ircd_gpt_vectorv *const restrict proj_bias,
              __global const ircd_gpt_vectorv *const restrict proj_weight,
              const uint ln,
              const uint li)
{
	// Layer re-normalization
	ircd_gpt_norm(token, token, tmp, norm_bias, norm_weight, ln, li);

	// ln's writes are still pending but fcon reads results across threads.
	barrier(CLK_LOCAL_MEM_FENCE);

	// Fully connected
	ircd_gpt_ffnn_fcon
	(
		ctrl,
		opts,
		buf,
		token,
		fcon_bias,
		fcon_weight,
		ln,
		li
	);

	// fcon's writes are still pending but proj reads results across threads.
	barrier(CLK_LOCAL_MEM_FENCE);

	// Projection
	ircd_gpt_ffnn_proj_tmul
	(
		opts,
		token,
		buf,
		proj_bias,
		proj_weight,
		li
	);
}

static void
ircd_gpt_attn_self_samax(__global const struct ircd_gpt_ctrl *const ctrl,
                         __constant const struct ircd_gpt_opts *const opts,
                         __local float self[restrict][12],
                         const uint ln,
                         const uint li,
                         const uint wn,
                         const uint wi)
{
	struct ircd_math_samax samax =
	{
		.mu = -10000.0f,
		.sum = 0.0f,
	};

	__attribute__((opencl_unroll_hint))
	for(uint i = 0; i < wn; ++i)
		samax.mu = max(samax.mu, self[i][li]);

	__attribute__((opencl_unroll_hint))
	for(uint i = 0; i < wn; ++i)
		self[i][li] -= samax.mu;

	for(uint i = 0; i < wn; ++i)
		self[i][li] = native_exp(self[i][li]);

	__attribute__((opencl_unroll_hint))
	for(uint i = 0; i < wn; ++i)
		samax.sum += self[i][li];

	samax.sum += FLT_EPSILON;
	samax.lambda = 1.0f / samax.sum;

	__attribute__((opencl_unroll_hint))
	for(uint i = 0; i < wn; ++i)
		self[i][li] *= samax.lambda;
}

static void
ircd_gpt_attn_self_keys(__global const struct ircd_gpt_ctrl *const ctrl,
                        __constant const struct ircd_gpt_opts *const opts,
                        __local float self[restrict][ircd_gpt_attn_rank],
                        __global const ircd_gpt_attn_qkvv *const restrict token,
                        const uint ln,
                        const uint li,
                        const uint wi,
                        const uint kn,
                        const uint i)
{
	assume(i < wi);

	self[i][li] = 0.0f;

	__attribute__((opencl_unroll_hint))
	for(uint k = 0; k < kn; ++k)
	{
		float4
		qry = token[wi].qry.attn[li][k],
		key = token[i].key.attn[li][k],
		res = qry * key;

		self[i][li] += ircd_simt_reduce_add_f4(res);
	}

	self[i][li] /= 8.0f;
}

static void
ircd_gpt_attn_self_vals(__global const struct ircd_gpt_ctrl *const ctrl,
                        __constant const struct ircd_gpt_opts *const opts,
                        __local ircd_gpt_vectorv *const restrict out,
                        __local const float self[restrict][ircd_gpt_attn_rank],
                        __global const ircd_gpt_attn_qkvv *const restrict token,
                        const uint li,
                        const uint wi,
                        const uint ki,
                        const uint ti)
{
	out->attn[ti][ki] = 0.0f;

	__attribute__((opencl_unroll_hint))
	for(uint i = 0; i < wi; ++i)
	{
		const float4
		val = token[i].val.attn[ti][ki],
		attn = self[i][ti],
		res = attn * val;

		out->attn[ti][ki] += res;
	}
}

static void
ircd_gpt_attn_self(__global struct ircd_gpt_ctrl *const ctrl,
                   __constant const struct ircd_gpt_opts *const opts,
                   __local ircd_gpt_vectorv *const restrict out,
                   __local float self[restrict][ircd_gpt_attn_rank],
                   __global float attns[restrict][ircd_gpt_attn_rank],
                   __global const ircd_gpt_attn_qkvv *const restrict token,
                   const uint ln,
                   const uint li,
                   const uint wi)
{
	//assume(opts->attn_rank == sizeof(self[0]) / sizeof(float));
	assume(opts->attn_rank == ircd_gpt_attn_rank);
	assume(ctrl->count < ircd_gpt_context_tokens);
	assume(ctrl->tokens <= ircd_gpt_context_tokens);
	assume(ctrl->tokens > wi);
	assume(ctrl->tokens > 0);

	const uint
	wn = ctrl->tokens,
	kn = ln / opts->attn_rank,
	ki = li / opts->attn_rank,
	ti = li % opts->attn_rank;

	// Low-rank mask
	if(li < opts->attn_rank)
	{
		// Left attention
		uint i;
		for(i = 0; i < wi; ++i)
			ircd_gpt_attn_self_keys(ctrl, opts, self, token, ln, li, wi, kn, i);

		// Future mask
		__attribute__((opencl_unroll_hint))
		while(i < wn)
			self[i++][li] = -10000.0f;

		// Three-piece softmax
		ircd_gpt_attn_self_samax(ctrl, opts, self, ln, li, wn, wi);
	}

	// Propagate to full width for value dot prod.
	barrier(CLK_LOCAL_MEM_FENCE);
	ircd_gpt_attn_self_vals(ctrl, opts, out, self, token, li, wi, ki, ti);

	// Save softmax results for later analysis/observation.
	if(li < opts->attn_rank)
	{
		__attribute__((opencl_unroll_hint))
		for(uint i = 0; i < wn; ++i)
			attns[i][li] = self[i][li];
	}
}

static void
ircd_gpt_attn_proj_tmul(__constant const struct ircd_gpt_opts *const opts,
                        __local ircd_gpt_vectorv *const restrict out,
                        __local const ircd_gpt_vectorv *const restrict in,
                        __global const ircd_gpt_vectorv *const restrict bias,
                        __global const ircd_gpt_vectorv *const restrict weight,
                        const uint li)
{
	const uint
	lanes = 4,
	height = ircd_gpt_vector_elems / 4;

	assume(height > 0);
	assume(height % lanes == 0);

	out->elem[li] = bias->elem[li];

	for(uint y = 0; y < height; ++y)
		for(uint k = 0; k < lanes; ++k)
		{
			const uint
			row = y * lanes + k;

			const float4
			a = in->elem[y][k],
			b = weight[row].elem[li];

			out->elem[li] += a * b;
		}
}

__kernel void
__attribute__((vec_type_hint(float4)))
__attribute__((reqd_work_group_size(192, 1, 1)))
__attribute__((amdgpu_flat_work_group_size(192, 192)))
__attribute__((visibility("protected")))
ircd_gpt_coil(__global struct ircd_gpt_ctrl *const ctrl,
              __constant const struct ircd_gpt_opts *const opts,
              __private const uint layer,
              __global ircd_gpt_vectorv *const restrict accum,
              __global float attns[restrict][ircd_gpt_attn_rank],
              __global const ircd_gpt_attn_qkvv *const restrict state,
              __global const ircd_gpt_vectorv *const restrict attn_proj_bias,
              __global const ircd_gpt_vectorv *const restrict attn_proj_weight,
              __global const ircd_gpt_vectorv *const restrict ffnn_norm_bias,
              __global const ircd_gpt_vectorv *const restrict ffnn_norm_weight,
              __global const ircd_gpt_ffnn_aperaturev *const restrict ffnn_fcon_bias,
              __global const ircd_gpt_ffnn_aperaturev *const restrict ffnn_fcon_weight,
              __global const ircd_gpt_vectorv *const restrict ffnn_proj_bias,
              __global const ircd_gpt_vectorv *const restrict ffnn_proj_weight)
{
	const uint
	li = get_local_id(0),
	ln = get_local_size(0),
	wo = get_global_offset(0),
	wi = wo / ln + get_group_id(0);

	assume(ln == 192);
	assume(wo % ln == 0);

	__local union
	{
		float
		attn_self[ircd_gpt_context_tokens][ircd_gpt_attn_rank];

		ircd_gpt_ffnn_aperaturev
		ffnn_fcon[2];

		ircd_gpt_vectorv
		vector[8];
	}
	buf;

	__local ircd_gpt_vectorv
	buf0, buf1,
	*const restrict attn_self = &buf1,
	*const restrict token = &buf0,
	*const restrict tmp = &buf1;

	// Self-attention backend; this computes the self-attention result now
	// that keys and values are globally visible across tokens.
	ircd_gpt_attn_self
	(
		ctrl,
		opts,
		attn_self,
		buf.attn_self,
		attns,
		state,
		ln,
		li,
		wi
	);

	barrier(CLK_LOCAL_MEM_FENCE);

	// Project result of self-attention.
	ircd_gpt_attn_proj_tmul
	(
		opts,
		token,
		attn_self,
		attn_proj_bias,
		attn_proj_weight,
		li
	);

	// Frontend accumulation
	{
		const float4
		attn = token->elem[li],
		resid = accum[wi].elem[li],
		result = resid + attn;

		token->elem[li] = result;
		accum[wi].elem[li] = result;
	}

	// Backend mlp; layer-norm acquires any pending writes, no fence required.
	ircd_gpt_ffnn
	(
		ctrl,
		opts,
		token,
		buf.ffnn_fcon,
		tmp,
		ffnn_norm_bias,
		ffnn_norm_weight,
		ffnn_fcon_bias,
		ffnn_fcon_weight,
		ffnn_proj_bias,
		ffnn_proj_weight,
		ln,
		li
	);

	// Backend accumulation
	{
		const float4
		ffnn = token->elem[li],
		resid = accum[wi].elem[li],
		result = resid + ffnn;

		accum[wi].elem[li] = result;
	}
}

static void
ircd_gpt_attn_fcon_tmul(__constant const struct ircd_gpt_opts *const opts,
                        __local ircd_gpt_attn_aperaturev *const restrict out,
                        __local const ircd_gpt_vectorv *const restrict in,
                        __global const ircd_gpt_attn_aperaturev *const restrict bias,
                        __global const ircd_gpt_attn_aperaturev *const restrict weight,
                        const uint ln,
                        const uint li)
{
	const uint
	lanes = 4,
	segs = ircd_gpt_attn_segs,
	height = ircd_gpt_vector_elems / lanes;

	assume(height > 0);
	assume(height % segs == 0);
	assume(height % lanes == 0);

	for(uint x = 0; x < segs; ++x)
		out->proj[x][li] = bias->proj[x][li];

	for(uint y = 0; y < height; ++y)
		for(uint k = 0; k < lanes; ++k)
			for(uint x = 0; x < segs; ++x)
			{
				const uint
				row = y * lanes + k;

				const float4
				a = in->elem[y][k],
				b = weight[row].proj[x][li];

				out->proj[x][li] += a * b;
			}
}

__kernel void
__attribute__((vec_type_hint(float4)))
__attribute__((reqd_work_group_size(192, 1, 1)))
__attribute__((amdgpu_flat_work_group_size(192, 192)))
ircd_gpt_attn_fcon(__global const struct ircd_gpt_ctrl *const ctrl,
                   __constant const struct ircd_gpt_opts *const opts,
                   __private const uint layer,
                   __global ircd_gpt_attn_aperaturev *const restrict state,
                   __global const ircd_gpt_vectorv *const restrict accum,
                   __global const ircd_gpt_vectorv *const restrict norm_bias,
                   __global const ircd_gpt_vectorv *const restrict norm_weight,
                   __global const ircd_gpt_attn_aperaturev *const restrict fcon_bias,
                   __global const ircd_gpt_attn_aperaturev *const restrict fcon_weight)
{
	const uint
	li = get_local_id(0),
	ln = get_local_size(0),
	wo = get_global_offset(0),
	wi = wo / ln + get_group_id(0),
	segs = ircd_gpt_attn_segs;

	assume(ln == 192);
	assume(wo % ln == 0);

	__local ircd_gpt_attn_aperaturev
	attn;

	__local ircd_gpt_vectorv
	token, *const restrict tmp = attn.vector;

	token.elem[li] = accum[wi].elem[li];

	// Layer re-normalization
	ircd_gpt_norm(&token, &token, tmp, norm_bias, norm_weight, ln, li);

	// Ln's writes are still pending; fcon requires results across threads.
	barrier(CLK_LOCAL_MEM_FENCE);

	// Fully connected
	ircd_gpt_attn_fcon_tmul
	(
		opts,
		&attn,
		&token,
		fcon_bias,
		fcon_weight,
		ln,
		li
	);

	// Export queries, keys, and values.
	for(uint x = 0; x < segs; ++x)
		state[wi].proj[x][li] = attn.proj[x][li];
}

__kernel void
__attribute__((vec_type_hint(float4)))
__attribute__((reqd_work_group_size(192, 1, 1)))
__attribute__((amdgpu_flat_work_group_size(192, 192)))
ircd_gpt_lm_norm(__global const struct ircd_gpt_ctrl *const ctrl,
                 __constant const struct ircd_gpt_opts *const opts,
                 __global ircd_gpt_vectorv *const restrict accum,
                 __global const ircd_gpt_vectorv *const restrict norm_bias,
                 __global const ircd_gpt_vectorv *const restrict norm_weight)
{
	const uint
	li = get_local_id(0),
	ln = get_local_size(0),
	wo = get_global_offset(0),
	wi = wo / ln + get_group_id(0);

	assume(ln == 192);
	assume(wo % ln == 0);

	__local ircd_gpt_vectorv
	tmp, token;

	token.elem[li] = accum[wi].elem[li];

	// Final re-normalization
	ircd_gpt_norm(&token, &token, &tmp, norm_bias, norm_weight, ln, li);

	accum[wi].elem[li] = token.elem[li];
}

__kernel void
ircd_gpt_lm_logit(__global const struct ircd_gpt_ctrl *const ctrl,
                  __constant const struct ircd_gpt_opts *const opts,
                  __global float *const restrict logit,
                  __global const ircd_gpt_vectorv *const restrict accum,
                  __global const ircd_gpt_vectorv *const restrict pos,
                  __global const ircd_gpt_vectorv *const restrict vocab)
{
	const uint
	gi = get_global_id(0),
	wi = ctrl->count - 1;

	assume(opts->embed_width == 192);
	assume(opts->logits <= 65536);

	if(gi >= opts->logits)
	{
		logit[gi] = -10000.0f;
		return;
	}

	float acc = 0.0f;
	for(uint j = 0; j < opts->embed_width; ++j)
	{
		const float4
		token = vocab[gi].elem[j],
		in = accum[wi].elem[j],
		wpe = pos[wi].elem[j],
		res = in * token + wpe;

		acc += ircd_simt_reduce_add_f4(res);
	}

	logit[gi] = acc;
}

__kernel void
__attribute__((reqd_work_group_size(256, 1, 1)))
__attribute__((amdgpu_flat_work_group_size(256, 256)))
ircd_gpt_lm_logsm(__global struct ircd_gpt_ctrl *const ctrl,
                  __constant const struct ircd_gpt_opts *const opts,
                  __global float logit[restrict 65536])
{
	const uint
	li = get_local_id(0),
	ln = get_local_size(0),
	//wo = get_global_offset(0),
	//wi = wo / ln + get_group_id(0),
	wn = 50432,
	tn = wn / ln,
	start = tn * li,
	stop = min(start + tn, opts->logits);

	__local float
	mu[256], sum[256], lambda[256];

	__local struct ircd_math_samax
	samax;

	assume(ln == 256);

	mu[li] = -10000.0f;
	__attribute__((opencl_unroll_hint))
	for(uint ti = start; ti < stop; ++ti)
		mu[li] = max(mu[li], logit[ti]);

	ircd_simt_reduce_max_flldr(mu, ln, li);

	if(li == 0)
		samax.mu = mu[li];

	sum[li] = 0.0f;
	for(uint ti = start; ti < stop; ++ti)
	{
		const float
		sub = logit[ti] - samax.mu,
		res = native_exp(sub);

		sum[li] += res;
	}

	ircd_simt_reduce_add_flldr(sum, ln, li);

	if(li == 0)
		sum[li] += FLT_EPSILON,
		samax.sum = sum[li],
		samax.lambda = lambda[li] = 1.0f / sum[li];

	ircd_simt_broadcast_flldr(lambda, ln, li);

	for(uint ti = start; ti < stop; ++ti)
	{
		const float
		sub = logit[ti] - samax.mu,
		res = lambda[li] * native_exp(sub);

		logit[ti] = res;
	}
}

void
ircd_gpt_lm_result_top(__local struct ircd_gpt_ctrl *const ctrl,
                       __constant const struct ircd_gpt_opts *const opts,
                       __local const ushort *const restrict idx,
                       __global const float *const restrict logsm,
                       const uint i)
{
	const ushort
	token = idx[i];

	const float
	samax = logsm[token] + FLT_EPSILON;

	ctrl->top[i].token = token;
	ctrl->top[i].samax = samax;
}

void
ircd_gpt_lm_result_label_mean(__local struct ircd_gpt_ctrl *const ctrl,
                              __constant const struct ircd_gpt_opts *const opts,
                              __local struct ircd_math_mean *const mean,
                              const float last)
{
	const uint
	div = mean->div + 1,
	sum_sel = mean->div % 4;

	const float
	sum = mean->sum[0] + mean->sum[1] + mean->sum[2] + mean->sum[3] + last,
	res = sum / div;

	mean->sum[sum_sel] += last;
	mean->div = div;
	mean->last = last;
	mean->mean = res;
}

void
ircd_gpt_lm_result_label(__local struct ircd_gpt_ctrl *const ctrl,
                         __constant const struct ircd_gpt_opts *const opts,
                         __local struct ircd_gpt_ctrl_label *const label,
                         __global const float *const restrict logsm)
{
	const ushort
	token = label->logit.token;

	const float
	samax = logsm[token] + FLT_EPSILON,
	loss = 0.0f - native_log(samax),
	ppl = (1.0f - samax) * native_log2(opts->logits);

	label->logit.samax = samax;
	ircd_gpt_lm_result_label_mean(ctrl, opts, &label->loss, loss);
	ircd_gpt_lm_result_label_mean(ctrl, opts, &label->ppl, ppl);
}

ushort
ircd_gpt_lm_result_select(__local struct ircd_gpt_ctrl *const ctrl,
                          __constant const struct ircd_gpt_opts *const opts,
                          __local const ushort *const restrict idx,
                          __global const float *const restrict logsm)
{
	const ulong
	ent_k = max(opts->top_k, 1U) - 1,
	rnd = ircd_simt_rand_xoshiro256pl(ctrl->rand);

	const float
	ent_p = min(max(opts->top_p, 0.0f), 1.0f),
	thresh = ent_p;

	float acc = 1.0f;
	ushort select = 0;
	for(; select < ent_k; ++select)
		if((acc -= logsm[idx[select]]) < thresh)
			break;

	const ushort
	token = idx[select];

	return token;
}

static ushort
ircd_gpt_lm_result(__local struct ircd_gpt_ctrl *const ctrl,
                   __constant const struct ircd_gpt_opts *const opts,
                   __local const ushort *const restrict idx,
                   __global const float *const restrict logsm)
{
	const ushort
	token = ircd_gpt_lm_result_select(ctrl, opts, idx, logsm);

	// Update the dynamic result label.
	ctrl->select.logit.token = token;
	ircd_gpt_lm_result_label(ctrl, opts, &ctrl->select, logsm);

	// Update the dynamic target label.
	ctrl->target.logit.token = ctrl->count < ctrl->tokens?
		ctrl->token[ctrl->count]:
		ctrl->select.logit.token;

	ircd_gpt_lm_result_label(ctrl, opts, &ctrl->target, logsm);

	const bool
	hit = ctrl->select.logit.token == ctrl->target.logit.token;

	// Update the token context.
	if(ctrl->count == ctrl->tokens)
	{
		ctrl->token[ctrl->count] = ctrl->select.logit.token;
		ctrl->tokens++;
	}

	ctrl->miss += !hit;
	ctrl->hit += hit;
	ctrl->count++;
	return token;
}

static void
ircd_gpt_lm_result_attns(__local struct ircd_gpt_ctrl *const ctrl,
                         __constant const struct ircd_gpt_opts *const opts,
                         __global const float *const restrict attns,
                         const uint ln,
                         const uint li)
{
	const uint
	layer = li / opts->layers,
	head = li % opts->attn_rank,
	base = layer * opts->attn_self_elems;

	uint best = 0;
	float bestv = 10000.0f;
	for(uint i = 0; i < ctrl->count; ++i)
	{
		const uint
		bx = (((i + 1) * i) / 2) * opts->attn_rank,
		idx = base + bx + i * 12 + head;

		if(attns[idx] < bestv)
			bestv = attns[idx],
			best = i;
	}

	ctrl->attn[layer][head] = best;
}

__kernel void
__attribute__((reqd_work_group_size(256, 1, 1)))
__attribute__((amdgpu_flat_work_group_size(256, 256)))
__attribute__((visibility("protected")))
ircd_gpt_lm_select(__global struct ircd_gpt_ctrl *const restrict ctrl_,
                   __constant const struct ircd_gpt_opts *const opts,
                   __global const float logsm[restrict 65536],
                   __global const float *const restrict attns)
{
	const uint
	li = get_local_id(0),
	ln = get_local_size(0),
	logits_pad = ln - (opts->logits % ln),
	tn = (opts->logits + logits_pad) / ln,
	start = tn * li,
	stop = min(start + tn, opts->logits);

	__local ushort idx[256];
	__local struct ircd_gpt_ctrl ctrl;
	__private event_t event[1];

	assume(ln == 256);
	assume(start < stop);

	event[0] = async_work_group_copy
	(
		(__local char16 *)&ctrl,
		(__global const char16 *)ctrl_,
		sizeof(struct ircd_gpt_ctrl) / sizeof(char16),
		0
	);

	idx[li] = start;
	for(uint j = start + 1; j < stop; ++j)
		if(logsm[j] > logsm[idx[li]])
			idx[li] = j;

	ircd_simt_sort_idx16_flldr(idx, logsm, ln, li);
	wait_group_events(1, event);

	if(ctrl.count >= opts->buffer_tokens)
		return;

	if(li < opts->top_n)
		ircd_gpt_lm_result_top(&ctrl, opts, idx, logsm, li);

	if(li < opts->labels)
		ircd_gpt_lm_result_label(&ctrl, opts, ctrl.label + li, logsm);

	if(li < opts->layers * opts->attn_rank)
		ircd_gpt_lm_result_attns(&ctrl, opts, attns, ln, li);

	barrier(CLK_LOCAL_MEM_FENCE);

	if(li == 0)
		ircd_gpt_lm_result(&ctrl, opts, idx, logsm);

	barrier(CLK_LOCAL_MEM_FENCE);

	event[0] = async_work_group_copy
	(
		(__global char16 *)ctrl_,
		(__local const char16 *)&ctrl,
		sizeof(struct ircd_gpt_ctrl) / sizeof(char16),
		0
	);

	wait_group_events(1, event);
}

__kernel void
__attribute__((visibility("protected")))
__attribute__((reqd_work_group_size(256, 1, 1)))
__attribute__((amdgpu_flat_work_group_size(256, 256)))
ircd_gpt_leave(__global const void *const restrict model,
               __global void *const restrict state,
               __global void *const restrict master,
               __constant const struct ircd_gpt_opts *const opts,
               __global struct ircd_gpt_ctrl *const ctrl_,
               __global struct ircd_gpt_ctrl *const frame)
{
	const ushort
	li = get_local_id(0),
	ln = get_local_size(0);

	assume(ln == 256);

	__local struct ircd_gpt_ctrl _ctrl;
	__local struct ircd_gpt_ctrl *const ctrl = &_ctrl;

	if(li == 0)
		*ctrl = *ctrl_;

	barrier(CLK_LOCAL_MEM_FENCE);

	if(li == 0 && ctrl->accept < 0)
		ircd_gpt_accept(ctrl, opts);

	barrier(CLK_LOCAL_MEM_FENCE);

	const uint
	batch_size = opts->batch_size,
	samps = opts->training_steps + opts->validation_steps + opts->testing_steps,
	steps = samps / batch_size;

	const bool
	accepting = ctrl->accept >= 0,
	cycling = !accepting,
	sampling = accepting,
	stepping = sampling && (ctrl->clk.samp + 1) >= batch_size,
	epoching = stepping && (ctrl->clk.step + 1) >= steps;

	if(li == 0)
		;//ctrl->prof.finished = __builtin_readcyclecounter();

	if(li == 0)
		*frame = *ctrl;

	if(!accepting && li == 0)
	{
		ctrl->clk.cycle += cycling;
		ctrl->clk.samp += sampling;
		ctrl->clk.step += stepping;
		ctrl->clk.epoch += epoching;
	}
}

void
ircd_gpt_accept(__local struct ircd_gpt_ctrl *const ctrl,
                __constant const struct ircd_gpt_opts *const opts)
{
	const bool
	unlimited = opts->limit < 0;

	const uint
	batch_size = opts->batch_size,
	samps = opts->training_steps + opts->validation_steps + opts->testing_steps,
	steps = samps / batch_size,
	limit_ = opts->limit,
	unproc = ctrl->tokens - ctrl->count;

	const int
	limit = min(limit_?: unproc, opts->context_tokens),
	cycle_remain = limit - (ctrl->clk.cycle + 1),    // cycle not yet incr
	token_remain = opts->context_tokens - ctrl->count, // but count already incr
	remain_ = min(cycle_remain, token_remain),
	accept_ = ircd_gpt_accept_check(ctrl, opts),
	accept_abs = abs(accept_),
	remain = accept_ < 0 && accept_abs < remain_? accept_abs: remain_,
	_accept = accept_ >= 0? accept_: -remain;

	const bool
	accepting = _accept >= 0,
	dispatching = _accept < 0,
	limiting = remain <= 0;

	const int
	accept_num = 4,
	accept = limiting? accept_num: _accept,
	dispatch = accept >= 0? 0: remain;

	ctrl->accept = accept;
	ctrl->dispatch = dispatch;

	ctrl->magic = 0xC7012C70UL;
}

int
ircd_gpt_accept_check(__local struct ircd_gpt_ctrl *const ctrl,
                      __constant const struct ircd_gpt_opts *const opts)
{
	int best = 8;
	for(uint i = 0; i < 4; ++i)
	{
		const int
		remain = ircd_gpt_accept_match(ctrl, opts, i);

		if(remain == 0)
			return i;

		if(remain < best)
			best = remain;
	}

	return -best;
}

uint
ircd_gpt_accept_match(__local struct ircd_gpt_ctrl *const ctrl,
                      __constant const struct ircd_gpt_opts *const opts,
                      const uint i)
{
	const uint
	len = ircd_gpt_accept_len(ctrl, opts, i),
	n = min(ctrl->count, len),
	maxlen = 8;

	uint ret = len?: maxlen;
	for(uint j = 1; j <= n; ++j)
	{
		uint match = 0;
		for(; match < j; ++match)
		{
			const uint
			accept = opts->accept[i][match],
			token = ctrl->token[ctrl->count - j + match];

			if(token != accept)
				break;
		}

		if(match >= j)
			if(!(ret = len - match))
				break;
	}

	ret = max(ret, ctrl->tokens - ctrl->count);
	ret = min(ret, maxlen);
	return ret;
}

uint
ircd_gpt_accept_len(__local struct ircd_gpt_ctrl *const ctrl,
                    __constant const struct ircd_gpt_opts *const opts,
                    const uint i)
{
	uint len = 0;
	for(; len < 8; ++len)
		if(opts->accept[i][len] == (ushort)-1U)
			break;

	return len;
}

//
// backside
//

void
__attribute__((always_inline))
ircd_gpt_prop_elem(__global const struct ircd_gpt_ctrl *const ctrl,
                   __constant const struct ircd_gpt_opts *const opts,
                   __global float4 *const restrict param_,
                   __global float4 *const restrict exp_avg_,
                   __global float4 *const restrict exp_avg_sqr_)
{
	const uint
	li = get_local_id(0),
	ts = ctrl->clk.step;

	const float4
	param = param_[li],
	grad = ctrl->label[0].loss.mean,
	alpha[2] = { 1.0f - opts->beta[0], 1.0f - opts->beta[1], },
	exp_avg = ts? exp_avg_[li]: 0.0f,
	exp_avg_sqr = ts? exp_avg_sqr_[li]: 0.0f,
	exp_avg_mul = exp_avg * opts->beta[0],
	exp_avg_dot = exp_avg_mul + alpha[0] * grad,
	exp_avg_sqr_mul = exp_avg_sqr * opts->beta[1],
	exp_avg_sqr_dot = exp_avg_sqr_mul + alpha[1] * grad * grad,
	denom = native_sqrt(exp_avg_sqr_dot) + FLT_EPSILON,
	delta = opts->alpha * (exp_avg_dot / denom),
	update = param - delta;

	param_[li] = param + FLT_EPSILON;
	exp_avg_[li] = exp_avg + FLT_EPSILON;
	exp_avg_sqr_[li] = exp_avg_sqr + FLT_EPSILON;

	//param_[li] = update;
	//exp_avg_[li] = exp_avg_dot;
	//exp_avg_sqr_[li] = exp_avg_sqr_dot;
}

//
// backpropagations
//

__kernel void
__attribute__((always_inline))
//__attribute__((vec_type_hint(float4)))
//__attribute__((reqd_work_group_size(192, 1, 1)))
//__attribute__((amdgpu_flat_work_group_size(192, 192)))
ircd_gpt_norm_prop(__global const struct ircd_gpt_ctrl *const ctrl,
                   __constant const struct ircd_gpt_opts *const opts,
                   __global ircd_gpt_vectorv *const restrict bias,
                   __global ircd_gpt_vectorv *const restrict bias_m0,
                   __global ircd_gpt_vectorv *const restrict bias_m1,
                   __global ircd_gpt_vectorv *const restrict weight,
                   __global ircd_gpt_vectorv *const restrict weight_m0,
                   __global ircd_gpt_vectorv *const restrict weight_m1)
{
	ircd_gpt_prop_elem
	(
		ctrl, opts,
		bias->elem,
		bias_m0->elem,
		bias_m1->elem
	);

	ircd_gpt_prop_elem
	(
		ctrl, opts,
		weight->elem,
		weight_m0->elem,
		weight_m1->elem
	);
}

__kernel void
//__attribute__((vec_type_hint(float4)))
//__attribute__((reqd_work_group_size(192, 1, 1)))
//__attribute__((amdgpu_flat_work_group_size(192, 192)))
ircd_gpt_coil_prop_attn(__global const struct ircd_gpt_ctrl *const ctrl,
                        __constant const struct ircd_gpt_opts *const opts,
                        __global ircd_gpt_vectorv *const restrict norm_bias,
                        __global ircd_gpt_vectorv *const restrict norm_bias_m0,
                        __global ircd_gpt_vectorv *const restrict norm_bias_m1,
                        __global ircd_gpt_vectorv *const restrict norm_weight,
                        __global ircd_gpt_vectorv *const restrict norm_weight_m0,
                        __global ircd_gpt_vectorv *const restrict norm_weight_m1,
                        __global ircd_gpt_attn_aperaturev *const restrict fcon_bias,
                        __global ircd_gpt_attn_aperaturev *const restrict fcon_bias_m0,
                        __global ircd_gpt_attn_aperaturev *const restrict fcon_bias_m1,
                        __global ircd_gpt_attn_aperaturev *const restrict fcon_weight,
                        __global ircd_gpt_attn_aperaturev *const restrict fcon_weight_m0,
                        __global ircd_gpt_attn_aperaturev *const restrict fcon_weight_m1,
                        __global ircd_gpt_vectorv *const restrict proj_bias,
                        __global ircd_gpt_vectorv *const restrict proj_bias_m0,
                        __global ircd_gpt_vectorv *const restrict proj_bias_m1,
                        __global ircd_gpt_vectorv *const restrict proj_weight,
                        __global ircd_gpt_vectorv *const restrict proj_weight_m0,
                        __global ircd_gpt_vectorv *const restrict proj_weight_m1)
{
	const uint
	fcon_height = opts->embed_elems,
	proj_height = opts->embed_elems,
	segs = 3;

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

	for(uint j = 0; j < segs; ++j)
		ircd_gpt_prop_elem
		(
			ctrl, opts,
			fcon_bias->proj[j],
			fcon_bias_m0->proj[j],
			fcon_bias_m1->proj[j]
		);

	for(uint i = 0; i < fcon_height; ++i)
		for(uint j = 0; j < segs; ++j)
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
		proj_bias->elem,
		proj_bias_m0->elem,
		proj_bias_m1->elem
	);

	for(uint i = 0; i < proj_height; ++i)
		ircd_gpt_prop_elem
		(
			ctrl, opts,
			proj_weight[i].elem,
			proj_weight_m0[i].elem,
			proj_weight_m1[i].elem
		);
}

__kernel void
//__attribute__((vec_type_hint(float4)))
//__attribute__((reqd_work_group_size(192, 1, 1)))
//__attribute__((amdgpu_flat_work_group_size(192, 192)))
ircd_gpt_coil_prop_ffnn(__global const struct ircd_gpt_ctrl *const ctrl,
                        __constant const struct ircd_gpt_opts *const opts,
                        __global ircd_gpt_vectorv *const restrict norm_bias,
                        __global ircd_gpt_vectorv *const restrict norm_bias_m0,
                        __global ircd_gpt_vectorv *const restrict norm_bias_m1,
                        __global ircd_gpt_vectorv *const restrict norm_weight,
                        __global ircd_gpt_vectorv *const restrict norm_weight_m0,
                        __global ircd_gpt_vectorv *const restrict norm_weight_m1,
                        __global ircd_gpt_ffnn_aperaturev *const restrict fcon_bias,
                        __global ircd_gpt_ffnn_aperaturev *const restrict fcon_bias_m0,
                        __global ircd_gpt_ffnn_aperaturev *const restrict fcon_bias_m1,
                        __global ircd_gpt_ffnn_aperaturev *const restrict fcon_weight,
                        __global ircd_gpt_ffnn_aperaturev *const restrict fcon_weight_m0,
                        __global ircd_gpt_ffnn_aperaturev *const restrict fcon_weight_m1,
                        __global ircd_gpt_vectorv *const restrict proj_bias,
                        __global ircd_gpt_vectorv *const restrict proj_bias_m0,
                        __global ircd_gpt_vectorv *const restrict proj_bias_m1,
                        __global ircd_gpt_vectorv *const restrict proj_weight,
                        __global ircd_gpt_vectorv *const restrict proj_weight_m0,
                        __global ircd_gpt_vectorv *const restrict proj_weight_m1)
{
	const uint
	fcon_height = opts->embed_elems,
	proj_height = opts->ffnn_elems,
	segs = 4;

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

	for(uint j = 0; j < segs; ++j)
		ircd_gpt_prop_elem
		(
			ctrl, opts,
			fcon_bias->proj[j],
			fcon_bias_m0->proj[j],
			fcon_bias_m1->proj[j]
		);

	for(uint i = 0; i < fcon_height; ++i)
		for(uint j = 0; j < segs; ++j)
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
		proj_bias->elem,
		proj_bias_m0->elem,
		proj_bias_m1->elem
	);

	for(uint i = 0; i < proj_height; ++i)
		ircd_gpt_prop_elem
		(
			ctrl, opts,
			proj_weight[i].elem,
			proj_weight_m0[i].elem,
			proj_weight_m1[i].elem
		);
}

__kernel void
//__attribute__((vec_type_hint(float4)))
//__attribute__((reqd_work_group_size(192, 1, 1)))
//__attribute__((amdgpu_flat_work_group_size(192, 192)))
ircd_gpt_lm_embed_prop(__global const struct ircd_gpt_ctrl *const ctrl,
                       __constant const struct ircd_gpt_opts *const opts,
                       __global ircd_gpt_vectorv *const restrict pos,
                       __global ircd_gpt_vectorv *const restrict pos_m0,
                       __global ircd_gpt_vectorv *const restrict pos_m1,
                       __global ircd_gpt_vectorv *const restrict token,
                       __global ircd_gpt_vectorv *const restrict token_m0,
                       __global ircd_gpt_vectorv *const restrict token_m1)
{
	const uint
	ln = get_local_size(0),
	wi = get_global_offset(0) / ln + get_group_id(0),
	wn = ctrl->count,
	cn = opts->context_tokens / wn,
	ci = cn * wi,
	tn = opts->logits / wn,
	ti = tn * wi;

	for(uint i = ci; i < ci + cn; ++i)
		ircd_gpt_prop_elem
		(
			ctrl, opts,
			pos[i].elem,
			pos_m0[i].elem,
			pos_m1[i].elem
		);

	for(uint i = ti; i < ti + tn; ++i)
		ircd_gpt_prop_elem
		(
			ctrl, opts,
			token[i].elem,
			token_m0[i].elem,
			token_m1[i].elem
		);
}

/// Gaussian Error Linear Unit
void
ircd_gpt_ffnn_gelu(__local ircd_gpt_ffnn_aperaturev *const out,
                   __local const ircd_gpt_ffnn_aperaturev *const in_,
                   const uint i)
{
	const float4
	in = in_->fcon[i];

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

	out->fcon[i] = a;
}

void
ircd_gpt_norm(__local ircd_gpt_vectorv *const out,
              __local const ircd_gpt_vectorv *const in,
              __local ircd_gpt_vectorv *const restrict tmp,
              __global const ircd_gpt_vectorv *const restrict bias,
              __global const ircd_gpt_vectorv *const restrict weight,
              const uint ln,
              const uint li)
{
	// Layer re-normalization
	ircd_simt_math_norm_f4lldr(out->elem, in->elem, tmp->elem, ln, li);
	ircd_gpt_norm_fmad(out, out, bias, weight, li);
}

void
ircd_gpt_norm_fmad(__local ircd_gpt_vectorv *const out,
                   __local const ircd_gpt_vectorv *const in,
                   __global const ircd_gpt_vectorv *const restrict bias,
                   __global const ircd_gpt_vectorv *const restrict weight,
                   const uint i)
{
	out->elem[i] = in->elem[i] * weight->elem[i] + bias->elem[i];
}
