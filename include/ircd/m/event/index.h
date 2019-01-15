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
#define HAVE_IRCD_M_EVENT_INDEX_H

namespace ircd::m
{
	bool index(const event::id &, std::nothrow_t, const event::closure_idx &);

	event::idx index(const event::id &, std::nothrow_t);
	event::idx index(const event::id &);

	event::idx index(const event &, std::nothrow_t);
	event::idx index(const event &);
}
