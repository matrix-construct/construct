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

#include "type.h"
#include "type.unaligned.h"
#include "traits.h"
#include "lane_cast.h"
#include "broad_cast.h"
#include "shift.h"
#include "popcnt.h"
#include "lzcnt.h"
#include "tzcnt.h"
#include "sum_or.h"
#include "sum_and.h"
#include "print.h"

namespace ircd
{
	using simd::lane_cast;

	using simd::shl;
	using simd::shr;

	using simd::lzcnt;
	using simd::tzcnt;

	using simd::popcnt;
	using simd::popmask;
	using simd::boolmask;
}
