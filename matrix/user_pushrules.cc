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
		user_room.get(type, ruleid)
	};

	const m::event::id::buf event_id
	{
		m::event_id(event_idx, std::nothrow)
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

	// Present the default rules to the closure.
	if(likely(scope == "global"))
	{
		const auto &rules
		{
			push::rules::defaults.get<json::array>(kind)
		};

		for(const json::object &rule : rules)
		{
			const json::string &_ruleid
			{
				rule["rule_id"]
			};

			if(_ruleid == ruleid)
			{
				closure(path, rule);
				return true;
			}
		}
	}

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

	return m::get(std::nothrow, event_idx, "content", [&path, &closure]
	(const json::object &content)
	{
		closure(path, content);
	});
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

	// Present the default rules to the closure; note that the for_each
	// convention dictates if the user's closure returns false we'll never
	// get past this branch to iterate the default rules.
	if(!scope || scope == "global")
	{
		for(const auto &_kind : json::keys<decltype(push::rules::defaults)>())
		{
			if(kind && kind != _kind)
				continue;

			for(const json::object &rule : push::rules::defaults.at<json::array>(_kind))
			{
				const push::path path
				{
					"global", _kind, json::string{rule["rule_id"]}
				};

				if(!closure(path, rule))
					return false;
			}
		}
	}

	char typebuf[event::TYPE_MAX_SIZE];
	const room::state::type_prefix type
	{
		push::make_type(typebuf, path)
	};

	const user::room user_room
	{
		user
	};

	const room::state state
	{
		user_room
	};

	return state.for_each(type, room::state::closure_bool{[&closure]
	(const string_view &type, const string_view &state_key, const m::event::idx &event_idx)
	{
		const auto path
		{
			push::make_path(type, state_key)
		};

		return m::query<bool>(std::nothrow, event_idx, "content", true, [&path, &closure]
		(const json::object &content)
		{
			return closure(path, content);
		});
	}});
}
