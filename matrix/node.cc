// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

IRCD_MODULE_EXPORT
ircd::m::node::node(const string_view &node_id)
:node_id{node_id}
{
	rfc3986::valid_remote(node_id);
}

void
IRCD_MODULE_EXPORT
ircd::m::node::key(const string_view &key_id,
                   const ed25519_closure &closure)
const
{
	key(key_id, key_closure{[&closure]
	(const string_view &keyb64)
	{
		const ed25519::pk pk
		{
			[&keyb64](auto &buf)
			{
				b64decode(buf, unquote(keyb64));
			}
		};

		closure(pk);
	}});
}

void
IRCD_MODULE_EXPORT
ircd::m::node::key(const string_view &key_id,
                   const key_closure &closure)
const
{
	m::keys::get(node_id, key_id, [&closure, &key_id]
	(const json::object &keys)
	{
		const json::object &vks
		{
			keys.at("verify_keys")
		};

		const json::object &vkk
		{
			vks.at(key_id)
		};

		const string_view &key
		{
			vkk.at("key")
		};

		closure(key);
	});
}

/// Generates a node-room ID into buffer; see room_id() overload.
ircd::m::id::room::buf
IRCD_MODULE_EXPORT
ircd::m::node::room_id()
const
{
	ircd::m::id::room::buf buf;
	return buf.assigned(room_id(buf));
}

/// This generates a room mxid for the "node's room" essentially serving as
/// a database mechanism for this specific node. This room_id is a hash of
/// the node's full mxid.
///
ircd::m::id::room
IRCD_MODULE_EXPORT
ircd::m::node::room_id(const mutable_buffer &buf)
const
{
	assert(!empty(this->node_id));

	// for compatibility with hashing legacy node_id's
	thread_local char node_id_buf[m::id::MAX_SIZE];
	mutable_buffer mb{node_id_buf};
	consume(mb, copy(mb, "::"_sv));
	consume(mb, copy(mb, this->node_id));
	const string_view &node_id
	{
		node_id_buf, data(mb)
	};

	const sha256::buf hash
	{
		sha256{node_id}
	};

	thread_local char b58_buf[b58encode_size(size(hash))];
	const string_view b58
	{
		b58encode(b58_buf, hash)
	};

	return id::room
	{
		buf, b58, my_host()
	};
}

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

	create(room_id, me());
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

//
// node::room
//

IRCD_MODULE_EXPORT
ircd::m::node::room::room(const string_view &node_id)
:room
{
	m::node{node_id}
}
{
}

IRCD_MODULE_EXPORT
ircd::m::node::room::room(const m::node &node)
:node
{
	node
}
,room_id
{
	node.room_id()
}
{
	this->m::room::room_id = this->room_id;
}
