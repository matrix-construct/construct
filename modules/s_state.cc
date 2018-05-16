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
		opts.idx = it.event_idx();
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
