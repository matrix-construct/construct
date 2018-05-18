// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

mapi::header
IRCD_MODULE
{
	"Matrix state library; modular components."
};

extern "C" size_t
state__rebuild_present(const m::room &room)
{
	size_t ret{0};
	const m::room::state state
	{
		room
	};

	const auto create_id
	{
		state.get("m.room.create")
	};

	m::room::messages it
	{
		room, create_id
	};

	if(!it)
		return ret;

	db::txn txn
	{
		*m::dbs::events
	};

	for(; it; ++it)
	{
		const m::event &event{*it};
		if(!defined(json::get<"state_key"_>(event)))
			continue;

		m::dbs::write_opts opts;
		opts.event_idx = it.event_idx();
		opts.present = true;
		opts.history = false;
		opts.head = false;
		opts.refs = false;

		m::dbs::_index__room_state(txn, event, opts);
		m::dbs::_index__room_joined(txn, event, opts);
		++ret;
	}

	txn();
	return ret;
}

extern "C" size_t
state__rebuild_history(const m::room &room)
{
	size_t ret{0};
	const m::room::state state
	{
		room
	};

	const auto create_id
	{
		state.get("m.room.create")
	};

	m::room::messages it
	{
		room, create_id
	};

	if(!it)
		return ret;

	db::txn txn
	{
		*m::dbs::events
	};

	uint r(0);
	char root[2][64] {0};
	m::dbs::write_opts opts;
	opts.root_in = root[++r % 2];
	opts.root_out = root[++r % 2];
	opts.present = false;
	opts.history = true;
	opts.head = false;
	opts.refs = false;

	int64_t depth{0};
	for(; it; ++it)
	{
		const m::event &event{*it};
		opts.event_idx = it.event_idx();
		if(at<"depth"_>(event) == depth + 1)
			++depth;

		if(at<"depth"_>(event) != depth)
			throw ircd::error
			{
				"Incomplete room history: gap between %ld and %ld [%s]",
				depth,
				at<"depth"_>(event),
				string_view{at<"event_id"_>(event)}
			};

		if(at<"type"_>(event) == "m.room.redaction")
		{
			opts.root_in = m::dbs::_index_redact(txn, event, opts);
			opts.root_out = root[++r % 2];
			txn();
			txn.clear();
		}
		else if(defined(json::get<"state_key"_>(event)))
		{
			opts.root_in = m::dbs::_index_state(txn, event, opts);
			opts.root_out = root[++r % 2];
			txn();
			txn.clear();
		}
		else m::dbs::_index_ephem(txn, event, opts);

		++ret;
	}

	txn();
	return ret;
}
