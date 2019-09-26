// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::m::node
IRCD_MODULE_EXPORT
ircd::m::create(const node &node,
                const json::members &args)
{
	assert(node.node_id);
	const m::room::id::buf room_id
	{
		node.room_id()
	};

	create(room_id, m::me.user_id);
	return node;
}

bool
IRCD_MODULE_EXPORT
ircd::m::exists(const node &node)
{
	const m::room::id::buf room_id
	{
		node.room_id()
	};

	return exists(room_id);
}

bool
IRCD_MODULE_EXPORT
ircd::m::my(const node &node)
{
	return my_host(node.node_id);
}
