// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_SIMT_SAMAX_H

/// Softargmax state.
///
/// In FP32 environments, naively implementing softmax as expressed in
/// literature will overflow. Researchers have mitigated this at the cost
/// of some extra state and passes.
struct ircd_math_samax
{
	float
	mu,
	sum,
	lambda;
};
