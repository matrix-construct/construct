// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	static void auth_room_power_levels(const event &, room::auth::hookdata &);
	extern hookfn<room::auth::hookdata &> auth_room_power_levels_hookfn;

	static void changed_room_power_levels(const event &, vm::eval &);
	extern m::hookfn<m::vm::eval &> changed_room_power_levels_hookfn;
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix m.room.power_levels"
};

decltype(ircd::m::changed_room_power_levels_hookfn)
ircd::m::changed_room_power_levels_hookfn
{
	changed_room_power_levels,
	{
		{ "_site",    "vm.notify"            },
		{ "type",     "m.room.power_levels"  },
	}
};

void
ircd::m::changed_room_power_levels(const m::event &event,
                                   m::vm::eval &)
{
	if(myself(json::get<"sender"_>(event)))
		return;

	log::info
	{
		m::log, "%s changed power_levels in %s [%s]",
		json::get<"sender"_>(event),
		json::get<"room_id"_>(event),
		string_view{event.event_id}
    };
}

decltype(ircd::m::auth_room_power_levels_hookfn)
ircd::m::auth_room_power_levels_hookfn
{
	auth_room_power_levels,
	{
		{ "_site",    "room.auth"            },
		{ "type",     "m.room.power_levels"  },
	}
};

void
ircd::m::auth_room_power_levels(const m::event &event,
                                room::auth::hookdata &data)
{
	using FAIL = room::auth::FAIL;

	// 10. If type is m.room.power_levels:
	assert(json::get<"type"_>(event) == "m.room.power_levels");

	// a. If users key in content is not a dictionary with keys that are
	// valid user IDs with values that are integers (or a string that is
	// an integer), reject.
	if(!json::type(json::get<"content"_>(event).get("users"), json::OBJECT))
		throw FAIL
		{
			"m.room.power_levels content.users is not a json object."
		};

	for(const auto &[user_id, value] : json::object(at<"content"_>(event).at("users")))
	{
		if(!m::valid(m::id::USER, user_id))
		{
			log::dwarning
			{
				m::log, "Power levels in %s 'users' invalid entry '%s'; not user mxid.",
				string_view{event.event_id},
				user_id,
			};

			continue;
		}

		if(!lex_castable<int64_t>(unquote(value)))
			throw FAIL
			{
				"m.room.power_levels content.users value is not an integer."
			};
	}

//	// b. If there is no previous m.room.power_levels event in the room, allow.
//	if(!data.auth_power)
//	{
//		data.allow = true;
//		return;
//	};

	// b'. If there is no previous m.room.power_levels event in the room,
	// allow if the sender is the room creator.
	if(!data.auth_power && data.auth_create && data.auth_member_sender)
	{
		const json::string &creator
		{
			json::get<"content"_>(*data.auth_create).get("creator")
		};

		if(creator == json::get<"state_key"_>(*data.auth_member_sender))
		{
			data.allow = true;
			return;
		}
	}

	if(!data.auth_power)
		throw FAIL
		{
			"Cannot create the m.room.power_levels event."
		};

	if(!data.auth_create)
		throw FAIL
		{
			"Missing m.room.create in auth_events."
		};

	const m::room::power old_power
	{
		*data.auth_power, *data.auth_create
	};

	const m::room::power new_power
	{
		event, *data.auth_create
	};

	const int64_t current_level
	{
		old_power.level_user(at<"sender"_>(event))
	};

	// c. For each of the keys users_default, events_default,
	// state_default, ban, redact, kick, invite, as well as each entry
	// being changed under the events or users keys:
	static const string_view keys[]
	{
		"users_default",
		"events_default",
		"state_default",
		"ban",
		"redact",
		"kick",
		"invite",
	};

	for(const auto &key : keys)
	{
		const int64_t old_level(old_power.level(key));
		const int64_t new_level(new_power.level(key));
		if(old_level == new_level)
			continue;

		// i. If the current value is higher than the sender's current
		// power level, reject.
		if(old_level > current_level)
			throw FAIL
			{
				"m.room.power_levels property denied to sender's power level."
			};

		// ii. If the new value is higher than the sender's current power
		// level, reject.
		if(new_level > current_level)
			throw FAIL
			{
				"m.room.power_levels property exceeds sender's power level."
			};
	}

	old_power.for_each("users", [&new_power, &current_level]
	(const string_view &user_id, const int64_t &old_level)
	{
		if(new_power.has_user(user_id))
			if(new_power.level_user(user_id) == old_level)
				return true;

		// i. If the current value is higher than the sender's current
		// power level, reject.
		if(old_level > current_level)
			throw FAIL
			{
				"m.room.power_levels user property denied to sender's power level."
			};

		// ii. If the new value is higher than the sender's current power
		// level, reject.
		if(new_power.level_user(user_id) > current_level)
			throw FAIL
			{
				"m.room.power_levels user property exceeds sender's power level."
			};

		return true;
	});

	new_power.for_each("users", [&old_power, &current_level]
	(const string_view &user_id, const int64_t &new_level)
	{
		if(old_power.has_user(user_id))
			if(old_power.level_user(user_id) == new_level)
				return true;

		// i. If the current value is higher than the sender's current
		// power level, reject.
		if(new_level > current_level)
			throw FAIL
			{
				"m.room.power_levels user property exceeds sender's power level."
			};

		return true;
	});

	old_power.for_each("events", [&new_power, &current_level]
	(const string_view &type, const int64_t &old_level)
	{
		if(new_power.has_event(type))
			if(new_power.level_event(type) == old_level)
				return true;

		// i. If the current value is higher than the sender's current
		// power level, reject.
		if(old_level > current_level)
			throw FAIL
			{
				"m.room.power_levels event property denied to sender's power level."
			};

		// ii. If the new value is higher than the sender's current power
		// level, reject.
		if(new_power.level_event(type) > current_level)
			throw FAIL
			{
				"m.room.power_levels event property exceeds sender's power level."
			};

		return true;
	});

	new_power.for_each("events", [&old_power, &current_level]
	(const string_view &type, const int64_t &new_level)
	{
		if(old_power.has_event(type))
			if(old_power.level_event(type) == new_level)
				return true;

		// i. If the current value is higher than the sender's current
		// power level, reject.
		if(new_level > current_level)
			throw FAIL
			{
				"m.room.power_levels event property exceeds sender's power level."
			};

		return true;
	});

	// d. For each entry being changed under the users key...
	old_power.for_each("users", [&event, &new_power, &current_level]
	(const string_view &user_id, const int64_t &old_level)
	{
		// ...other than the sender's own entry:
		if(user_id == at<"sender"_>(event))
			return true;

		if(new_power.has_user(user_id))
			if(new_power.level_user(user_id) == old_level)
				return true;

		// i. If the current value is equal to the sender's current power
		// level, reject.
		if(old_level == current_level)
			throw FAIL
			{
				"m.room.power_levels user property denied to sender's power level."
			};

		return true;
	});

	// e. Otherwise, allow.
	data.allow = true;
}
