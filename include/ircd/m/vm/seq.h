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
#define HAVE_IRCD_M_VM_SEQ_H

namespace ircd::m::vm::sequence
{
	extern ctx::dock dock;
	extern uint64_t retired;      // already written; always monotonic
	extern uint64_t committed;    // pending write; usually monotonic
	extern uint64_t uncommitted;  // evaluating; not monotonic
	static size_t pending;

	const uint64_t &get(const eval &);
	uint64_t get(id::event::buf &); // [GET]

	uint64_t max();
	uint64_t min();
}
