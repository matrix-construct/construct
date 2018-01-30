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

	virtual void operator()(const event &, db::txn &txn, const db::op &op) const {}

	indexer(std::string name)
	:name{std::move(name)}
	{}

	virtual ~indexer() noexcept = default;
};

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
                    db::txn &txn)
{
	db::txn::append
	{
		txn, at<"event_id"_>(event), event
	};

	if(defined(json::get<"state_key"_>(event)))
		append_nodes(event, txn);

	append_indexes(event, txn);
}

void
ircd::m::dbs::append_nodes(const event &event,
                           db::txn &txn)
{
	const auto &type{at<"type"_>(event)};
	const auto &state_key{at<"state_key"_>(event)};
	const auto &event_id{at<"event_id"_>(event)};
	const auto &room_id{at<"room_id"_>(event)};

	if(type == "m.room.create")
	{
		thread_local char key[512], head[64];
		const json::array keys[]
		{
			{ state::make_key(key, type, state_key) }
		};

		const string_view vals[]
		{
			{ event_id }
		};

		state::set_head(txn, room_id, state::set_node(txn, head, keys, 1, vals, 1));
		return;
	}

	state::insert(txn, room_id, type, state_key, event_id);
}

void
ircd::m::dbs::append_indexes(const event &event,
                             db::txn &txn)
{
	for(const auto &ptr : indexers)
	{
		const m::indexer &indexer{*ptr};
		indexer(event, txn, db::op::SET);
	}

	if(json::get<"type"_>(event) == "m.room.member")
	{
		const m::indexer &indexer{*indexer_origin_joined};
		if(json::get<"membership"_>(event) == "join")
			indexer(event, txn, db::op::SET);
		//else
		//	indexer(event, txn, db::op::DELETE);
	}
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

struct ircd::m::indexer::concat
:indexer
{
	std::string col_a;
	std::string col_b;

	void operator()(const event &, db::txn &, const db::op &op) const final override;

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
                                     db::txn &txn,
                                     const db::op &op)
const
{
	if(!txn.has(op, col_a) || !txn.has(op, col_b))
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

	db::txn::append
	{
		txn, db::delta
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

	void operator()(const event &, db::txn &, const db::op &) const final override;

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
                                       db::txn &txn,
                                       const db::op &op)
const
{
	if(!txn.has(op, col_a) || !txn.has(op, col_b))
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

	db::txn::append
	{
		txn, db::delta
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

	void operator()(const event &, db::txn &, const db::op &op) const final override;
	void operator()(const event &, db::txn &, const db::op &op, const string_view &) const;

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
                                       db::txn &txn,
                                       const db::op &op)
const
{
	if(!txn.has(op, col_c) || !txn.has(op, col_b) || !txn.has(op, col_a))
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

	db::txn::append
	{
		txn, db::delta
		{
			name,        // col
			index,       // key
			val,         // val
		}
	};
}

void
ircd::m::indexer::concat_v::operator()(const event &event,
                                       db::txn &txn,
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

	db::txn::append
	{
		txn, db::delta
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

	void operator()(const event &, db::txn &, const db::op &op) const final override;

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
                                        db::txn &txn,
                                        const db::op &op)
const
{
	if(!txn.has(op, col_c) || !txn.has(op, col_b0) || !txn.has(op, col_b1))
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

	db::txn::append
	{
		txn, db::delta
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

	void operator()(const event &, db::txn &, const db::op &op, const string_view &prev_event_id) const;

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
                         db::txn &txn,
                         const db::op &op,
                         const string_view &prev_event_id)
{
	concat_3vs(event, txn, op, prev_event_id);
}

// Non-participating
void
ircd::m::_index_special1(const event &event,
                         db::txn &txn,
                         const db::op &op,
                         const string_view &prev_event_id)
{
	static const ircd::m::indexer::concat_v idxr
	{
		"prev_event_id", "event_id", "room_id"
	};

	idxr(event, txn, op, prev_event_id);
}

// Non-participating
void
ircd::m::indexer::concat_3vs::operator()(const event &event,
                                         db::txn &txn,
                                         const db::op &op,
                                         const string_view &prev_event_id)
const
{
	if(!txn.has(op, col_c) ||
	   !txn.has(op, col_b0) ||
	   !txn.has(op, col_b1) ||
	   !txn.has(op, col_b2))
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

	db::txn::append
	{
		txn, db::delta
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
