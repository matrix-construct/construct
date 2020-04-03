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
#define HAVE_IRCD_M_EVENT_EVENT_ID_H

// Convenience suite to reverse event::idx to event::id. Note that event::id's
// have more many more tools and related elements than what is found in this
// header; despite the name which merely matches the function name here.

namespace ircd::m
{
	bool event_id(std::nothrow_t, const event::idx &, const event::id::closure &);

	[[nodiscard]] event::id event_id(std::nothrow_t, const event::idx &, event::id::buf &);
	event::id event_id(const event::idx &, event::id::buf &);

	[[nodiscard]] event::id::buf event_id(std::nothrow_t, const event::idx &);
	event::id::buf event_id(const event::idx &);
}
