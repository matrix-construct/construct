// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

bool
ircd::m::user::pushrules::del(const path &path)
const
{
	const auto &[scope, kind, ruleid]
	{
		path
	};

	char typebuf[event::TYPE_MAX_SIZE];
	const string_view &type
	{
		push::make_type(typebuf, path)
	};

	const m::user::room user_room
	{
		user
	};

	const event::idx &event_idx
	{
		user_room.get(std::nothrow, type, ruleid)
	};

	const m::event::id::buf event_id
	{
		m::event_id(std::nothrow, event_idx)
	};

	if(!event_id)
		return false;

	const auto redact_id
	{
		m::redact(user_room, user, event_id, "deleted")
	};

	return true;
}

bool
ircd::m::user::pushrules::set(const path &path,
                              const json::object &content)
const
{
	const auto &[scope, kind, ruleid]
	{
		path
	};

	const push::rule rule
	{
		content
	};

	char typebuf[event::TYPE_MAX_SIZE];
	const string_view &type
	{
		push::make_type(typebuf, path)
	};

	const m::user::room user_room
	{
		user
	};

	const auto rule_event_id
	{
		m::send(user_room, user, type, ruleid, content)
	};

	return true;
}

void
ircd::m::user::pushrules::get(const path &path,
                              const closure &closure)
const
{
	const auto &[scope, kind, ruleid]
	{
		path
	};

	if(!get(std::nothrow, path, closure))
		throw m::NOT_FOUND
		{
			"push rule (%s,%s,%s) for user %s not found",
			scope,
			kind,
			ruleid,
			string_view{user.user_id}
		};
}

bool
ircd::m::user::pushrules::get(std::nothrow_t,
                              const path &path,
                              const closure &closure)
const
{
	const auto &[scope, kind, ruleid]
	{
		path
	};

	char typebuf[event::TYPE_MAX_SIZE];
	const string_view &type
	{
		push::make_type(typebuf, path)
	};

	const m::user::room user_room
	{
		user
	};

	const event::idx &event_idx
	{
		user_room.get(std::nothrow, type, ruleid)
	};

	const bool user_rule_found
	{
		m::get(std::nothrow, event_idx, "content", [&path, &closure, &event_idx]
		(const json::object &content)
		{
			closure(event_idx, path, content);
		})
	};

	// Allow user-set rule found with the same id as a server-default to
	// always take priority.
	if(user_rule_found)
		return true;

	// Present the default rule to the closure.
	if(likely(scope == "global"))
	{
		const auto &rules
		{
			push::rules::defaults.get<json::array>(kind)
		};

		for(const json::object rule : rules)
		{
			const json::string _ruleid
			{
				rule["rule_id"]
			};

			if(_ruleid != ruleid)
				continue;

			closure(0UL, path, rule);
			return true;
		}
	}

	// Nothing found
	return false;
}

bool
ircd::m::user::pushrules::for_each(const closure_bool &closure)
const
{
	return for_each(path{}, closure);
}

bool
ircd::m::user::pushrules::for_each(const path &path,
                                   const closure_bool &closure)
const
{
	const auto &[scope, kind, ruleid]
	{
		path
	};

	// If the path contains a ruleid there's at most one item to iterate...
	if(ruleid)
	{
		get(std::nothrow, path, closure);
		return true;
	}

	const user::room user_room
	{
		user
	};

	const room::state state
	{
		user_room
	};

	// Present the default rules to the closure
	if(!scope || scope == "global")
	{
		for(const auto &_kind : json::keys<decltype(push::rules::defaults)>())
		{
			if(kind && kind != _kind)
				continue;

			for(const json::object rule : push::rules::defaults.at<json::array>(_kind))
			{
				const json::string _ruleid
				{
					rule["rule_id"]
				};

				const push::path path
				{
					"global", _kind, _ruleid
				};

				char typebuf[event::TYPE_MAX_SIZE];
				const string_view &type
				{
					push::make_type(typebuf, path)
				};

				// Check if the user set a rule with the same path/id as a
				// server-default rule; if so, we want their rule to take
				// priority over the this server-default below.
				if(state.has(type, _ruleid))
					continue;

				// Present the server-default rule to the iteration.
				if(!closure(0UL, path, rule))
					return false;
			}
		}
	}

	char typebuf[event::TYPE_MAX_SIZE];
	const room::state::type_prefix type
	{
		push::make_type(typebuf, path)
	};

	return state.for_each(type, [&closure]
	(const string_view &type, const string_view &state_key, const m::event::idx &event_idx)
	{
		const auto path
		{
			push::make_path(type, state_key)
		};

		return m::query<bool>(std::nothrow, event_idx, "content", true, [&]
		(const json::object &content)
		{
			return closure(event_idx, path, content);
		});
	});
}
