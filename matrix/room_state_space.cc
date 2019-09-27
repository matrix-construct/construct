// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::m::room::state::space::space(const m::room &room)
:room
{
	room
}
{
}

bool
ircd::m::room::state::space::prefetch(const string_view &type)
const
{
	return prefetch(type, string_view{});
}

bool
ircd::m::room::state::space::prefetch(const string_view &type,
                                      const string_view &state_key)
const
{
	return prefetch(type, state_key, -1);
}

bool
ircd::m::room::state::space::prefetch(const string_view &type,
                                      const string_view &state_key,
                                      const int64_t &depth)
const
{
	const int64_t &_depth
	{
		type? depth : 0L
	};

	char buf[dbs::ROOM_STATE_SPACE_KEY_MAX_SIZE];
	const string_view &key
	{
		dbs::room_state_space_key(buf, room.room_id, type, state_key, _depth, 0UL)
	};

	return db::prefetch(dbs::room_state_space, key);
}

bool
ircd::m::room::state::space::has(const string_view &type)
const
{
	return has(type, string_view{});
}

bool
ircd::m::room::state::space::has(const string_view &type,
                                 const string_view &state_key)
const
{
	return has(type, state_key, -1);
}

bool
ircd::m::room::state::space::has(const string_view &type,
                                 const string_view &state_key,
                                 const int64_t &depth)
const
{
	return !for_each(type, state_key, depth, []
	(const auto &type, const auto &state_key, const auto &depth, const auto &event_idx)
	{
		return false;
	});
}

size_t
ircd::m::room::state::space::count()
const
{
	return count(string_view{});
}

size_t
ircd::m::room::state::space::count(const string_view &type)
const
{
	return count(type, string_view{});
}

size_t
ircd::m::room::state::space::count(const string_view &type,
                                   const string_view &state_key)
const
{
	return count(type, state_key, -1L);
}

size_t
ircd::m::room::state::space::count(const string_view &type,
                                   const string_view &state_key,
                                   const int64_t &depth)
const
{
	size_t ret(0);
	for_each(type, state_key, depth, [&ret]
	(const auto &type, const auto &state_key, const auto &depth, const auto &event_idx)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
ircd::m::room::state::space::for_each(const closure &closure)
const
{
	return for_each(string_view{}, string_view{}, -1L, closure);
}

bool
ircd::m::room::state::space::for_each(const string_view &type,
                                      const closure &closure)
const
{
	return for_each(type, string_view{}, -1L, closure);
}

bool
ircd::m::room::state::space::for_each(const string_view &type,
                                      const string_view &state_key,
                                      const closure &closure)
const
{
	return for_each(type, state_key, -1L, closure);
}

bool
ircd::m::room::state::space::for_each(const string_view &type,
                                      const string_view &state_key,
                                      const int64_t &depth,
                                      const closure &closure)
const
{
	const int64_t &_depth
	{
		type? depth : 0L
	};

	char buf[dbs::ROOM_STATE_SPACE_KEY_MAX_SIZE];
	const string_view &key
	{
		dbs::room_state_space_key(buf, room.room_id, type, state_key, _depth, 0UL)
	};

	auto it
	{
		dbs::room_state_space.begin(key)
	};

	for(; it; ++it)
	{
		const auto &[_type, _state_key, _depth, _event_idx]
		{
			dbs::room_state_space_key(it->first)
		};

		if(type && type != _type)
			break;

		if(state_key && state_key != _state_key)
			break;

		if(depth >= 0 && depth != _depth)
			break;

		if(!closure(_type, _state_key, _depth, _event_idx))
			return false;
	}

	return true;
}

//
// room::state::space::rebuild
//

ircd::m::room::state::space::rebuild::rebuild(const room::id &room_id)
{
	db::txn txn
	{
		*m::dbs::events
	};

	m::room::events it
	{
		room_id, uint64_t(0)
	};

	if(!it)
		return;

	const bool check_auth
	{
		!m::internal(room_id)
	};

	size_t state_count(0), messages_count(0), state_deleted(0);
	for(; it; ++it, ++messages_count) try
	{
		const m::event::idx &event_idx
		{
			it.event_idx()
		};

		if(!state::is(std::nothrow, event_idx))
			continue;

		++state_count;
		const m::event &event{*it};
		const auto &[pass_static, reason_static]
		{
			check_auth?
				room::auth::check_static(event):
				room::auth::passfail{true, {}}
		};

		if(!pass_static)
			log::dwarning
			{
				log, "%s in %s erased from state space (static) :%s",
				string_view{event.event_id},
				string_view{room_id},
				what(reason_static),
			};

		const auto &[pass_relative, reason_relative]
		{
			!check_auth?
				room::auth::passfail{true, {}}:
			pass_static?
				room::auth::check_relative(event):
				room::auth::passfail{false, {}},
		};

		if(pass_static && !pass_relative)
			log::dwarning
			{
				log, "%s in %s erased from state space (relative) :%s",
				string_view{event.event_id},
				string_view{room_id},
				what(reason_relative),
			};

		dbs::write_opts opts;
		opts.event_idx = event_idx;

		opts.appendix.reset();
		opts.appendix.set(dbs::appendix::ROOM_STATE_SPACE);

		opts.op = pass_static && pass_relative? db::op::SET : db::op::DELETE;
		state_deleted += opts.op == db::op::DELETE;

		dbs::write(txn, event, opts);
	}
	catch(const ctx::interrupted &e)
	{
		log::dwarning
		{
			log, "room::state::space::rebuild :%s",
			e.what()
		};

		throw;
	}
	catch(const std::exception &e)
	{
		log::error
		{
			log, "room::state::space::rebuild :%s",
			e.what()
		};
	}

	log::info
	{
		log, "room::state::space::rebuild %s complete msgs:%zu state:%zu del:%zu transaction elems:%zu size:%s",
		string_view{room_id},
		messages_count,
		state_count,
		state_deleted,
		txn.size(),
		pretty(iec(txn.bytes()))
	};

	txn();
}
