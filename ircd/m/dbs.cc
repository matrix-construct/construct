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

decltype(ircd::m::dbs::databases)
ircd::m::dbs::databases
{};

decltype(ircd::m::dbs::modules)
ircd::m::dbs::modules
{};

//
// init
//

ircd::m::dbs::init::init()
{
	_modules();
	_databases();

	ircd::m::event::events = databases.at("events").get();
}

ircd::m::dbs::init::~init()
noexcept
{
	ircd::m::event::events = nullptr;

	databases.clear();
	modules.clear();
}

void
ircd::m::dbs::init::_databases()
{
	for(const auto &pair : modules)
	{
		const auto &name(pair.first);
		const auto dbname(mods::unpostfixed(name));
		const std::string shortname(lstrip(dbname, "db_"));
		const std::string symname(shortname + "_database"s);
		databases.emplace(shortname, import_shared<database>
		{
			dbname, symname
		});
	}
}

void
ircd::m::dbs::init::_modules()
{
	for(const auto &name : mods::available())
		if(startswith(name, "db_"))
			modules.emplace(name, name);
}

//
//
//

void
ircd::m::dbs::write(const event &event,
                    db::iov &txn)
{
	db::iov::append
	{
		txn, at<"event_id"_>(event), event
	};

	//if(defined(json::get<"state_key"_>(event)))
	//	append_nodes(event, txn);

	append_indexes(event, txn);
}

void
ircd::m::dbs::append_nodes(const event &event,
                           db::iov &txn)
{
	thread_local char key_buf[1_KiB], hash_buf[64];

	const json::array key
	{
		make_key(key_buf, at<"type"_>(event), at<"state_key"_>(event))
	};

	const string_view hash
	{
		make_node(txn, hash_buf, { key }, { at<"event_id"_>(event) })
	};

}

//
// node
//

ircd::json::object
ircd::m::dbs::get_node(const mutable_buffer &buf,
                       const string_view &id)
{
	db::column column
	{
		*event::events, "state_node"
	};

	return get_node(column, buf, id);
}

ircd::json::object
ircd::m::dbs::get_node(db::column &column,
                       const mutable_buffer &buf,
                       const string_view &id)
{
	return db::read(column, id, buf);
}

ircd::string_view
ircd::m::dbs::make_node(db::iov &iov,
                        const mutable_buffer &hashbuf,
                        const std::initializer_list<json::array> &keys,
                        const std::initializer_list<string_view> &vals)
{
	thread_local char buf[2_KiB];
	const ctx::critical_assertion ca;

	const string_view node
	{
		make_node(buf, keys, vals)
	};

	const sha256::buf hash
	{
		sha256{const_buffer{node}}
	};

	const auto hashb64
	{
		b64encode_unpadded(hashbuf, hash)
	};

	db::iov::append
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

ircd::json::object
ircd::m::dbs::make_node(const mutable_buffer &out,
                        const std::initializer_list<json::array> &_keys,
                        const std::initializer_list<string_view> &_vals)
{
	assert(_keys.size() <= 2);
	assert(_vals.size() <= 3);
	json::value keys[2], vals[3];
	{
		size_t i(0);
		for(const auto &key : _keys)
			keys[i++] = key;

		i = 0;
		for(const auto &val : _vals)
			vals[i++] = val;
	};

	json::iov iov;
	const json::iov::push push[]
	{
		{ iov, { "k"_sv, { keys, _keys.size() } } },
		{ iov, { "v"_sv, { vals, _vals.size() } } },
	};

	return { data(out), json::print(out, iov) };
}

size_t
ircd::m::dbs::find(const node &node,
                   const string_view &type,
                   const string_view &state_key)
{
	thread_local char buf[1_KiB];
	const ctx::critical_assertion ca;
	return find(node, make_key(buf, type, state_key));
}

size_t
ircd::m::dbs::find(const node &node,
                   const json::array &parts)
{
	const auto &keys
	{
		json::get<"k"_>(node)
	};

	size_t ret{0};
	for(const json::array key : keys)
	{
		auto kit(begin(key));
		auto pit(begin(parts));
		for(; pit != end(parts) && kit != end(key); ++pit, ++kit)
		{
			const auto &keyp{*kit};
			const auto &part{*pit};
			assert(surrounds(part, '"'));
			assert(surrounds(keyp, '"'));

			if(part < keyp)
				return ret;

			if(part > keyp)
				break;
		}

		if(kit == end(key))
			return ret;

		++ret;
	}

	return ret;
}

ircd::json::array
ircd::m::dbs::make_key(const mutable_buffer &out,
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
ircd::m::dbs::val(const node &node,
                  const size_t &pos)
{
	return json::at<"v"_>(node).at(pos);
}

ircd::json::array
ircd::m::dbs::key(const node &node,
                  const size_t &pos)
{
	return json::at<"k"_>(node).at(pos);
}

size_t
ircd::m::dbs::keys(const node &node)
{
    return json::get<"k"_>(node).count();
}

size_t
ircd::m::dbs::vals(const node &node)
{
    return json::get<"v"_>(node).count();
}

//
//
//

decltype(ircd::m::dbs::noop)
ircd::m::dbs::noop
{};

ircd::m::dbs::query<>::~query()
noexcept
{
}

bool
ircd::m::dbs::_query(const query<> &clause,
                    const closure_bool &closure)
{
	switch(clause.type)
	{
		case where::equal:
		{
			const auto &c
			{
				dynamic_cast<const query<where::equal> &>(clause)
			};

			switch(_query_where(c, closure))
			{
				case 0:   return false;
				case 1:   return true;
				default:  break;
			}

			break;
		}

		case where::logical_and:
		{
			const auto &c
			{
				dynamic_cast<const query<where::logical_and> &>(clause)
			};

			switch(_query_where(c, closure))
			{
				case 0:   return false;
				case 1:   return true;
				default:  break;
			}

			break;
		}

		default:
		{
			return _query_event_id(clause, closure);
		}
	}

	return _query_event_id(clause, closure);
}

int
ircd::m::dbs::_query_where(const query<where::equal> &where,
                          const closure_bool &closure)
{
	log.debug("Query [%s]: %s",
	          "where equal",
	          pretty_oneline(where.value));

	const auto &value{where.value};
	const auto &room_id{json::get<"room_id"_>(value)};
	if(room_id)
		return _query_where_room_id(where, closure);

	const auto &event_id{json::get<"event_id"_>(value)};
	if(event_id)
		return _query_where_event_id(where, closure);

	return -1;
}

int
ircd::m::dbs::_query_where(const query<where::logical_and> &where,
                          const closure_bool &closure)
{
	const auto &lhs{*where.a}, &rhs{*where.b};
	const auto reclosure{[&lhs, &rhs, &closure]
	(const auto &event)
	{
		if(!rhs(event))
			return false;

		return closure(event);
	}};

	return _query(lhs, reclosure);
}

int
ircd::m::dbs::_query_where_event_id(const query<where::equal> &where,
                                   const closure_bool &closure)
{
	const event::id &event_id
	{
		at<"event_id"_>(where.value)
	};

	if(my(event_id))
	{
		std::cout << "GET LOCAL? " << event_id << std::endl;
		if(_query_event_id(where, closure))
			return true;
	}

	std::cout << "GET REMOTE? " << event_id << std::endl;
	const unique_buffer<mutable_buffer> buf
	{
		64_KiB
	};

	event::fetch tab
	{
		event_id, buf
	};

	const m::event event
	{
		io::acquire(tab)
	};

	return closure(event);
}

int
ircd::m::dbs::_query_where_room_id_at_event_id(const query<where::equal> &where,
                                              const closure_bool &closure)
{
	const auto &value{where.value};
	const room::id &room_id{json::get<"room_id"_>(value)};
	const auto &event_id{json::get<"event_id"_>(value)};
	const auto &state_key{json::get<"state_key"_>(value)};

	if(!defined(state_key))
		return _query_where_event_id(where, closure);

	if(my(room_id))
	{
		std::cout << "GET LOCAL STATE? " << event_id << std::endl;
		return -1;
	}

	const auto &type{json::get<"type"_>(value)};
	if(type && state_key)
		return _query_for_type_state_key_in_room_id(where, closure, room_id, type, state_key);

	return _query_in_room_id(where, closure, room_id);
}

int
ircd::m::dbs::_query_where_room_id(const query<where::equal> &where,
                                  const closure_bool &closure)
{
	const auto &value{where.value};
	const auto &room_id{json::get<"room_id"_>(value)};
	const auto &event_id{json::get<"event_id"_>(value)};
	if(event_id)
		return _query_where_room_id_at_event_id(where, closure);

	const auto &type{json::get<"type"_>(value)};
	const auto &state_key{json::get<"state_key"_>(value)};
	const bool is_state{defined(state_key) == true};
	if(type && is_state)
		return _query_for_type_state_key_in_room_id(where, closure, room_id, type, state_key);

	if(is_state)
		return _query_for_type_state_key_in_room_id(where, closure, room_id, type, state_key);

	//std::cout << "in room id?" << std::endl;
	return _query_in_room_id(where, closure, room_id);
}

bool
ircd::m::dbs::_query_event_id(const query<> &query,
                             const closure_bool &closure)
{
	cursor cursor
	{
		"event_id", &query
	};

	for(auto it(cursor.begin()); bool(it); ++it)
		if(closure(*it))
			return true;

	return false;
}

bool
ircd::m::dbs::_query_in_room_id(const query<> &query,
                                   const closure_bool &closure,
                                   const room::id &room_id)
{
	cursor cursor
	{
		"event_id in room_id", &query
	};

	for(auto it(cursor.begin(room_id)); bool(it); ++it)
		if(closure(*it))
			return true;

	return false;
}

bool
ircd::m::dbs::_query_for_type_state_key_in_room_id(const query<> &query,
                                                  const closure_bool &closure,
                                                  const room::id &room_id,
                                                  const string_view &type,
                                                  const string_view &state_key)
{
	cursor cursor
	{
		"event_id for type,state_key in room_id", &query
	};

	static const size_t max_type_size
	{
		255
	};

	static const size_t max_state_key_size
	{
		255
	};

	const auto key_max
	{
		room::id::buf::SIZE + max_type_size + max_state_key_size + 2
	};

	size_t key_len;
	char key[key_max]; key[0] = '\0';
	key_len = strlcat(key, room_id, sizeof(key));
	key_len = strlcat(key, "..", sizeof(key)); //TODO: prefix protocol
	key_len = strlcat(key, type, sizeof(key)); //TODO: prefix protocol
	key_len = strlcat(key, state_key, sizeof(key)); //TODO: prefix protocol
	for(auto it(cursor.begin(key)); bool(it); ++it)
		if(closure(*it))
			return true;

	return false;
}

//
//
//

bool
ircd::m::dbs::exists(const event::id &event_id)
{
	db::column column
	{
		*event::events, "event_id"
	};

	return has(column, event_id);
}

//
//
//

namespace ircd::m
{
	struct indexer;

	extern std::set<std::shared_ptr<indexer>> indexers;
	extern const std::unique_ptr<indexer> indexer_origin_joined;
}

struct ircd::m::indexer
{
	//TODO: collapse
	struct concat;
	struct concat_s; //TODO: special
	struct concat_v;
	struct concat_2v;
	struct concat_3vs; //TODO: special

	std::string name;

	virtual void operator()(const event &, db::iov &iov, const db::op &op) const {}

	indexer(std::string name)
	:name{std::move(name)}
	{}

	virtual ~indexer() noexcept;
};

ircd::m::indexer::~indexer()
noexcept
{
}

void
ircd::m::dbs::append_indexes(const event &event,
                             db::iov &iov)
{
	for(const auto &ptr : indexers)
	{
		const m::indexer &indexer{*ptr};
		indexer(event, iov, db::op::SET);
	}

	if(json::get<"type"_>(event) == "m.room.member")
	{
		const m::indexer &indexer{*indexer_origin_joined};
		if(json::get<"membership"_>(event) == "join")
			indexer(event, iov, db::op::SET);
		//else
		//	indexer(event, iov, db::op::DELETE);
	}
}

struct ircd::m::indexer::concat
:indexer
{
	std::string col_a;
	std::string col_b;

	void operator()(const event &, db::iov &, const db::op &op) const final override;

	concat(std::string col_a, std::string col_b)
	:indexer
	{
		fmt::snstringf(512, "%s in %s", col_a, col_b)
	}
	,col_a{col_a}
	,col_b{col_b}
	{}
};

void
ircd::m::indexer::concat::operator()(const event &event,
                                     db::iov &iov,
                                     const db::op &op)
const
{
	if(!iov.has(op, col_a) || !iov.has(op, col_b))
		return;

	static const size_t buf_max
	{
		1024
	};

	char index[buf_max];
	index[0] = '\0';
	const auto function
	{
		[&index](auto &val)
		{
			strlcat(index, byte_view<string_view>{val}, buf_max);
		}
	};

	at(event, col_b, function);
	at(event, col_a, function);

	db::iov::append
	{
		iov, db::delta
		{
			name,        // col
			index,       // key
			{},          // val
		}
	};
}

struct ircd::m::indexer::concat_s
:indexer
{
	std::string col_a;
	std::string col_b;

	void operator()(const event &, db::iov &, const db::op &) const final override;

	concat_s(std::string col_a, std::string col_b, std::string name = {})
	:indexer
	{
		!name? std::string{fmt::snstringf(512, "%s in %s", col_a, col_b)} : name
	}
	,col_a{col_a}
	,col_b{col_b}
	{}
};

void
ircd::m::indexer::concat_s::operator()(const event &event,
                                       db::iov &iov,
                                       const db::op &op)
const
{
	if(!iov.has(op, col_a) || !iov.has(op, col_b))
		return;

	static const size_t buf_max
	{
		1024
	};

	char index[buf_max];
	index[0] = '\0';
	const auto function
	{
		[&index](auto &val)
		{
			strlcat(index, byte_view<string_view>{val}, buf_max);
		}
	};

	at(event, col_b, function);
	strlcat(index, ":::", buf_max);  //TODO: special
	at(event, col_a, function);

	db::iov::append
	{
		iov, db::delta
		{
			op,
			name,        // col
			index,       // key
			{},          // val
		}
	};
}

struct ircd::m::indexer::concat_v
:indexer
{
	std::string col_a;
	std::string col_b;
	std::string col_c;

	void operator()(const event &, db::iov &, const db::op &op) const final override;
	void operator()(const event &, db::iov &, const db::op &op, const string_view &) const;

	concat_v(std::string col_a, std::string col_b, std::string col_c)
	:indexer
	{
		fmt::snstringf(512, "%s for %s in %s", col_a, col_b, col_c)
	}
	,col_a{col_a}
	,col_b{col_b}
	,col_c{col_c}
	{}
};

void
ircd::m::indexer::concat_v::operator()(const event &event,
                                       db::iov &iov,
                                       const db::op &op)
const
{
	if(!iov.has(op, col_c) || !iov.has(op, col_b) || !iov.has(op, col_a))
		return;

	static const size_t buf_max
	{
		1024
	};

	char index[buf_max];
	index[0] = '\0';
	const auto concat
	{
		[&index](auto &val)
		{
			strlcat(index, byte_view<string_view>{val}, buf_max);
		}
	};

	at(event, col_c, concat);
	at(event, col_b, concat);

	string_view val;
	at(event, col_a, [&val](auto &_val)
	{
		val = byte_view<string_view>{_val};
	});

	db::iov::append
	{
		iov, db::delta
		{
			name,        // col
			index,       // key
			val,         // val
		}
	};
}

void
ircd::m::indexer::concat_v::operator()(const event &event,
                                       db::iov &iov,
                                       const db::op &op,
                                       const string_view &val)
const
{
	static const size_t buf_max
	{
		1024
	};

	char index[buf_max];
	index[0] = '\0';
	const auto concat
	{
		[&index](auto &val)
		{
			strlcat(index, byte_view<string_view>{val}, buf_max);
		}
	};

	at(event, col_c, concat);
	at(event, col_b, concat);

	db::iov::append
	{
		iov, db::delta
		{
			name,        // col
			index,       // key
			val,         // val
		}
	};
}

struct ircd::m::indexer::concat_2v
:indexer
{
	std::string col_a;
	std::string col_b0;
	std::string col_b1;
	std::string col_c;

	void operator()(const event &, db::iov &, const db::op &op) const final override;

	concat_2v(std::string col_a, std::string col_b0, std::string col_b1, std::string col_c)
	:indexer
	{
		fmt::snstringf(512, "%s for %s,%s in %s", col_a, col_b0, col_b1, col_c)
	}
	,col_a{col_a}
	,col_b0{col_b0}
	,col_b1{col_b1}
	,col_c{col_c}
	{}
};

void
ircd::m::indexer::concat_2v::operator()(const event &event,
                                        db::iov &iov,
                                        const db::op &op)
const
{
	if(!iov.has(op, col_c) || !iov.has(op, col_b0) || !iov.has(op, col_b1))
		return;

	static const size_t buf_max
	{
		2048
	};

	char index[buf_max];
	index[0] = '\0';
	const auto concat
	{
		[&index](auto &val)
		{
			strlcat(index, byte_view<string_view>{val}, buf_max);
		}
	};

	at(event, col_c, concat);
	strlcat(index, "..", buf_max);  //TODO: special
	at(event, col_b0, concat);
	at(event, col_b1, concat);

	string_view val;
	at(event, col_a, [&val](auto &_val)
	{
		val = byte_view<string_view>{_val};
	});

	db::iov::append
	{
		iov, db::delta
		{
			name,        // col
			index,       // key
			val,         // val
		}
	};
}

struct ircd::m::indexer::concat_3vs
:indexer
{
	std::string col_a;
	std::string col_b0;
	std::string col_b1;
	std::string col_b2;
	std::string col_c;

	void operator()(const event &, db::iov &, const db::op &op, const string_view &prev_event_id) const;

	concat_3vs(std::string col_a, std::string col_b0, std::string col_b1, std::string col_b2, std::string col_c)
	:indexer
	{
		fmt::snstringf(512, "%s for %s,%s,%s in %s", col_a, col_b0, col_b1, col_b2, col_c)
	}
	,col_a{col_a}
	,col_b0{col_b0}
	,col_b1{col_b1}
	,col_b2{col_b2}
	,col_c{col_c}
	{}
}
const concat_3vs
{
	"prev_event_id", "type", "state_key", "event_id", "room_id"
};

// Non-participating
void
ircd::m::_index_special0(const event &event,
                         db::iov &iov,
                         const db::op &op,
                         const string_view &prev_event_id)
{
	concat_3vs(event, iov, op, prev_event_id);
}

// Non-participating
void
ircd::m::_index_special1(const event &event,
                         db::iov &iov,
                         const db::op &op,
                         const string_view &prev_event_id)
{
	static const ircd::m::indexer::concat_v idxr
	{
		"prev_event_id", "event_id", "room_id"
	};

	idxr(event, iov, op, prev_event_id);
}

// Non-participating
void
ircd::m::indexer::concat_3vs::operator()(const event &event,
                                         db::iov &iov,
                                         const db::op &op,
                                         const string_view &prev_event_id)
const
{
	if(!iov.has(op, col_c) ||
	   !iov.has(op, col_b0) ||
	   !iov.has(op, col_b1) ||
	   !iov.has(op, col_b2))
		return;

	static const size_t buf_max
	{
		2048
	};

	char index[buf_max];
	index[0] = '\0';
	const auto concat
	{
		[&index](auto &val)
		{
			strlcat(index, byte_view<string_view>{val}, buf_max);
		}
	};

	at(event, col_c, concat);
	strlcat(index, "..", buf_max);  //TODO: special
	at(event, col_b0, concat);
	at(event, col_b1, concat);
	at(event, col_b2, concat);

	db::iov::append
	{
		iov, db::delta
		{
			name,           // col
			index,          // key
			prev_event_id,  // val
		}
	};
}

decltype(ircd::m::indexers)
ircd::m::indexers
{{
	std::make_shared<ircd::m::indexer::concat>("event_id", "sender"),
	std::make_shared<ircd::m::indexer::concat>("event_id", "room_id"),
	std::make_shared<ircd::m::indexer::concat_s>("origin", "room_id"),
	std::make_shared<ircd::m::indexer::concat_2v>("event_id", "type", "state_key", "room_id"),
	std::make_shared<ircd::m::indexer::concat_3vs>("prev_event_id", "type", "state_key", "event_id", "room_id"),
}};

decltype(ircd::m::indexer_origin_joined)
ircd::m::indexer_origin_joined
{
	std::make_unique<ircd::m::indexer::concat_s>("origin", "room_id", "origin_joined in room_id")
};

//
//
//

ircd::string_view
ircd::m::dbs::reflect(const where &w)
{
	switch(w)
	{
		case where::noop:           return "noop";
		case where::test:           return "test";
		case where::equal:          return "equal";
		case where::not_equal:      return "not_equal";
		case where::logical_or:     return "logical_or";
		case where::logical_and:    return "logical_and";
		case where::logical_not:    return "logical_not";
	}

	return "?????";
}
