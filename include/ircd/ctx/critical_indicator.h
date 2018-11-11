// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_CTX_CRITICAL_INDICATOR_H

namespace ircd::ctx::this_ctx
{
	struct critical_indicator;                   // Indicates if yielding happened for a section
}

/// An instance of critical_indicator reports if context switching happened.
///
/// A critical_indicator remains true after construction until a context switch
/// has occurred. It then becomes false. This is not an assertion and is
/// available in optimized builds for real use. For example, a context may
/// want to recompute some value after a context switch and opportunistically
/// skip this effort when this indicator shows no switch occurred.
///
class ircd::ctx::this_ctx::critical_indicator
{
	uint64_t state;

  public:
	uint64_t count() const             { return yields(cur()) - state;         }
	operator bool() const              { return yields(cur()) == state;        }

	critical_indicator()
	:state{yields(cur())}
	{}
};
