// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_ROOM_BOOTSTRAP_H

struct ircd::m::room::bootstrap
{
	static bool required(const id &);

	// restrap: synchronous; send_join
	bootstrap(const event &, const string_view &host, const string_view &room_version = {});

	// restrap: asynchronous; launch ctx; send_join
	bootstrap(const event::id &, const string_view &host, const string_view &room_version = {});

	// synchronous make_join, eval; asynchronous send_join
	bootstrap(event::id::buf &, const room::id &, const m::id::user &, const vector_view<const string_view> &hosts);
};
