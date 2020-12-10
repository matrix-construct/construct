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
#define HAVE_IRCD_M_VM_H

/// Matrix Virtual Machine
///
namespace ircd::m::vm
{
	struct init;

	extern log::log log;
	extern ctx::dock dock;
	extern bool ready;
}

#include "fault.h"
#include "error.h"
#include "phase.h"
#include "opts.h"
#include "eval.h"
#include "seq.h"

namespace ircd::m::vm
{
	fault execute(eval &, const vector_view<const event> &);
	fault execute(eval &, const json::array &);
	fault inject(eval &, json::iov &, const json::iov &);
}

struct ircd::m::vm::init
{
	init(), ~init() noexcept;
};
