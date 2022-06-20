// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

typedef union ircd_gpt_vector_f32x4 ircd_gpt_vectorv __attribute__((aligned(4096)));
typedef struct ircd_gpt_attn_qkv_f32x4 ircd_gpt_attn_qkvv __attribute__((aligned(4096)));
typedef union ircd_gpt_attn_aperature_f32x4 ircd_gpt_attn_aperaturev __attribute__((aligned(4096)));
typedef union ircd_gpt_ffnn_aperature_f32x4 ircd_gpt_ffnn_aperaturev __attribute__((aligned(4096)));

//
// Frontside
//

void
__attribute__((internal_linkage))
ircd_gpt_norm_fmad(__local ircd_gpt_vectorv *out,
                   __local const ircd_gpt_vectorv *in,
                   __global const ircd_gpt_vectorv *bias,
                   __global const ircd_gpt_vectorv *weight,
                   const uint li);

void
__attribute__((internal_linkage))
ircd_gpt_norm(__local ircd_gpt_vectorv *out,
              __local const ircd_gpt_vectorv *in,
              __local ircd_gpt_vectorv *tmp,
              __global const ircd_gpt_vectorv *bias,
              __global const ircd_gpt_vectorv *weight,
              const uint ln,
              const uint li);

void
__attribute__((internal_linkage))
ircd_gpt_ffnn_gelu(__local ircd_gpt_ffnn_aperaturev *out,
                   __local const ircd_gpt_ffnn_aperaturev *in,
                   const uint i);

void
__attribute__((internal_linkage))
ircd_gpt_ffnn_fcon_tmul(__constant const struct ircd_gpt_opts *opts,
                        __local ircd_gpt_ffnn_aperaturev *out,
                        __local const ircd_gpt_vectorv *in,
                        __global const ircd_gpt_ffnn_aperaturev *bias,
                        __global const ircd_gpt_ffnn_aperaturev *weight,
                        const uint li);

void
__attribute__((internal_linkage))
ircd_gpt_ffnn_fcon(__global const struct ircd_gpt_ctrl *ctrl,
                   __constant const struct ircd_gpt_opts *opts,
                   __local ircd_gpt_ffnn_aperaturev *out,
                   __local const ircd_gpt_vectorv *in,
                   __global const ircd_gpt_ffnn_aperaturev *bias,
                   __global const ircd_gpt_ffnn_aperaturev *weight,
                   const uint ln,
                   const uint li);

void
__attribute__((internal_linkage))
ircd_gpt_ffnn_proj_tmul(__constant const struct ircd_gpt_opts *opts,
                        __local ircd_gpt_vectorv *out,
                        __local const ircd_gpt_ffnn_aperaturev *in,
                        __global const ircd_gpt_vectorv *bias,
                        __global const ircd_gpt_vectorv *weight,
                        const uint li);

void
__attribute__((internal_linkage))
ircd_gpt_ffnn(__global const struct ircd_gpt_ctrl *ctrl,
              __constant const struct ircd_gpt_opts *opts,
              __local ircd_gpt_vectorv *vector,
              __local ircd_gpt_ffnn_aperaturev *buf,
              __local ircd_gpt_vectorv *tmp,
              __global const ircd_gpt_vectorv *norm_bias,
              __global const ircd_gpt_vectorv *norm_weight,
              __global const ircd_gpt_ffnn_aperaturev *fcon_bias,
              __global const ircd_gpt_ffnn_aperaturev *fcon_weight,
              __global const ircd_gpt_vectorv *proj_bias,
              __global const ircd_gpt_vectorv *proj_weight,
              const uint ln,
              const uint li);

void
__attribute__((internal_linkage))
ircd_gpt_attn_fcon_tmul(__constant const struct ircd_gpt_opts *opts,
                        __local ircd_gpt_attn_aperaturev *out,
                        __local const ircd_gpt_vectorv *in,
                        __global const ircd_gpt_attn_aperaturev *bias,
                        __global const ircd_gpt_attn_aperaturev *weight,
                        const uint ln,
                        const uint li);

__kernel void
__attribute__((visibility("protected")))
ircd_gpt_attn_fcon(__global const struct ircd_gpt_ctrl *ctrl,
                   __constant const struct ircd_gpt_opts *opts,
                   __private const uint layer,
                   __global ircd_gpt_attn_aperaturev *state,
                   __global const ircd_gpt_vectorv *accum,
                   __global const ircd_gpt_vectorv *norm_bias,
                   __global const ircd_gpt_vectorv *norm_weight,
                   __global const ircd_gpt_attn_aperaturev *fcon_bias,
                   __global const ircd_gpt_attn_aperaturev *fcon_weight);

//
// head
//

void
__attribute__((internal_linkage))
_ircd_gpt_lm_embed(__global const struct ircd_gpt_ctrl *ctrl,
                   __constant const struct ircd_gpt_opts *opts,
                   __global ircd_gpt_vectorv *accum,
                   __global const ircd_gpt_vectorv *pos,
                   __global const ircd_gpt_vectorv *vocab,
                   const ushort out_idx,
                   const ushort tok_idx,
                   const ushort word_idx);

__kernel void
__attribute__((visibility("protected")))
ircd_gpt_lm_embed(__global const struct ircd_gpt_ctrl *ctrl,
                  __constant const struct ircd_gpt_opts *opts,
                  __global ircd_gpt_vectorv *accum,
                  __global const ircd_gpt_vectorv *pos,
                  __global const ircd_gpt_vectorv *vocab);

__kernel void
__attribute__((visibility("protected")))
ircd_gpt_lm_norm(__global const struct ircd_gpt_ctrl *ctrl,
                 __constant const struct ircd_gpt_opts *opts,
                 __global ircd_gpt_vectorv *accum,
                 __global const ircd_gpt_vectorv *norm_bias,
                 __global const ircd_gpt_vectorv *norm_weight);

__kernel void
__attribute__((visibility("protected")))
ircd_gpt_lm_logit(__global const struct ircd_gpt_ctrl *ctrl,
                  __constant const struct ircd_gpt_opts *opts,
                  __global float *logit,
                  __global const ircd_gpt_vectorv *accum,
                  __global const ircd_gpt_vectorv *pos,
                  __global const ircd_gpt_vectorv *vocab);

__kernel void
__attribute__((visibility("protected")))
ircd_gpt_lm_logsm(__global struct ircd_gpt_ctrl *ctrl,
                  __constant const struct ircd_gpt_opts *opts,
                  __global float logit[65536]);

void
__attribute__((internal_linkage))
ircd_gpt_lm_result_top(__local struct ircd_gpt_ctrl *ctrl,
                       __constant const struct ircd_gpt_opts *opts,
                       __local const ushort *idx,
                       __global const float *logsm,
                       const uint i);

void
__attribute__((internal_linkage))
ircd_gpt_lm_result_label_mean(__local struct ircd_gpt_ctrl *ctrl,
                              __constant const struct ircd_gpt_opts *opts,
                              __local struct ircd_math_mean *mean,
                              const float last);

void
__attribute__((internal_linkage))
ircd_gpt_lm_result_label(__local struct ircd_gpt_ctrl *ctrl,
                         __constant const struct ircd_gpt_opts *opts,
                         __local struct ircd_gpt_ctrl_label *label,
                         __global const float *logsm);

ushort
__attribute__((internal_linkage))
ircd_gpt_lm_result_select(__local struct ircd_gpt_ctrl *ctrl,
                          __constant const struct ircd_gpt_opts *opts,
                          __local const ushort *idx,
                          __global const float *logsm);

uint
__attribute__((internal_linkage))
ircd_gpt_accept_len(__local struct ircd_gpt_ctrl *ctrl,
                    __constant const struct ircd_gpt_opts *opts,
                    const uint i);

uint
__attribute__((internal_linkage))
ircd_gpt_accept_match(__local struct ircd_gpt_ctrl *ctrl,
                      __constant const struct ircd_gpt_opts *opts,
                      const uint i);

int
__attribute__((internal_linkage))
ircd_gpt_accept_check(__local struct ircd_gpt_ctrl *ctrl,
                      __constant const struct ircd_gpt_opts *opts);

void
__attribute__((internal_linkage))
ircd_gpt_accept(__local struct ircd_gpt_ctrl *ctrl,
                __constant const struct ircd_gpt_opts *opts);

//
// backside
//

void
__attribute__((internal_linkage))
ircd_gpt_prop_elem(__global const struct ircd_gpt_ctrl *ctrl,
                   __constant const struct ircd_gpt_opts *opts,
                   __global float4 *param_,
                   __global float4 *exp_avg_,
                   __global float4 *exp_avg_sqr_);

//
// backpropagations
//

__kernel void
__attribute__((visibility("protected")))
ircd_gpt_norm_prop(__global const struct ircd_gpt_ctrl *ctrl,
                   __constant const struct ircd_gpt_opts *opts,
                   __global ircd_gpt_vectorv *bias,
                   __global ircd_gpt_vectorv *bias_m0,
                   __global ircd_gpt_vectorv *bias_m1,
                   __global ircd_gpt_vectorv *weight,
                   __global ircd_gpt_vectorv *weight_m0,
                   __global ircd_gpt_vectorv *weight_m1);

__kernel void
__attribute__((visibility("protected")))
ircd_gpt_coil_prop_attn(__global const struct ircd_gpt_ctrl *ctrl,
                        __constant const struct ircd_gpt_opts *opts,
                        __global ircd_gpt_vectorv *norm_bias,
                        __global ircd_gpt_vectorv *norm_bias_m0,
                        __global ircd_gpt_vectorv *norm_bias_m1,
                        __global ircd_gpt_vectorv *norm_weight,
                        __global ircd_gpt_vectorv *norm_weight_m0,
                        __global ircd_gpt_vectorv *norm_weight_m1,
                        __global ircd_gpt_attn_aperaturev *fcon_bias,
                        __global ircd_gpt_attn_aperaturev *fcon_bias_m0,
                        __global ircd_gpt_attn_aperaturev *fcon_bias_m1,
                        __global ircd_gpt_attn_aperaturev *fcon_weight,
                        __global ircd_gpt_attn_aperaturev *fcon_weight_m0,
                        __global ircd_gpt_attn_aperaturev *fcon_weight_m1,
                        __global ircd_gpt_vectorv *proj_bias,
                        __global ircd_gpt_vectorv *proj_bias_m0,
                        __global ircd_gpt_vectorv *proj_bias_m1,
                        __global ircd_gpt_vectorv *proj_weight,
                        __global ircd_gpt_vectorv *proj_weight_m0,
                        __global ircd_gpt_vectorv *proj_weight_m1);

__kernel void
__attribute__((visibility("protected")))
ircd_gpt_coil_prop_ffnn(__global const struct ircd_gpt_ctrl *ctrl,
                        __constant const struct ircd_gpt_opts *opts,
                        __global ircd_gpt_vectorv *norm_bias,
                        __global ircd_gpt_vectorv *norm_bias_m0,
                        __global ircd_gpt_vectorv *norm_bias_m1,
                        __global ircd_gpt_vectorv *norm_weight,
                        __global ircd_gpt_vectorv *norm_weight_m0,
                        __global ircd_gpt_vectorv *norm_weight_m1,
                        __global ircd_gpt_ffnn_aperaturev *fcon_bias,
                        __global ircd_gpt_ffnn_aperaturev *fcon_bias_m0,
                        __global ircd_gpt_ffnn_aperaturev *fcon_bias_m1,
                        __global ircd_gpt_ffnn_aperaturev *fcon_weight,
                        __global ircd_gpt_ffnn_aperaturev *fcon_weight_m0,
                        __global ircd_gpt_ffnn_aperaturev *fcon_weight_m1,
                        __global ircd_gpt_vectorv *proj_bias,
                        __global ircd_gpt_vectorv *proj_bias_m0,
                        __global ircd_gpt_vectorv *proj_bias_m1,
                        __global ircd_gpt_vectorv *proj_weight,
                        __global ircd_gpt_vectorv *proj_weight_m0,
                        __global ircd_gpt_vectorv *proj_weight_m1);

__kernel void
__attribute__((visibility("protected")))
ircd_gpt_lm_embed_prop(__global const struct ircd_gpt_ctrl *ctrl,
                       __constant const struct ircd_gpt_opts *opts,
                       __global ircd_gpt_vectorv *pos,
                       __global ircd_gpt_vectorv *pos_m0,
                       __global ircd_gpt_vectorv *pos_m1,
                       __global ircd_gpt_vectorv *token,
                       __global ircd_gpt_vectorv *token_m0,
                       __global ircd_gpt_vectorv *token_m1);
