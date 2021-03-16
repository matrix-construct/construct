// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_SIMD_H

#include "support.h"
#include "type.h"
#include "unaligned.h"
#include "traits.h"
#include "cast.h"
#include "lane_cast.h"
#include "broad_cast.h"
#include "print.h"
#include "split.h"
#include "lower.h"
#include "upper.h"
#include "gather.h"
#include "scatter.h"
#include "shl.h"
#include "shr.h"
#include "rol.h"
#include "ror.h"
#include "popcnt.h"
#include "lzcnt.h"
#include "tzcnt.h"
#include "reduce.h"
#include "hadd.h"
#include "any.h"
#include "all.h"
#include "stream.h"
#include "for_each.h"
#include "transform.h"
#include "generate.h"
#include "accumulate.h"
#include "tokens.h"
#include "str.h"

namespace ircd
{
	using simd::lane_cast;
	using simd::shl;
	using simd::shr;
	using simd::rol;
	using simd::ror;
	using simd::lzcnt;
	using simd::tzcnt;
	using simd::popcnt;
}
