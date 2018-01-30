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
ircd::m::state::insert(db::txn &txn,
                       const id::room &room_id,
                       const string_view &type,
                       const string_view &state_key,
                       const id::event &event_id)
{
	char key[512];
	return insert(txn, room_id, make_key(key, type, state_key), event_id);
}

void
ircd::m::state::insert(db::txn &txn,
                       const id::room &room_id,
                       const json::array &key,
                       const id::event &event_id)
{
	db::column column
	{
		*event::events, "state_node"
	};

	// Start with the root node ID for room
	char nextbuf[512];
	string_view nextid
	{
		get_head(room_id, nextbuf)
	};

	while(nextid)
	{
		get_node(column, nextid, [&](const auto &node)
		{
			std::cout << "@" << nextid << " " << node << std::endl;
			const auto pos(find(node, key));

			char headbuf[512];
			const auto head(set_into(txn, headbuf, node, pos, key, event_id));
			set_head(txn, room_id, head);
		});

		nextid = {};
	}
}

void
ircd::m::state::get_value__room(const id::room &room_id,
                                const string_view &type,
                                const string_view &state_key,
                                const id_closure &closure)
{
	char head[64];
	get_head(room_id, [&head](const string_view &id)
	{
		strlcpy(head, unquote(id));
	});

	return get_value(head, type, state_key, closure);
}

void
ircd::m::state::get_value(const string_view &head,
                          const string_view &type,
                          const string_view &state_key,
                          const id_closure &closure)
{
	char key[512];
	return get_value(head, make_key(key, type, state_key), closure);
}

void
ircd::m::state::get_value(const string_view &head,
                          const json::array &key,
                          const id_closure &closure)
{
	db::column column
	{
		*event::events, "state_node"
	};

	char nextbuf[512];
	string_view nextid{head}; do
	{
		get_node(column, nextid, [&key, &closure, &nextid, &nextbuf]
		(const auto &node)
		{
			const auto pos{find(node, key)};
			if(pos >= vals(node))
				throw m::NOT_FOUND{};

			const auto &v{unquote(val(node, pos))};
			if(valid(id::EVENT, v))
			{
				if(state::key(node, pos) != key)
					throw m::NOT_FOUND{};

				nextid = {};
				closure(v);
			}
			else nextid = { nextbuf, strlcpy(nextbuf, v) };
		});
	}
	while(nextid);
}

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

ircd::string_view
ircd::m::state::get_head(const id::room &room_id,
                         const mutable_buffer &buf)
{
	string_view ret;
	get_head(room_id, [&ret, &buf]
	(const string_view &head)
	{
		ret = { data(buf), strlcpy(buf, head) };
	});

	return ret;
}

void
ircd::m::state::get_head(const id::room &room_id,
                         const id_closure &closure)
{
	db::column column
	{
		*event::events, "state_head"
	};

	get_head(column, room_id, closure);
}

void
ircd::m::state::get_head(db::column &column,
                         const id::room &room_id,
                         const id_closure &closure)
{
	column(room_id, closure);
}

ircd::string_view
ircd::m::state::set_into(db::txn &iov,
                         const mutable_buffer &hashbuf,
                         const node &old,
                         const size_t &pos,
                         const json::array &key,
                         const string_view &val)
{
	thread_local char buf[2_KiB];
	const ctx::critical_assertion ca;

	const string_view node
	{
		make_into(buf, old, pos, key, val)
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

ircd::string_view
ircd::m::state::set_node(db::txn &iov,
                         const mutable_buffer &hashbuf,
                         const json::array *const &keys,
                         const size_t &kn,
                         const string_view *const &vals,
                         const size_t &vn)
{
	thread_local char buf[2_KiB];
	const ctx::critical_assertion ca;

	const string_view node
	{
		make_node(buf, keys, kn, vals, vn)
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

void
ircd::m::state::get_node(db::column &column,
                         const string_view &node_id,
                         const node_closure &closure)
{
	column(node_id, closure);
}

ircd::json::object
ircd::m::state::make_into(const mutable_buffer &out,
                          const node &old,
                          const size_t &pos,
                          const json::array &_key,
                          const string_view &_val)
{
	const size_t kn{keys(old) + 1};
	json::array _keys[kn];
	{
		size_t i(0), j(0);
		while(i < pos)
			_keys[i++] = key(old, j++);

		_keys[i++] = _key;
		while(i < keys(old) + 1)
			_keys[i++] = key(old, j++);
	}

	const size_t vn{vals(old) + 1};
	string_view _vals[vn + 1];
	{
		size_t i(0), j(0);
		while(i < pos)
			_vals[i++] = val(old, j++);

		_vals[i++] = _val;
		while(i < vals(old) + 1)
			_vals[i++] = val(old, j++);
	}

	return make_node(out, _keys, kn, _vals, vn);
}

ircd::json::object
ircd::m::state::make_node(const mutable_buffer &out,
                          const json::array *const &keys_,
                          const size_t &kn,
                          const string_view *const &vals_,
                          const size_t &vn)
{
	assert(kn > 0 && vn > 0);
	assert(kn == vn || kn + 1 == vn);

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

	json::iov iov;
	const json::iov::push push[]
	{
		{ iov, { "k"_sv, { keys, kn } } },
		{ iov, { "v"_sv, { vals, vn } } },
	};

	return { data(out), json::print(out, iov) };
}

size_t
ircd::m::state::find(const node &node,
                     const string_view &type,
                     const string_view &state_key)
{
	thread_local char buf[1_KiB];
	const ctx::critical_assertion ca;
	return find(node, make_key(buf, type, state_key));
}

size_t
ircd::m::state::find(const node &node,
                     const json::array &parts)
{
	size_t ret{0};
	for(const json::array key : json::get<"k"_>(node))
		if(keycmp(parts, key) <= 0)
			return ret;
		else
			++ret;

	return ret;
}

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

ircd::string_view
ircd::m::state::val(const node &node,
                    const size_t &pos)
{
	return json::at<"v"_>(node).at(pos);
}

ircd::json::array
ircd::m::state::key(const node &node,
                    const size_t &pos)
{
	return json::at<"k"_>(node).at(pos);
}

size_t
ircd::m::state::children(const node &node)
{
	size_t ret(0);
	for(const auto &v : json::get<"v"_>(node))
		if(!valid(id::EVENT, v))
			++ret;

	return ret;
}

size_t
ircd::m::state::vals(const node &node)
{
	return json::get<"v"_>(node).count();
}

size_t
ircd::m::state::keys(const node &node)
{
	return json::get<"k"_>(node).count();
}
