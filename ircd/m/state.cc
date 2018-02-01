// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/m/m.h>

void
ircd::m::state::append_nodes(db::txn &txn,
                             const event &event)
{
	const auto &type{at<"type"_>(event)};
	const auto &state_key{at<"state_key"_>(event)};
	const auto &event_id{at<"event_id"_>(event)};
	const auto &room_id{at<"room_id"_>(event)};

	if(type == "m.room.create")
	{
		// Because this is a new tree and nothing is read from the DB, all
		// writes here are just copies into the txn and these buffers can
		// remain off-stack.
		const critical_assertion ca;
		thread_local char key[KEY_MAX_SZ];
		thread_local char head[ID_MAX_SZ];

		const json::array keys[]
		{
			{ state::make_key(key, type, state_key) }
		};

		const string_view vals[]
		{
			{ event_id }
		};

		set_head(txn, room_id, set_node(txn, head, keys, 1, vals, 1));
		return;
	}

	state::insert(txn, room_id, type, state_key, event_id);
}

void
ircd::m::state::insert(db::txn &txn,
                       const id::room &room_id,
                       const string_view &type,
                       const string_view &state_key,
                       const id::event &event_id)
{
	// The insertion process reads from the DB and will yield this ircd::ctx
	// so the key buffer must stay on this stack.
	char key[KEY_MAX_SZ];
	return insert(txn, room_id, make_key(key, type, state_key), event_id);
}

void
ircd::m::state::insert(db::txn &txn,
                       const id::room &room_id,
                       const json::array &key,
                       const id::event &event_id)
{
	db::column heads{*event::events, "state_head"};
	db::column nodes{*event::events, "state_node"};

	// Start with the root node ID for room.
	char nextbuf[ID_MAX_SZ];
	string_view nextid{get_head(heads, nextbuf, room_id)};

	char prevbuf[ID_MAX_SZ];
	string_view previd;

	while(nextid) get_node(nodes, nextid, [&](const node &node)
	{
		const auto pos(node.find(key));
		const auto head(set_node(txn, nextbuf, node, pos, key, event_id));
		set_head(txn, room_id, head);
		nextid = {};
	});
}

/// Convenience to get value from the current room head.
void
ircd::m::state::get_value__room(const id::room &room_id,
                                const string_view &type,
                                const string_view &state_key,
                                const id_closure &closure)
{
	char head[ID_MAX_SZ];
	return get_value(get_head(head, room_id), type, state_key, closure);
}

/// Convenience to get value making a key
void
ircd::m::state::get_value(const string_view &head,
                          const string_view &type,
                          const string_view &state_key,
                          const id_closure &closure)
{
	char key[KEY_MAX_SZ];
	return get_value(head, make_key(key, type, state_key), closure);
}

/// see: get_value(); user does not have to supply column reference here
void
ircd::m::state::get_value(const string_view &head,
                          const json::array &key,
                          const id_closure &closure)
{
	db::column column
	{
		*event::events, "state_node"
	};

	get_value(column, head, key, closure);
}

/// Recursive query to find the leaf value for the given key, starting from
/// the given head node ID. Value can be viewed in the closure. This throws
/// m::NOT_FOUND if the exact key and its value does not exist in the tree;
/// no node ID's are ever returned here.
void
ircd::m::state::get_value(db::column &column,
                          const string_view &head,
                          const json::array &key,
                          const id_closure &closure)
{
	char nextbuf[ID_MAX_SZ];
	string_view nextid{head};
	while(nextid) get_node(column, nextid, [&](const node &node)
	{
		const auto pos(node.find(key));
		if(pos >= node.vals())
			throw m::NOT_FOUND{};

		const auto &v(node.val(pos));
		if(valid(id::EVENT, v))
		{
			if(node.key(pos) != key)
				throw m::NOT_FOUND{};

			nextid = {};
			closure(v);
		} else {
			assert(size(v) < sizeof(nextbuf));
			nextid = { nextbuf, strlcpy(nextbuf, v) };
		}
	});
}

/// Set the root node ID for a room in this db transaction.
void
ircd::m::state::set_head(db::txn &iov,
                         const id::room &room_id,
                         const string_view &head_id)
{
	db::txn::append
	{
		iov, db::delta
		{
			"state_head",  // col
			room_id,       // key
			head_id,       // val
		}
	};
}

/// Copy a room's root node ID into buffer.
ircd::string_view
ircd::m::state::get_head(const mutable_buffer &buf,
                         const id::room &room_id)
{
	db::column column
	{
		*event::events, "state_head"
	};

	return get_head(column, buf, room_id);
}

/// Copy a room's root node ID into buffer; already have column reference.
ircd::string_view
ircd::m::state::get_head(db::column &column,
                         const mutable_buffer &buf,
                         const id::room &room_id)
{
	string_view ret;
	column(room_id, [&ret, &buf]
	(const string_view &head)
	{
		ret = { data(buf), strlcpy(buf, head) };
	});

	return ret;
}

/// View a node by ID.
void
ircd::m::state::get_node(const string_view &node_id,
                         const node_closure &closure)
{
	db::column column
	{
		*event::events, "state_node"
	};

	get_node(column, node_id, closure);
}

/// View a node by ID. This can be used when user already has a reference
/// to the db column.
void
ircd::m::state::get_node(db::column &column,
                         const string_view &node_id,
                         const node_closure &closure)
{
	column(node_id, closure);
}

/// Writes a node to the db::txn and returns the id of this node (a hash) into
/// the buffer. The template allows for arguments to be forwarded to your
/// choice of the non-template make_node() overloads (exclude their leading
/// `out` buffer parameter).
template<class... args>
ircd::string_view
ircd::m::state::set_node(db::txn &iov,
                         const mutable_buffer &hashbuf,
                         args&&... a)
{
	thread_local char buf[NODE_MAX_SZ];
	const ctx::critical_assertion ca;

	const json::object node
	{
		make_node(buf, std::forward<args>(a)...)
	};

	const sha256::buf hash
	{
		sha256{const_buffer{node}}
	};

	const auto hashb64
	{
		b64encode_unpadded(hashbuf, hash)
	};

	db::txn::append
	{
		iov, db::delta
		{
			db::op::SET,
			"state_node",  // col
			hashb64,       // key
			node,          // val
		}
	};

	return hashb64;
}

/// Add key/val pair to an existing node, which creates a new node printed
/// into the buffer `out`. If the key matches an existing key, it will be
/// replaced and the new node will have the same size as old.
ircd::json::object
ircd::m::state::make_node(const mutable_buffer &out,
                          const node &old,
                          const size_t &pos,
                          const json::array &key,
                          const string_view &val)
{
	json::array keys[old.keys() + 1];
	size_t kn{0};
	{
		size_t n(0);
		while(kn < pos)
			keys[kn++] = old.key(n++);

		keys[kn++] = key;
		if(pos < old.keys() && keycmp(key, old.key(n)) == 0)
			n++;

		while(kn < old.keys() + 1 && n < old.keys())
			keys[kn++] = old.key(n++);
	}

	string_view vals[old.vals() + 1];
	size_t vn{0};
	{
		size_t n(0);
		while(vn < pos)
			vals[vn++] = old.val(n++);

		vals[vn++] = val;
		if(kn == old.keys())
			n++;

		while(vn < old.vals() + 1 && n < old.vals())
			vals[vn++] = old.val(n++);
	}

	assert(kn == old.keys() || kn == old.keys() + 1);
	return make_node(out, keys, kn, vals, vn);
}

/// Prints a node into the buffer `out` using the keys and vals arguments
/// which must be pointers to arrays. Size of each array is specified in
/// the following argument. Each array must have at least one element each.
/// the chld array can have one more element than the keys array if desired.
ircd::json::object
ircd::m::state::make_node(const mutable_buffer &out,
                          const json::array *const &keys_,
                          const size_t &kn,
                          const string_view *const &vals_,
                          const size_t &vn,
                          const string_view *const &chld_,
                          const size_t &cn)
{
	assert(kn > 0 && vn > 0);
	assert(kn == vn);
	assert(cn <= kn + 1);

	json::value keys[kn];
	{
		for(size_t i(0); i < kn; ++i)
			keys[i] = keys_[i];
	}

	json::value vals[vn];
	{
		for(size_t i(0); i < vn; ++i)
			vals[i] = vals_[i];
	};

	json::value chld[cn];
	{
		for(size_t i(0); i < cn; ++i)
			chld[i] = chld_[i];
	};

	json::iov iov;
	const json::iov::push push[]
	{
		{ iov, { "k"_sv, { keys, kn } } },
		{ iov, { "v"_sv, { vals, vn } } },
		{ iov, { "c"_sv, { chld, cn } } },
	};

	return { data(out), json::print(out, iov) };
}

/// Convenience to close over the key creation using a stack buffer (hence
/// safe for reentrance / multiple closing)
void
ircd::m::state::make_key(const string_view &type,
                         const string_view &state_key,
                         const key_closure &closure)
{
	char buf[KEY_MAX_SZ];
	closure(make_key(buf, type, state_key));
}

/// Creates a key array from the most common key pattern of a matrix
/// room (type,state_key).
ircd::json::array
ircd::m::state::make_key(const mutable_buffer &out,
                         const string_view &type,
                         const string_view &state_key)
{
	const json::value key_parts[]
	{
		type, state_key
	};

	const json::value key
	{
		key_parts, 2
	};

	return { data(out), json::print(out, key) };
}

/// Compares two keys. Keys are arrays of strings which become safely
/// concatenated for a linear lexical comparison. Returns -1 if a less
/// than b; 0 if equal; 1 if a greater than b.
int
ircd::m::state::keycmp(const json::array &a,
                       const json::array &b)
{
	auto ait(begin(a));
	auto bit(begin(b));
	for(; ait != end(a) && bit != end(b); ++ait, ++bit)
	{
		assert(surrounds(*ait, '"'));
		assert(surrounds(*bit, '"'));

		if(*ait < *bit)
			return -1;

		if(*bit < *ait)
			return 1;
	}

	assert(ait == end(a) || bit == end(b));
	return ait == end(a) && bit != end(b)?  -1:
	       ait == end(a) && bit == end(b)?   0:
	                                         1;
}

//
// node
//

/// Find position for a val in node. Uses the keycmp(). If there is one
/// key in node, and the argument compares less than or equal to the key,
/// 0 is returned, otherwise 1 is returned. If there are two keys in node
/// and argument compares less than both, 0 is returned; equal to key[0],
/// 0 is returned; greater than key[0] and less than or equal to key[1],
/// 1 is returned; greater than both: 2 is returned. Note that there can
/// be one more vals() than keys() in a node (this is usually a "full node")
/// but there might not be, and the returned pos might be out of range.
size_t
ircd::m::state::node::find(const json::array &parts)
const
{
	size_t ret{0};
	for(const json::array key : json::get<"k"_>(*this))
		if(keycmp(parts, key) <= 0)
			return ret;
		else
			++ret;

	return ret;
}

// Count values that actually lead to other nodes
bool
ircd::m::state::node::has_child(const size_t &pos)
const
{
	return !empty(child(pos));
}

ircd::string_view
ircd::m::state::node::child(const size_t &pos)
const
{
	const json::array children{json::get<"c"_>(*this, json::empty_array)};
	return unquote(children[pos]);
}

// Get value at position pos (throws out_of_range)
ircd::string_view
ircd::m::state::node::val(const size_t &pos)
const
{
	const json::array values{json::get<"v"_>(*this, json::empty_array)};
	return unquote(values[pos]);
}

// Get key at position pos (throws out_of_range)
ircd::json::array
ircd::m::state::node::key(const size_t &pos)
const
{
	const json::array keys{json::get<"k"_>(*this, json::empty_array)};
	const json::array ret{keys[pos]};
	return ret;
}

// Count children in node
size_t
ircd::m::state::node::childs()
const
{
	size_t ret(0);
	for(const auto &c : json::get<"c"_>(*this))
		ret += !empty(c);

	return ret;
}

// Count values in node
size_t
ircd::m::state::node::vals()
const
{
	return json::get<"v"_>(*this).count();
}

/// Count keys in node
size_t
ircd::m::state::node::keys()
const
{
	return json::get<"k"_>(*this).count();
}
