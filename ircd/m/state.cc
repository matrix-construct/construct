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

/// Convenience to get value from the current room head.
void
ircd::m::state::get__room(const id::room &room_id,
                          const string_view &type,
                          const string_view &state_key,
                          const id_closure &closure)
{
	char head[ID_MAX_SZ];
	return get(get_head(head, room_id), type, state_key, closure);
}

/// Convenience to get value making a key
void
ircd::m::state::get(const string_view &head,
                    const string_view &type,
                    const string_view &state_key,
                    const id_closure &closure)
{
	char key[KEY_MAX_SZ];
	return get(head, make_key(key, type, state_key), closure);
}

/// see: get(); user does not have to supply column reference here
void
ircd::m::state::get(const string_view &head,
                    const json::array &key,
                    const id_closure &closure)
{
	db::column column
	{
		*event::events, "state_node"
	};

	get(column, head, key, closure);
}

/// Recursive query to find the leaf value for the given key, starting from
/// the given head node ID. Value can be viewed in the closure. This throws
/// m::NOT_FOUND if the exact key and its value does not exist in the tree;
/// no node ID's are ever returned here.
void
ircd::m::state::get(db::column &column,
                    const string_view &head,
                    const json::array &key,
                    const id_closure &closure)
{
	char nextbuf[ID_MAX_SZ];
	string_view nextid{head};
	while(nextid) get_node(column, nextid, [&](const node &node)
	{
		auto pos(node.find(key));
		if(pos < node.keys() && node.key(pos) == key)
		{
			nextid = {};
			closure(node.val(pos));
			return;
		}

		const auto c(node.childs());
		if(c && pos >= c)
			pos = c - 1;

		if(!node.has_child(pos))
			throw m::NOT_FOUND{};

		nextid = { nextbuf, strlcpy(nextbuf, node.child(pos)) };
	});
}

namespace ircd::m::state
{
	bool _dfs_recurse(db::column &, const search_closure &, const node &, int &);
}

bool
ircd::m::state::dfs(const string_view &node_id,
                    const search_closure &closure)
{
	db::column column
	{
		*event::events, "state_node"
	};

	return dfs(column, node_id, closure);
}

bool
ircd::m::state::dfs(db::column &column,
                    const string_view &node_id,
                    const search_closure &closure)
{
	bool ret{false};
	get_node(column, node_id, [&column, &closure, &ret]
	(const auto &node)
	{
		int depth(-1);
		ret = _dfs_recurse(column, closure, node, depth);
	});

	return ret;
}

bool
ircd::m::state::_dfs_recurse(db::column &column,
                             const search_closure &closure,
                             const node &node,
                             int &depth)
{
	++depth;
	const unwind down{[&depth]
	{
		--depth;
	}};

	const node::rep rep{node};
	for(uint pos(0); pos < rep.kn || pos < rep.cn; ++pos)
	{
		const auto child
		{
			unquote(rep.chld[pos])
		};

		if(!empty(child))
		{
			bool ret{false};
			get_node(column, child, [&column, &closure, &depth, &ret]
			(const auto &node)
			{
				ret = _dfs_recurse(column, closure, node, depth);
			});

			if(ret)
				return true;
		}

		if(rep.kn > pos)
		{
			const auto &key{rep.keys[pos]};
			const auto &val{unquote(rep.vals[pos])};
			if(closure(key, val, depth, pos))
				return true;
		}
	}

	return false;
}

// Internal insertion operations
namespace ircd::m::state
{
	static mutable_buffer _getbuffer(const uint8_t &height);
	static string_view _insert_overwrite(db::txn &txn, const json::array &key, const string_view &val, const mutable_buffer &idbuf, node::rep &rep, const size_t &pos);
	static string_view _insert_leaf_nonfull(db::txn &txn, const json::array &key, const string_view &val, const mutable_buffer &idbuf, node::rep &rep, const size_t &pos);
	static json::object _insert_leaf_full(const int8_t &height, db::txn &txn, const json::array &key, const string_view &val, node::rep &rep, const size_t &pos, node::rep &push);
	static string_view _insert_branch_nonfull(db::txn &txn, const mutable_buffer &idbuf, node::rep &rep, const size_t &pos, node::rep &pushed);
	static json::object _insert_branch_full(const int8_t &height, db::txn &txn, node::rep &rep, const size_t &pos, node::rep &push, const node::rep &pushed);
	static string_view _insert(int8_t &height, db::txn &txn, const json::array &key, const string_view &val, const node &node, const mutable_buffer &idbuf, node::rep &push);
}

/// State update from an event. (NOTE: sets the room head atm)
/// Leaves the root node ID in the head buffer; returns view.
ircd::string_view
ircd::m::state::insert(db::txn &txn,
                       const mutable_buffer &head,
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
		thread_local char node[NODE_MAX_SZ];

		node::rep rep;
		rep.keys[0] = make_key(key, type, state_key);
		rep.kn = 1;
		rep.vals[0] = event_id;
		rep.vn = 1;
		rep.chld[0] = string_view{};
		rep.cn = 1;

		return set_head(txn, room_id, set_node(txn, head, rep.write(node)));
	}

	return insert(txn, head, room_id, type, state_key, event_id);
}

/// State update for room_id inserting (type,state_key) = event_id into the
/// tree. Leaves the root node ID in the head buffer; returns view.
/// (NOTE: sets the room head in the txn right now).
ircd::string_view
ircd::m::state::insert(db::txn &txn,
                       const mutable_buffer &head,
                       const id::room &room_id,
                       const string_view &type,
                       const string_view &state_key,
                       const id::event &event_id)
{
	// The insertion process reads from the DB and will yield this ircd::ctx
	// so the key buffer must stay on this stack.
	char key[KEY_MAX_SZ];
	return insert(txn, head, room_id, make_key(key, type, state_key), event_id);
}

ircd::string_view
ircd::m::state::insert(db::txn &txn,
                       const mutable_buffer &headbuf,
                       const id::room &room_id,
                       const json::array &key,
                       const id::event &event_id)
{
	db::column heads{*event::events, "state_head"};
	db::column nodes{*event::events, "state_node"};

	string_view head
	{
		get_head(heads, headbuf, room_id)
	};

	node::rep push;
	int8_t height{0};
	get_node(head, [&](const node &node)
	{
		head = _insert(height, txn, key, event_id, node, headbuf, push);
	});

	if(push.kn)
		head = push.write(txn, headbuf);

	return set_head(txn, room_id, head);
}

ircd::string_view
ircd::m::state::_insert(int8_t &height,
                        db::txn &txn,
                        const json::array &key,
                        const string_view &val,
                        const node &node,
                        const mutable_buffer &idbuf,
                        node::rep &push)
{
	// Recursion metrics
	const unwind down{[&height]{ --height; }};
	if(unlikely(++height >= MAX_HEIGHT))
		throw assertive{"recursion limit exceeded"};

	// This function assumes that any node argument is a previously "existing"
	// node which means it contains at least one key/value.
	assert(node.keys() > 0);
	assert(node.keys() == node.vals());

	node::rep rep{node};
	const auto pos{node.find(key)};

	if(keycmp(node.key(pos), key) == 0)
		return _insert_overwrite(txn, key, val, idbuf, rep, pos);

	if(node.childs() == 0 && rep.full())
		return _insert_leaf_full(height, txn, key, val, rep, pos, push);

	if(node.childs() == 0 && !rep.full())
		return _insert_leaf_nonfull(txn, key, val, idbuf, rep, pos);

	if(empty(node.child(pos)))
		return _insert_leaf_nonfull(txn, key, val, idbuf, rep, pos);

	// These collect data from the next level.
	node::rep pushed;
	string_view child;

	// Recurse
	get_node(node.child(pos), [&](const auto &node)
	{
		child = _insert(height, txn, key, val, node, idbuf, pushed);
	});

	// Child was pushed but that will stop here.
	if(pushed.kn && !rep.full())
		return _insert_branch_nonfull(txn, idbuf, rep, pos, pushed);

	// Most complex branch
	if(pushed.kn && rep.full())
		return _insert_branch_full(height, txn, rep, pos, push, pushed);

	// Indicates no push, and the child value is just an ID of a node.
	rep.chld[pos] = child;
	return rep.write(txn, idbuf);
}

ircd::json::object
ircd::m::state::_insert_branch_full(const int8_t &height,
                                    db::txn &txn,
                                    node::rep &rep,
                                    const size_t &pos,
                                    node::rep &push,
                                    const node::rep &pushed)
{
	rep.shr(pos);

	rep.keys[pos] = pushed.keys[0];
	++rep.kn;

	rep.vals[pos] = pushed.vals[0];
	++rep.vn;

	rep.chld[pos] = pushed.chld[0];
	rep.chld[pos + 1] = pushed.chld[1];
	++rep.cn;

	size_t i(0);
	node::rep left;
	for(; i < rep.kn / 2; ++i)
	{
		left.keys[left.kn++] = rep.keys[i];
		left.vals[left.vn++] = rep.vals[i];
		left.chld[left.cn++] = rep.chld[i];
	}
	left.chld[left.cn++] = rep.chld[i];

	push.keys[push.kn++] = rep.keys[i];
	push.vals[push.vn++] = rep.vals[i];

	node::rep right;
	for(++i; i < rep.kn; ++i)
	{
		right.keys[right.kn++] = rep.keys[i];
		right.vals[right.vn++] = rep.vals[i];
		right.chld[right.cn++] = rep.chld[i];
	}
	right.chld[right.cn++] = rep.chld[i];

	thread_local char lc[ID_MAX_SZ], rc[ID_MAX_SZ];
	push.chld[push.cn++] = left.write(txn, lc);
	push.chld[push.cn++] = right.write(txn, rc);

	const auto ret
	{
		push.write(_getbuffer(height))
	};

	// Courtesy reassignment of all the references in `push` after rewrite.
	push = state::node{ret};
	return ret;
}

ircd::json::object
ircd::m::state::_insert_leaf_full(const int8_t &height,
                                  db::txn &txn,
                                  const json::array &key,
                                  const string_view &val,
                                  node::rep &rep,
                                  const size_t &pos,
                                  node::rep &push)
{
	rep.shr(pos);

	rep.keys[pos] = key;
	++rep.kn;

	rep.vals[pos] = val;
	++rep.vn;

	size_t i(0);
	node::rep left;
	for(; i < rep.kn / 2; ++i)
	{
		left.keys[left.kn++] = rep.keys[i];
		left.vals[left.vn++] = rep.vals[i];
		left.chld[left.cn++] = string_view{};
	}

	push.keys[push.kn++] = rep.keys[i];
	push.vals[push.vn++] = rep.vals[i];

	node::rep right;
	for(++i; i < rep.kn; ++i)
	{
		right.keys[right.kn++] = rep.keys[i];
		right.vals[right.vn++] = rep.vals[i];
		right.chld[right.cn++] = string_view{};
	}

	thread_local char lc[ID_MAX_SZ], rc[ID_MAX_SZ];
	push.chld[push.cn++] = left.write(txn, lc);
	push.chld[push.cn++] = right.write(txn, rc);

	const auto ret
	{
		push.write(_getbuffer(height))
	};

	// Courtesy reassignment of all the references in `push` after rewrite.
	push = state::node{ret};
	return ret;
}

ircd::string_view
ircd::m::state::_insert_branch_nonfull(db::txn &txn,
                                       const mutable_buffer &idbuf,
                                       node::rep &rep,
                                       const size_t &pos,
                                       node::rep &pushed)
{
	rep.shr(pos);

	rep.keys[pos] = pushed.keys[0];
	++rep.kn;

	rep.vals[pos] = pushed.vals[0];
	++rep.vn;

	rep.chld[pos] = pushed.chld[0];
	rep.chld[pos + 1] = pushed.chld[1];
	++rep.cn;

	return rep.write(txn, idbuf);
}

ircd::string_view
ircd::m::state::_insert_leaf_nonfull(db::txn &txn,
                                     const json::array &key,
                                     const string_view &val,
                                     const mutable_buffer &idbuf,
                                     node::rep &rep,
                                     const size_t &pos)
{
	rep.shr(pos);

	rep.keys[pos] = key;
	++rep.kn;

	rep.vals[pos] = val;
	++rep.vn;

	rep.chld[pos] = string_view{};
	++rep.cn;

	return rep.write(txn, idbuf);
}

ircd::string_view
ircd::m::state::_insert_overwrite(db::txn &txn,
                                  const json::array &key,
                                  const string_view &val,
                                  const mutable_buffer &idbuf,
                                  node::rep &rep,
                                  const size_t &pos)
{
	rep.keys[pos] = key;
	rep.vals[pos] = val;

	return rep.write(txn, idbuf);
}

/// This function returns a thread_local buffer intended for writing temporary
/// nodes which may be "pushed" down the tree during the btree insertion
/// process. This is an alternative to allocating such space in each stack
/// frame when only one or two are ever used at a time -- but because more than
/// one may be used at a time during complex rebalances we have the user pass
/// their current recursion depth which is used to partition the buffer so they
/// don't overwrite their own data.
ircd::mutable_buffer
ircd::m::state::_getbuffer(const uint8_t &height)
{
	static const size_t buffers{2};
	using buffer_type = std::array<char, NODE_MAX_SZ>;
	thread_local std::array<buffer_type, buffers> buffer;
	return buffer.at(height % buffer.size());
}

/// Set the root node ID for a room in this db transaction.
ircd::string_view
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

	return head_id;
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
/// the buffer. The template allows for arguments to be forwarded to make_node()
ircd::string_view
ircd::m::state::set_node(db::txn &iov,
                         const mutable_buffer &hashbuf,
                         const json::object &node)
{
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
// rep
//

ircd::m::state::node::rep::rep(const node &node)
:kn{node.keys(keys.data(), keys.size())}
,vn{node.vals(vals.data(), vals.size())}
,cn{node.childs(chld.data(), chld.size())}
{
}

ircd::string_view
ircd::m::state::node::rep::write(db::txn &txn,
                                 const mutable_buffer &idbuf)
{
	thread_local char buf[NODE_MAX_SZ];
	return set_node(txn, idbuf, write(buf));
}

ircd::json::object
ircd::m::state::node::rep::write(const mutable_buffer &out)
{
	assert(kn == vn);
	assert(cn <= kn + 1);
	assert(!childs() || childs() > kn);
	assert(!duplicates());

	assert(kn > 0 && vn > 0);
	assert(kn <= NODE_MAX_KEY);
	assert(vn <= NODE_MAX_VAL);
	assert(cn <= NODE_MAX_DEG);

	json::value keys[kn];
	{
		for(size_t i(0); i < kn; ++i)
			keys[i] = this->keys[i];
	}

	json::value vals[vn];
	{
		for(size_t i(0); i < vn; ++i)
			vals[i] = this->vals[i];
	};

	json::value chld[cn];
	{
		for(size_t i(0); i < cn; ++i)
			chld[i] = this->chld[i];
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

/// Shift right.
void
ircd::m::state::node::rep::shr(const size_t &pos)
{
	std::copy_backward(begin(keys) + pos, begin(keys) + kn, begin(keys) + kn + 1);
	std::copy_backward(begin(vals) + pos, begin(vals) + vn, begin(vals) + vn + 1);
	std::copy_backward(begin(chld) + pos, begin(chld) + cn, begin(chld) + cn + 1);
}

size_t
ircd::m::state::node::rep::find(const json::array &parts)
const
{
	size_t i{0};
	for(; i < kn; ++i)
		if(keycmp(parts, keys[i]) <= 0)
			return i;

	return i;
}

size_t
ircd::m::state::node::rep::childs()
const
{
	size_t ret(0);
	for(size_t i(0); i < cn; ++i)
		if(!empty(unquote(chld[i])))
			++ret;

	return ret;
}

bool
ircd::m::state::node::rep::duplicates()
const
{
	for(size_t i(0); i < kn; ++i)
		for(size_t j(0); j < kn; ++j)
			if(j != i && keys[i] == keys[j])
				return true;

	for(size_t i(0); i < cn; ++i)
		if(!empty(unquote(chld[i])))
			for(size_t j(0); j < cn; ++j)
				if(j != i && chld[i] == chld[j])
					return true;

	return false;
}

bool
ircd::m::state::node::rep::overfull()
const
{
	assert(kn == vn);
	return kn > NODE_MAX_KEY;
}

bool
ircd::m::state::node::rep::full()
const
{
	assert(kn == vn);
	return kn >= NODE_MAX_KEY;
}

//
// node
//

// Count values that actually lead to other nodes
bool
ircd::m::state::node::has_child(const size_t &pos)
const
{
	return !empty(child(pos));
}

// Count values that actually lead to other nodes
bool
ircd::m::state::node::has_key(const json::array &key)
const
{
	const auto pos(find(key));
	if(pos >= keys())
		return false;

	return keycmp(this->key(pos), key) == 0;
}

/// Find position for a val in node. Uses the keycmp(). If there is one
/// key in node, and the argument compares less than or equal to the key,
/// 0 is returned, otherwise 1 is returned. If there are two keys in node
/// and argument compares less than both, 0 is returned; equal to key[0],
/// 0 is returned; greater than key[0] and less than or equal to key[1],
/// 1 is returned; greater than both: 2 is returned. Note that there can
/// be one more childs() than keys() in a node (this is usually a "full
/// node") but there might not be, and the returned pos might be out of
/// range.
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

size_t
ircd::m::state::node::childs(string_view *const &out,
                             const size_t &max)
const
{
	size_t i(0);
	for(const string_view &c : json::get<"c"_>(*this))
		if(likely(i < max))
			out[i++] = unquote(c);

	return i;
}

size_t
ircd::m::state::node::vals(string_view *const &out,
                           const size_t &max)
const
{
	size_t i(0);
	for(const string_view &v : json::get<"v"_>(*this))
		if(likely(i < max))
			out[i++] = unquote(v);

	return i;
}

size_t
ircd::m::state::node::keys(json::array *const &out,
                           const size_t &max)
const
{
	size_t i(0);
	for(const json::array &k : json::get<"k"_>(*this))
		if(likely(i < max))
			out[i++] = k;

	return i;
}

ircd::string_view
ircd::m::state::node::child(const size_t &pos)
const
{
	const json::array &children
	{
		json::get<"c"_>(*this, json::empty_array)
	};

	return unquote(children[pos]);
}

// Get value at position pos (throws out_of_range)
ircd::string_view
ircd::m::state::node::val(const size_t &pos)
const
{
	const json::array &values
	{
		json::get<"v"_>(*this, json::empty_array)
	};

	return unquote(values[pos]);
}

// Get key at position pos (throws out_of_range)
ircd::json::array
ircd::m::state::node::key(const size_t &pos)
const
{
	const json::array &keys
	{
		json::get<"k"_>(*this, json::empty_array)
	};

	return keys[pos];
}

// Count children in node
size_t
ircd::m::state::node::childs()
const
{
	size_t ret(0);
	for(const auto &c : json::get<"c"_>(*this))
		ret += !empty(c) && c != json::empty_string;

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
