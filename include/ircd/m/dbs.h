// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_DBS_H

namespace ircd::m::dbs
{
	struct init;
	struct node;

	extern std::map<std::string, ircd::module> modules;
	extern std::map<std::string, import_shared<database>> databases;

	bool exists(const event::id &);

	size_t keys(const node &);
	size_t vals(const node &);
	json::array key(const node &, const size_t &);
	string_view val(const node &, const size_t &);
	json::array make_key(const mutable_buffer &out, const string_view &type, const string_view &state_key);
	size_t find(const node &, const json::array &key);
	size_t find(const node &, const string_view &type, const string_view &state_key);
	json::object make_node(const mutable_buffer &out, const std::initializer_list<json::array> &keys, const std::initializer_list<string_view> &vals);
	string_view make_node(db::iov &txn, const mutable_buffer &hash, const std::initializer_list<json::array> &keys, const std::initializer_list<string_view> &vals);
	json::object get_node(db::column &, const mutable_buffer &buf, const string_view &id);
	json::object get_node(const mutable_buffer &buf, const string_view &id);

	void append_indexes(const event &, db::iov &iov);
	void append_nodes(const event &, db::iov &iov);
	void write(const event &, db::iov &txn);
}

namespace ircd::m::dbs::name
{
	constexpr const char *const k {"k"};
	constexpr const char *const v {"v"};
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsubobject-linkage"
struct ircd::m::dbs::node
:json::tuple
<
	json::property<name::k, json::array>,
	json::property<name::v, json::array>
>
{
	using super_type::tuple;
	using super_type::operator=;
};
#pragma GCC diagnostic pop

class ircd::m::dbs::init
{
	void _modules();
	void _databases();

  public:
	init();
	~init() noexcept;
};

namespace ircd::m::dbs
{
	using closure = std::function<void (const event &)>;
	using closure_bool = std::function<bool (const event &)>;

	bool _query(const query<> &, const closure_bool &);
	bool _query_event_id(const query<> &, const closure_bool &);
	bool _query_in_room_id(const query<> &, const closure_bool &, const id::room &);
	bool _query_for_type_state_key_in_room_id(const query<> &, const closure_bool &, const id::room &, const string_view &type, const string_view &state_key);

	int _query_where_event_id(const query<where::equal> &, const closure_bool &);
	int _query_where_room_id_at_event_id(const query<where::equal> &, const closure_bool &);
	int _query_where_room_id(const query<where::equal> &, const closure_bool &);
	int _query_where(const query<where::equal> &where, const closure_bool &closure);
	int _query_where(const query<where::logical_and> &where, const closure_bool &closure);
}

namespace ircd::m
{
	void _index_special0(const event &event, db::iov &iov, const db::op &, const string_view &prev_event_id);
	void _index_special1(const event &event, db::iov &iov, const db::op &, const string_view &prev_event_id);
}

