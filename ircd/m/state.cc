// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

/// Convenience to make a key and then get a value
void
ircd::m::state::get(const string_view &root,
                    const string_view &type,
                    const string_view &state_key,
                    const val_closure &closure)
{
	if(!get(std::nothrow, root, type, state_key, closure))
		throw m::NOT_FOUND
		{
			"type='%s' state_key='%s' not found in tree %s",
			type,
			state_key,
			root
		};
}

/// Convenience to make a key and then get a value (doesn't throw NOT_FOUND)
bool
ircd::m::state::get(std::nothrow_t,
                    const string_view &root,
                    const string_view &type,
                    const string_view &state_key,
                    const val_closure &closure)
{
	char key[KEY_MAX_SZ];
	return get(std::nothrow, root, make_key(key, type, state_key), closure);
}

/// throws m::NOT_FOUND if the exact key and its value does not exist.
void
ircd::m::state::get(const string_view &root,
                    const json::array &key,
                    const val_closure &closure)
{
	if(!get(std::nothrow, root, key, closure))
		throw m::NOT_FOUND
		{
			"%s not found in tree %s",
			string_view{key},
			root
		};
}

/// Recursive query to find the leaf value for the given key, starting from
/// the given root node ID. Value can be viewed in the closure. Returns false
/// if the exact key and its value does not exist in the tree; no node ID's
/// are ever returned here.
bool
ircd::m::state::get(std::nothrow_t,
                    const string_view &root,
                    const json::array &key,
                    const val_closure &closure)
{
	bool ret{false};
	char nextbuf[ID_MAX_SZ];
	string_view nextid{root};
	const auto node_closure{[&ret, &nextbuf, &nextid, &key, &closure]
	(const node &node)
	{
		auto pos(node.find(key));
		if(pos < node.keys() && node.key(pos) == key)
		{
			ret = true;
			nextid = {};
			closure(node.val(pos));
			return;
		}

		const auto c(node.childs());
		if(c && pos >= c)
			pos = c - 1;

		if(node.has_child(pos))
			nextid = { nextbuf, strlcpy(nextbuf, node.child(pos)) };
		else
			nextid = {};
	}};

	while(nextid)
		if(!get_node(std::nothrow, nextid, node_closure))
			return false;

	return ret;
}

size_t
ircd::m::state::accumulate(const string_view &root,
                           const iter_bool_closure &closure)
{
	size_t ret{0};
	for_each(root, [&ret, &closure]
	(const json::array &key, const string_view &val)
	{
		ret += closure(key, val);
	});

	return ret;
}

void
ircd::m::state::for_each(const string_view &root,
                         const iter_closure &closure)
{
	test(root, [&closure]
	(const json::array &key, const string_view &val)
	{
		closure(key, val);
		return false;
	});
}

void
ircd::m::state::for_each(const string_view &root,
                         const string_view &type,
                         const iter_closure &closure)
{
	test(root, type, [&closure]
	(const json::array &key, const string_view &val)
	{
		closure(key, val);
		return false;
	});
}

bool
ircd::m::state::test(const string_view &root,
                     const iter_bool_closure &closure)
{
	return dfs(root, [&closure]
	(const json::array &key, const string_view &val, const uint &, const uint &)
	{
		return closure(key, val);
	});
}

bool
ircd::m::state::test(const string_view &root,
                     const string_view &type,
                     const iter_bool_closure &closure)
{
	char buf[KEY_MAX_SZ];
	const json::array key
	{
		make_key(buf, type)
	};

	return dfs(root, key, [&closure]
	(const json::array &key, const string_view &val, const uint &, const uint &)
	{
		return closure(key, val);
	});
}

bool
ircd::m::state::test(const string_view &root,
                     const string_view &type,
                     const string_view &state_key_lb,
                     const iter_bool_closure &closure)
{
	char buf[KEY_MAX_SZ];
	const json::array key
	{
		make_key(buf, type, state_key_lb)
	};

	return dfs(root, key, [&closure]
	(const json::array &key, const string_view &val, const uint &, const uint &)
	{
		return closure(key, val);
	});
}

namespace ircd::m::state
{
	size_t _count_recurse(const node &, const json::array &key, const json::array &dom);
	size_t _count(const string_view &root, const json::array &key);
}

size_t
ircd::m::state::count(const string_view &root)
{
	return 0;
}

size_t
ircd::m::state::count(const string_view &root,
                      const string_view &type)
{
	char buf[KEY_MAX_SZ];
	const json::array key
	{
		make_key(buf, type)
	};

	return _count(root, key);
}

size_t
ircd::m::state::_count(const string_view &root,
                       const json::array &key)
{
	size_t ret{0};
	get_node(root, [&key, &ret]
	(const auto &node)
	{
		ret += _count_recurse(node, key, json::array{});
	});

	return ret;
}

size_t
ircd::m::state::_count_recurse(const node &node,
                               const json::array &key,
                               const json::array &dom)
{
	const node::rep rep{node};

	bool under{!empty(dom)};
	for(uint pos(0); under && pos < rep.kn; ++pos)
		if(!prefix_eq(dom, rep.keys[pos]))
			under = false;

	if(under)
		return rep.totals();

	size_t ret{0};
	const auto kpos{rep.find(key)};
	for(uint pos(kpos); pos < rep.kn || pos < rep.cn; ++pos)
	{
		if(!empty(rep.chld[pos]))
			get_node(rep.chld[pos], [&key, &ret, &rep, &pos]
			(const auto &node)
			{
				ret += _count_recurse(node, key, rep.keys[pos]);
			});

		if(pos < rep.kn)
		{
			if(prefix_eq(key, rep.keys[pos]))
				++ret;
			else
				break;
		}
	}

	return ret;
}

namespace ircd::m::state
{
	bool _dfs_recurse(const search_closure &, const node &, const json::array &key, int &);
}

bool
ircd::m::state::dfs(const string_view &root,
                    const search_closure &closure)
{
	return dfs(root, json::array{}, closure);
}

bool
ircd::m::state::dfs(const string_view &root,
                    const json::array &key,
                    const search_closure &closure)
{
	bool ret{true};
	get_node(root, [&closure, &key, &ret]
	(const auto &node)
	{
		int depth(-1);
		ret = _dfs_recurse(closure, node, key, depth);
	});

	return ret;
}

bool
ircd::m::state::_dfs_recurse(const search_closure &closure,
                             const node &node,
                             const json::array &key,
                             int &depth)
{
	++depth;
	const unwind down{[&depth]
	{
		--depth;
	}};

	const node::rep rep{node};
	const auto kpos{rep.find(key)};
	for(uint pos(kpos); pos < rep.kn || pos < rep.cn; ++pos)
	{
		if(!empty(rep.chld[pos]))
		{
			bool ret{false};
			get_node(rep.chld[pos], [&closure, &key, &depth, &ret]
			(const auto &node)
			{
				ret = _dfs_recurse(closure, node, key, depth);
			});

			if(ret)
				return true;
		}

		if(rep.kn <= pos)
			continue;

		if(!empty(key) && !prefix_eq(key, rep.keys[pos]))
			break;

		if(closure(rep.keys[pos], rep.vals[pos], depth, pos))
			return true;
	}

	return false;
}

// Internal operations
namespace ircd::m::state
{
	static mutable_buffer _getbuffer(const uint8_t &height);

	static string_view _remove(int8_t &height, db::txn &, const json::array &key, const node &node, const mutable_buffer &idbuf, node::rep &push);

	static string_view _insert_overwrite(db::txn &, const json::array &key, const string_view &val, const mutable_buffer &idbuf, node::rep &, const size_t &pos);
	static string_view _insert_leaf_nonfull(db::txn &, const json::array &key, const string_view &val, const mutable_buffer &idbuf, node::rep &, const size_t &pos);
	static json::object _insert_leaf_full(const int8_t &height, db::txn &, const json::array &key, const string_view &val, node::rep &, const size_t &pos, node::rep &push);
	static string_view _insert_branch_nonfull(db::txn &, const mutable_buffer &idbuf, node::rep &, const size_t &pos, node::rep &pushed);
	static json::object _insert_branch_full(const int8_t &height, db::txn &, node::rep &, const size_t &pos, node::rep &push, const node::rep &pushed);
	static string_view _insert(int8_t &height, db::txn &, const json::array &key, const string_view &val, const node &node, const mutable_buffer &idbuf, node::rep &push);

	static string_view _create(db::txn &, const mutable_buffer &root, const string_view &type, const string_view &state_key, const string_view &val);
}

/// State update from an event. Leaves the root node ID in the root buffer;
/// returns view.
///
ircd::m::state::id
ircd::m::state::insert(db::txn &txn,
                       const mutable_buffer &rootout,
                       const string_view &rootin,
                       const event &event)
{
	const auto &type{at<"type"_>(event)};
	const auto &state_key{at<"state_key"_>(event)};
	const auto &event_id{at<"event_id"_>(event)};
	assert(defined(state_key));

	if(empty(rootin))
		return _create(txn, rootout, type, state_key, event_id);

	return insert(txn, rootout, rootin, type, state_key, event_id);
}

ircd::m::state::id
ircd::m::state::_create(db::txn &txn,
                        const mutable_buffer &root,
                        const string_view &type,
                        const string_view &state_key,
                        const string_view &val)
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
	rep.vals[0] = val;
	rep.vn = 1;
	rep.chld[0] = string_view{};
	rep.cn = 1;
	rep.cnts[0] = 0;
	rep.nn = 1;

	return set_node(txn, root, rep.write(node));
}

/// State update for room_id inserting (type,state_key) = event_id into the
/// tree. Leaves the root node ID in the root buffer; returns view.
ircd::m::state::id
ircd::m::state::insert(db::txn &txn,
                       const mutable_buffer &rootout,
                       const string_view &rootin,
                       const string_view &type,
                       const string_view &state_key,
                       const m::id::event &event_id)
{
	// The insertion process reads from the DB and will yield this ircd::ctx
	// so the key buffer must stay on this stack.
	char key[KEY_MAX_SZ];
	return insert(txn, rootout, rootin, make_key(key, type, state_key), event_id);
}

ircd::m::state::id
ircd::m::state::insert(db::txn &txn,
                       const mutable_buffer &rootout,
                       const string_view &rootin,
                       const json::array &key,
                       const m::id::event &event_id)
{
	node::rep push;
	int8_t height{0};
	string_view root{rootin};
	get_node(root, [&](const node &node)
	{
		root = _insert(height, txn, key, event_id, node, rootout, push);
	});

	if(push.kn)
		root = push.write(txn, rootout);

	return root;
}

ircd::m::state::id
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
	rep.cnts[pos]++;
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

	rep.cnts[pos] = pushed.cnts[0];
	rep.cnts[pos + 1] = pushed.cnts[1];
	++rep.nn;

	size_t i(0);
	node::rep left;
	for(; i < rep.kn / 2; ++i)
	{
		left.keys[left.kn++] = rep.keys[i];
		left.vals[left.vn++] = rep.vals[i];
		left.chld[left.cn++] = rep.chld[i];
		left.cnts[left.nn++] = rep.cnts[i];
	}
	left.chld[left.cn++] = rep.chld[i];
	left.cnts[left.nn++] = rep.cnts[i];

	push.keys[push.kn++] = rep.keys[i];
	push.vals[push.vn++] = rep.vals[i];

	node::rep right;
	for(++i; i < rep.kn; ++i)
	{
		right.keys[right.kn++] = rep.keys[i];
		right.vals[right.vn++] = rep.vals[i];
		right.chld[right.cn++] = rep.chld[i];
		right.cnts[right.nn++] = rep.cnts[i];
	}
	right.chld[right.cn++] = rep.chld[i];
	right.cnts[right.nn++] = rep.cnts[i];

	thread_local char lc[ID_MAX_SZ], rc[ID_MAX_SZ];
	push.chld[push.cn++] = left.write(txn, lc);
	push.chld[push.cn++] = right.write(txn, rc);
	push.cnts[push.nn++] = left.totals();
	push.cnts[push.nn++] = right.totals();

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
		left.cnts[left.nn++] = 0;
	}

	push.keys[push.kn++] = rep.keys[i];
	push.vals[push.vn++] = rep.vals[i];

	node::rep right;
	for(++i; i < rep.kn; ++i)
	{
		right.keys[right.kn++] = rep.keys[i];
		right.vals[right.vn++] = rep.vals[i];
		right.chld[right.cn++] = string_view{};
		right.cnts[right.nn++] = 0;
	}

	thread_local char lc[ID_MAX_SZ], rc[ID_MAX_SZ];
	push.chld[push.cn++] = left.write(txn, lc);
	push.chld[push.cn++] = right.write(txn, rc);
	push.cnts[push.nn++] = left.totals();
	push.cnts[push.nn++] = right.totals();

	const auto ret
	{
		push.write(_getbuffer(height))
	};

	// Courtesy reassignment of all the references in `push` after rewrite.
	push = state::node{ret};
	return ret;
}

ircd::m::state::id
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

	rep.cnts[pos] = pushed.cnts[0];
	rep.cnts[pos + 1] = pushed.cnts[1];
	++rep.nn;

	return rep.write(txn, idbuf);
}

ircd::m::state::id
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

	rep.cnts[pos] = 0;
	++rep.nn;

	return rep.write(txn, idbuf);
}

ircd::m::state::id
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

ircd::m::state::id
ircd::m::state::remove(db::txn &txn,
                       const mutable_buffer &rootout,
                       const string_view &rootin,
                       const event &event)
{
	const auto &type{at<"type"_>(event)};
	const auto &state_key{at<"state_key"_>(event)};

	assert(!empty(rootin));
	return remove(txn, rootout, rootin, type, state_key);
}

/// State update for room_id inserting (type,state_key) = event_id into the
/// tree. Leaves the root node ID in the root buffer; returns view.
ircd::m::state::id
ircd::m::state::remove(db::txn &txn,
                       const mutable_buffer &rootout,
                       const string_view &rootin,
                       const string_view &type,
                       const string_view &state_key)
{
	// The removal process reads from the DB and will yield this ircd::ctx
	// so the key buffer must stay on this stack.
	char key[KEY_MAX_SZ];
	return remove(txn, rootout, rootin, make_key(key, type, state_key));
}

ircd::m::state::id
ircd::m::state::remove(db::txn &txn,
                       const mutable_buffer &rootout,
                       const string_view &rootin,
                       const json::array &key)
{
	node::rep push;
	int8_t height{0};
	string_view root{rootin};
	get_node(root, [&](const node &node)
	{
		root = _remove(height, txn, key, node, rootout, push);
	});

	if(push.kn)
		root = push.write(txn, rootout);

	return root;
}

ircd::m::state::id
ircd::m::state::_remove(int8_t &height,
                        db::txn &txn,
                        const json::array &key,
                        const node &node,
                        const mutable_buffer &idbuf,
                        node::rep &push)
{
	const unwind down{[&height]{ --height; }};
	if(unlikely(++height >= MAX_HEIGHT))
		throw assertive{"recursion limit exceeded"};

	node::rep rep{node};
	const auto pos{node.find(key)};

	if(keycmp(node.key(pos), key) == 0)
	{

		return {};
	}

	// These collect data from the next level.
	node::rep pushed;
	string_view child;

	// Recurse
	get_node(node.child(pos), [&](const auto &node)
	{

		child = _remove(height, txn, key, node, idbuf, pushed);
	});

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

/// View a node by ID. This makes a DB query and may yield ircd::ctx.
void
ircd::m::state::get_node(const string_view &node_id,
                         const node_closure &closure)
{
	if(!get_node(std::nothrow, node_id, closure))
		throw m::NOT_FOUND
		{
			"node_id %s not found",
			string_view{node_id}
		};
}

/// View a node by ID. This makes a DB query and may yield ircd::ctx.
bool
ircd::m::state::get_node(const std::nothrow_t,
                         const string_view &node_id,
                         const node_closure &closure)
{
	assert(bool(dbs::state_node));
	auto &column{dbs::state_node};
	return column(node_id, std::nothrow, closure);
}

/// Writes a node to the db::txn and returns the id of this node (a hash) into
/// the buffer.
ircd::m::state::id
ircd::m::state::set_node(db::txn &iov,
                         const mutable_buffer &hashbuf,
                         const json::object &node)
{
	const sha256::buf hash
	{
		sha256{node}
	};

	const auto hashb64
	{
		b64encode_unpadded(hashbuf, hash)
	};

	db::txn::append
	{
		iov, dbs::state_node,
		{
			db::op::SET,
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

ircd::json::array
ircd::m::state::make_key(const mutable_buffer &out,
                         const string_view &type)
{
	const json::value key_parts[]
	{
		type
	};

	const json::value key
	{
		key_parts, 1
	};

	return { data(out), json::print(out, key) };
}

bool
ircd::m::state::prefix_eq(const json::array &a,
                          const json::array &b)
{
	ushort i(0);
	auto ait(begin(a));
	auto bit(begin(b));
	for(; ait != end(a) && bit != end(b) && i < 2; ++ait, ++bit)
	{
		assert(surrounds(*ait, '"'));
		assert(surrounds(*bit, '"'));

		if(*ait == *bit)
		{
			if(i)
				return false;
		}
		else ++i;
	}

	return ait != end(a) || bit != end(b)? i == 0 : i < 2;
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
,nn{node.counts(cnts.data(), cnts.size())}
{
	assert(cn == nn);
}

ircd::m::state::id
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
	assert(cn == nn);
	assert(cn <= kn + 1);
	assert(!childs() || childs() > kn);
	assert(!duplicates());

	assert(kn > 0 && vn > 0);
	assert(kn <= NODE_MAX_KEY);
	assert(vn <= NODE_MAX_VAL);
	assert(cn <= NODE_MAX_DEG);

	std::array<json::value, NODE_MAX_KEY> keys;
	{
		for(size_t i(0); i < kn; ++i)
			keys[i] = this->keys[i];
	}

	std::array<json::value, NODE_MAX_VAL> vals;
	{
		for(size_t i(0); i < vn; ++i)
			vals[i] = this->vals[i];
	};

	std::array<json::value, NODE_MAX_DEG> chld;
	{
		for(size_t i(0); i < cn; ++i)
			chld[i] = this->chld[i];
	};

	std::array<json::value, NODE_MAX_DEG> cnts;
	{
		for(size_t i(0); i < nn; ++i)
			cnts[i] = json::value{long(this->cnts[i])};
	};

	json::iov iov;
	const json::iov::push push[]
	{
		{ iov, { name::key,   { keys.data(), kn } } },
		{ iov, { name::val,   { vals.data(), vn } } },
		{ iov, { name::child, { chld.data(), cn } } },
		{ iov, { name::count, { cnts.data(), nn } } },
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
	std::copy_backward(begin(cnts) + pos, begin(cnts) + nn, begin(cnts) + nn + 1);
}

/// Shift left.
void
ircd::m::state::node::rep::shl(const size_t &pos)
{
	std::copy(begin(keys) + pos + 1, begin(keys) + kn, begin(keys) + std::max(ssize_t(kn) - 1, 0L));
	std::copy(begin(vals) + pos + 1, begin(vals) + vn, begin(vals) + std::max(ssize_t(vn) - 1, 0L));
	std::copy(begin(chld) + pos + 1, begin(chld) + cn, begin(chld) + std::max(ssize_t(cn) - 1, 0L));
	std::copy(begin(cnts) + pos + 1, begin(cnts) + nn, begin(cnts) + std::max(ssize_t(nn) - 1, 0L));
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
ircd::m::state::node::rep::totals()
const
{
	return kn + counts();
}

size_t
ircd::m::state::node::rep::counts()
const
{
	size_t ret(0);
	for(size_t i(0); i < nn; ++i)
		ret += cnts[i];

	return ret;
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
	for(const json::array key : json::get<name::key>(*this))
		if(keycmp(parts, key) <= 0)
			return ret;
		else
			++ret;

	return ret;
}

size_t
ircd::m::state::node::counts(size_t *const &out,
                             const size_t &max)
const
{
	size_t i(0);
	for(const string_view &c : json::get<name::count>(*this))
		if(likely(i < max))
			out[i++] = lex_cast<size_t>(c);

	return i;
}

size_t
ircd::m::state::node::childs(state::id *const &out,
                             const size_t &max)
const
{
	size_t i(0);
	for(const string_view &c : json::get<name::child>(*this))
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
	for(const string_view &v : json::get<name::val>(*this))
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
	for(const json::array &k : json::get<name::key>(*this))
		if(likely(i < max))
			out[i++] = k;

	return i;
}

size_t
ircd::m::state::node::count(const size_t &pos)
const
{
	const json::array &counts
	{
		json::get<name::count>(*this, json::empty_array)
	};

	return counts.at<size_t>(pos);
}

ircd::m::state::id
ircd::m::state::node::child(const size_t &pos)
const
{
	const json::array &children
	{
		json::get<name::child>(*this, json::empty_array)
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
		json::get<name::val>(*this, json::empty_array)
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
		json::get<name::key>(*this, json::empty_array)
	};

	return keys[pos];
}

// Count counts in node
size_t
ircd::m::state::node::totals()
const
{
	return keys() + counts();
}

// Count counts in node
size_t
ircd::m::state::node::counts()
const
{
	size_t ret(0);
	for(const auto &c : json::get<name::count>(*this))
		ret += lex_cast<size_t>(c);

	return ret;
}

// Count children in node
size_t
ircd::m::state::node::childs()
const
{
	size_t ret(0);
	for(const auto &c : json::get<name::child>(*this))
		ret += !empty(c) && c != json::empty_string;

	return ret;
}

// Count values in node
size_t
ircd::m::state::node::vals()
const
{
	return json::get<name::val>(*this).count();
}

/// Count keys in node
size_t
ircd::m::state::node::keys()
const
{
	return json::get<name::key>(*this).count();
}
