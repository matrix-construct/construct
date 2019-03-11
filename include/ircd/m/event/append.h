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
#define HAVE_IRCD_M_EVENT_APPEND

namespace ircd::m
{
	struct event_append_opts
	{
		const event::idx *event_idx {nullptr};
		const string_view *client_txnid {nullptr};
		const id::user *user_id {nullptr};
		const room *user_room {nullptr};
	};

	void append(json::stack::object &, const event &, const event_append_opts & = {});
	void append(json::stack::array &, const event &, const event_append_opts & = {});
}
