// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

mapi::header
IRCD_MODULE
{
	"Server Nodes"
};

m::room
nodes_room
{
	m::nodes.room_id
};

extern "C" m::node
create_node(const m::node::id &node_id,
            const json::members &args)
{
	const m::node node
	{
		node_id
	};

	const m::room::id::buf room_id
	{
		node.room_id()
	};

	//TODO: ABA
	create(room_id, m::me.user_id);
	send(nodes_room, m::me.user_id, "ircd.node", node_id, args);
	return node;
}

extern "C" bool
exists__nodeid(const m::node::id &node_id)
{
	return nodes_room.has("ircd.node", node_id);
}

static void
create_my_node_room(const m::event &)
{
	create(m::my_node.room_id(), m::me.user_id);
}

const m::hook<>
create_my_node_hook
{
	create_my_node_room,
	{
		{ "_site",       "vm.notify"      },
		{ "room_id",     "!nodes"         },
		{ "type",        "m.room.create"  },
	}
};

static void
create_nodes_room(const m::event &)
{
	create(nodes_room, m::me.user_id);
}

const m::hook<>
create_nodes_hook
{
	create_nodes_room,
	{
		{ "_site",       "vm.notify"      },
		{ "room_id",     "!ircd"          },
		{ "type",        "m.room.create"  },
	}
};
