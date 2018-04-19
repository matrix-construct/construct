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
#define HAVE_IRCD_M_EVENTS_H

namespace ircd::m::events
{
	using id_closure_bool = std::function<bool (const event::idx &, const event::id &)>;
	using closure_bool = std::function<bool (const event::idx &, const event &)>;

	// counts up from start
	bool for_each(const uint64_t &start, const id_closure_bool &);
	bool for_each(const uint64_t &start, const closure_bool &);
	bool for_each(const uint64_t &start, const event_filter &, const closure_bool &);

	// -1 starts at newest event; counts down
	bool rfor_each(const uint64_t &start, const id_closure_bool &);
	bool rfor_each(const uint64_t &start, const closure_bool &);
	bool rfor_each(const uint64_t &start, const event_filter &, const closure_bool &);
}
