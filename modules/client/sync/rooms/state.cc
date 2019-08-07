// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "../args.h"

namespace ircd::m::sync
{
	static bool room_state_append(data &, json::stack::array &, const m::event &, const m::event::idx &);

	static bool room_state_phased_member_events(data &, json::stack::array &);
	static bool room_state_phased_events(data &);
	static bool room_state_polylog_events(data &);
	static bool _room_state_polylog(data &);
	static bool room_state_polylog(data &);
	static bool room_invite_state_polylog(data &);

	static bool room_state_linear_events(data &);
	static bool room_invite_state_linear(data &);
	static bool room_state_linear(data &);

	extern conf::item<bool> crazyload_historical_members;
	extern conf::item<int64_t> state_exposure_depth; //TODO: XXX

	extern item room_invite_state;
	extern item room_state;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client Sync :Room State"
};

decltype(ircd::m::sync::room_state)
ircd::m::sync::room_state
{
	"rooms.state",
	room_state_polylog,
	room_state_linear,
	{
		{ "phased", true },
	}
};

decltype(ircd::m::sync::room_invite_state)
ircd::m::sync::room_invite_state
{
	"rooms.invite_state",
	room_invite_state_polylog,
	room_invite_state_linear,
	{
		{ "phased", true },
	}
};

bool
ircd::m::sync::room_state_linear(data &data)
{
	if(data.membership == "invite")
		return false;

	return room_state_linear_events(data);
}

bool
ircd::m::sync::room_invite_state_linear(data &data)
{
	if(data.membership != "invite")
		return false;

	return room_state_linear_events(data);
}

//TODO: This has to be merged into the timeline conf items
decltype(ircd::m::sync::state_exposure_depth)
ircd::m::sync::state_exposure_depth
{
	{ "name",         "ircd.client.sync.rooms.state.exposure.depth" },
	{ "default",      20L                                           },
};

bool
ircd::m::sync::room_state_linear_events(data &data)
{
	if(!data.event_idx)
		return false;

	if(!data.room)
		return false;

	if(!data.membership)
		return false;

	assert(data.event);
	if(!json::get<"state_key"_>(*data.event))
		return false;

	const bool is_own_membership
	{
		json::get<"type"_>(*data.event) == "m.room.member"
		&& json::get<"state_key"_>(*data.event) == data.user.user_id
	};

	const bool is_own_join
	{
		is_own_membership && data.membership == "join"
	};
	assert(!is_own_join || m::membership(*data.event) == "join");

	// Figure out whether the event was included in the timeline or whether
	// to include it here in the state, which comes before the timeline.
	// Since linear-sync is already distinct from polylog-sync, the
	// overwhelming majority of state events coming through linear-sync will
	// use the timeline. We make an exception for past state events the server
	// only recently obtained, to hide them from the timeline.
	if(int64_t(state_exposure_depth) > -1)
		if(data.membership != "invite" && !is_own_join)
			if(json::get<"depth"_>(*data.event) + int64_t(state_exposure_depth) >= data.room_depth)
				return false;

	json::stack::object rooms
	{
		*data.out, "rooms"
	};

	json::stack::object membership_
	{
		*data.out, data.membership
	};

	json::stack::object room_
	{
		*data.out, data.room->room_id
	};

	const auto &state_member_name
	{
		data.membership == "invite"?
			"invite_state": // "invite_state"_sv:
			"state"
	};

	json::stack::object state
	{
		*data.out, state_member_name
	};

	json::stack::array array
	{
		*data.out, "events"
	};

	bool ret{false};
	const auto append
	{
		[&data, &array, &ret](const event::idx &event_idx)
		{
			const event::fetch event
			{
				event_idx, std::nothrow
			};

			if(event.valid)
				ret |= room_state_append(data, array, event, event_idx);

			return true;
		}
	};

	if(is_own_membership && (data.membership == "invite" || data.membership == "join"))
	{
		const m::room::state state{*data.room};
		state.get(std::nothrow, "m.room.create", "", append);
		state.get(std::nothrow, "m.room.join_rules", "", append);
		state.get(std::nothrow, "m.room.power_levels", "", append);
		state.get(std::nothrow, "m.room.history_visibility", "", append);
		state.get(std::nothrow, "m.room.avatar", "", append);
		state.get(std::nothrow, "m.room.name", "", append);
		state.get(std::nothrow, "m.room.canonical_alias", "", append);
		state.get(std::nothrow, "m.room.aliases", my_host(), append);
	}

	// Branch for supplying state to the client after its user's invite
	// is processed. At this point the client has not received prior room
	// state in /sync.
	if(is_own_membership && data.membership == "invite")
	{
		const m::room::state state{*data.room};
		const auto &sender
		{
			json::get<"sender"_>(*data.event)
		};

		state.get(std::nothrow, "m.room.member", sender, append);
	}

	ret |= room_state_append(data, array, *data.event, data.event_idx);
	return ret;
}

bool
ircd::m::sync::room_state_polylog(data &data)
{
	if(data.membership == "invite")
		return false;

	return _room_state_polylog(data);
}

bool
ircd::m::sync::room_invite_state_polylog(data &data)
{
	if(data.membership != "invite")
		return false;

	return _room_state_polylog(data);
}

bool
ircd::m::sync::_room_state_polylog(data &data)
{
	assert(data.args);
	if(likely(!data.args->full_state))
		if(!apropos(data, data.room_head))
			if(!data.phased || int64_t(data.range.first) > 0)
				return false;

	return room_state_polylog_events(data);
}

decltype(ircd::m::sync::crazyload_historical_members)
ircd::m::sync::crazyload_historical_members
{
	{ "name",         "ircd.client.sync.rooms.state.historical.members" },
	{ "default",      false                                             },
};

bool
ircd::m::sync::room_state_polylog_events(data &data)
{
	if(data.phased && data.range.first == 0)
		return room_state_phased_events(data);

	bool ret{false};
	ctx::mutex mutex;
	json::stack::array array
	{
		*data.out, "events"
	};

	sync::pool.min(64); //TODO: XXX
	ctx::concurrent<event::idx> concurrent
	{
		sync::pool, [&](const event::idx &event_idx)
		{
			const m::event::fetch event
			{
				event_idx, std::nothrow
			};

			if(unlikely(!event.valid))
			{
				log::error
				{
					log, "Failed to fetch event idx:%lu in room %s state.",
					event_idx,
					string_view{data.room->room_id},
				};

				return;
			}

			const std::lock_guard lock{mutex};
			ret |= room_state_append(data, array, event, event_idx);
		}
	};

	const room::state state{*data.room};
	state.for_each([&data, &concurrent]
	(const string_view &type, const string_view &state_key, const event::idx &event_idx)
	{
		// Skip this event if it's not in the sync range, except
		// when the request came with a `?full_state=true`
		assert(data.args);
		if(likely(!data.args->full_state))
			if(!apropos(data, event_idx))
				return true;

		// For crazyloading, skip membership events in rooms the user is not
		// presently joined.
		if(!crazyload_historical_members && !data.args->full_state && data.phased)
			if(data.membership != "join" && type == "m.room.member")
				return true;

		concurrent(event_idx);
		return true;
	});

	concurrent.wait();
	return ret;
}

bool
ircd::m::sync::room_state_phased_events(data &data)
{
	bool ret{false};
	ctx::mutex mutex;
	json::stack::array array
	{
		*data.out, "events"
	};

	const std::pair<string_view, string_view> keys[]
	{
		{ "m.room.create",           ""                        },
		{ "m.room.canonical_alias",  ""                        },
		{ "m.room.name",             ""                        },
		{ "m.room.avatar",           ""                        },
		{ "m.room.aliases",          data.user.user_id.host()  },
		{ "m.room.member",           data.user.user_id         },
	};

	const auto append
	{
		[&data, &array, &ret, &mutex]
		(const m::event::idx &event_idx, const m::event &event)
		{
			const std::lock_guard lock{mutex};
			ret |= room_state_append(data, array, event, event_idx);
		}
	};

	sync::pool.min(6);
	ctx::concurrent_for_each<const std::pair<string_view, string_view>>
	{
		sync::pool, keys, [&data, &append](const auto &key)
		{
			const auto &event_idx
			{
				data.room->get(std::nothrow, key.first, key.second)
			};

			const m::event::fetch event
			{
				event_idx, std::nothrow
			};

			if(unlikely(!event.valid))
				return;

			append(event_idx, event);
		}
	};

	if(data.membership == "join")
		ret |= room_state_phased_member_events(data, array);

	return ret;
}

bool
ircd::m::sync::room_state_phased_member_events(data &data,
                                               json::stack::array &array)
{
	static const auto count{20}, bufsz{32}, limit{20};

	size_t i(0), ret(0);
	std::array<char[bufsz], count> buf;
	std::array<string_view, count> last;
	const auto already
	{
		[&last, &ret](const string_view &sender) -> bool
		{
			return std::any_of(begin(last), begin(last)+ret, [&sender]
			(const auto &last)
			{
				return startswith(sender, last);
			});
		}
	};

	m::room::messages it
	{
		*data.room
	};

	for(; it && ret < count && i < limit; --it, ++i)
		m::get(std::nothrow, it.event_idx(), "sender", [&]
		(const string_view &sender)
		{
			if(already(sender))
				return;

			const auto sender_idx
			{
				data.room->get(std::nothrow, "m.room.member", sender)
			};

			if(!sender_idx)
				return;

			// check if this is an m.room.member event in the timeline.
			if(sender_idx == it.event_idx())
				return;

			const m::event::fetch event
			{
				sender_idx, std::nothrow
			};

			if(!event.valid)
				return;

			last.at(ret) = strlcpy(buf.at(ret), sender);
			room_state_append(data, array, event, sender_idx);
			++ret;
		});

	return ret;
}

bool
ircd::m::sync::room_state_append(data &data,
                                 json::stack::array &events,
                                 const m::event &event,
                                 const m::event::idx &event_idx)
{
	m::event::append::opts opts;
	opts.event_idx = &event_idx;
	opts.user_id = &data.user.user_id;
	opts.user_room = &data.user_room;
	opts.query_txnid = false;
	opts.room_depth = &data.room_depth;
	return m::event::append(events, event, opts);
}
