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
	bool exists(const event::id &);

	void append_indexes(const event &, db::txn &);
	void write(const event &, db::txn &);
}

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
	void _index_special0(const event &event, db::txn &txn, const db::op &, const string_view &prev_event_id);
	void _index_special1(const event &event, db::txn &txn, const db::op &, const string_view &prev_event_id);
}

