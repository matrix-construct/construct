// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_TRACE_H

namespace ircd::m::trace
{
	using closure = util::function_bool
	<
		const event::idx &, const uint64_t &, const m::room::message &
	>;

	bool for_each(const event::idx &, const closure &);
}
