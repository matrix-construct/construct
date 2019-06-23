// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	extern hookfn<vm::eval &> create_my_node_hook;
	extern hookfn<vm::eval &> create_nodes_hook;
}

ircd::mapi::header
IRCD_MODULE
{
	"Server Nodes"
};

decltype(ircd::m::create_my_node_hook)
ircd::m::create_my_node_hook
{
	{
		{ "_site",       "vm.effect"      },
		{ "room_id",     "!nodes"         },
		{ "type",        "m.room.create"  },
	},
	[](const m::event &, m::vm::eval &)
	{
		create(m::my_node.room_id(), m::me.user_id);
	}
};

decltype(ircd::m::create_nodes_hook)
ircd::m::create_nodes_hook
{
	{
		{ "_site",       "vm.effect"      },
		{ "room_id",     "!ircd"          },
		{ "type",        "m.room.create"  },
	},
	[](const m::event &, m::vm::eval &)
	{
		create(m::nodes, m::me.user_id);
	}
};

//
// node
//

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

	//TODO: ABA
	create(room_id, m::me.user_id);
	send(nodes, m::me.user_id, "ircd.node", node.node_id, args);
	return node;
}

bool
IRCD_MODULE_EXPORT
ircd::m::exists(const node &node)
{
	return nodes.has("ircd.node", node.node_id);
}

bool
IRCD_MODULE_EXPORT
ircd::m::my(const node &node)
{
	return my_host(node.node_id);
}
