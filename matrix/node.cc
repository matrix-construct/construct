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
ircd::m::create(const node &node,
                const json::members &args)
{
	assert(node.node_id);
	const m::node::room node_room
	{
		node
	};

	const auto event_id
	{
		create(node_room.room_id, me())
	};

	return node;
}

bool
ircd::m::exists(const node &node)
{
	const m::node::room node_room
	{
		node
	};

	return exists(node_room.room_id);
}

bool
ircd::m::my(const node &node)
{
	return my_host(node.node_id);
}

//
// node::room
//

/// This generates a room mxid for the "node's room" essentially serving as
/// a database mechanism for this specific node. This room_id is a hash of
/// the node's full mxid.
///
ircd::m::node::room::room(const m::node &node)
:node
{
	node
}
,room_id{[this]
{
	assert(!empty(this->node.node_id));

	// for compatibility with hashing legacy node_id's
	char buf[256 + 16];
	mutable_buffer mb{buf};
	consume(mb, copy(mb, "::"_sv));
	consume(mb, copy(mb, this->node.node_id));
	const string_view &node_id
	{
		buf, data(mb)
	};

	const sha256::buf hash
	{
		sha256{node_id}
	};

	const string_view b58
	{
		b58::encode(buf, hash)
	};

	return id::room::buf
	{
		b58, my_host()
	};
}()}
{
	this->m::room::room_id = this->room_id;
}

//
// node::keys
//

bool
ircd::m::node::keys::get(const string_view &key_id,
                         const ed25519_closure &closure)
const
{
	return get(key_id, key_closure{[&closure]
	(const json::string &keyb64)
	{
		const ed25519::pk pk
		{
			[&keyb64](auto&& buf)
			{
				b64::decode(buf, keyb64);
			}
		};

		closure(pk);
	}});
}

bool
ircd::m::node::keys::get(const string_view &key_id,
                         const key_closure &closure)
const
{
	return m::keys::get(node.node_id, key_id, [&closure, &key_id]
	(const json::object &keys)
	{
		const json::object &verify_keys
		{
			keys.at("verify_keys")
		};

		const json::object old_verify_keys
		{
			keys["old_verify_keys"]
		};

		const json::object &verify_key
		{
			verify_keys.has(key_id)?
				verify_keys.get(key_id):
				old_verify_keys.get(key_id)
		};

		const json::string &key
		{
			verify_key.at("key")
		};

		closure(key);
	});
}

//
// node::mitsein
//

bool
ircd::m::node::mitsein::has(const m::node &other,
                            const string_view &membership)
const
{
	return !for_each(other, membership, []
	(const auto &, const auto &)
	{
		return false;
	});
}

size_t
ircd::m::node::mitsein::count(const m::node &other,
                              const string_view &membership)
const
{
	size_t ret(0);
	for_each(other, membership, [&ret]
	(const auto &, const auto &)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
ircd::m::node::mitsein::for_each(const m::node &other,
                                 const closure &closure)
const
{
	return for_each(other, string_view{}, closure);
}

bool
ircd::m::node::mitsein::for_each(const m::node &other,
                                 const string_view &membership,
                                 const closure &closure)
const
{
	always_assert(0);
	return true;
}
