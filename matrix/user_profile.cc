// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	extern conf::item<seconds> remote_request_timeout;
}

decltype(ircd::m::remote_request_timeout)
ircd::m::remote_request_timeout
{
	{ "name",    "ircd.m.user.profile.remote_request.timeout" },
	{ "default", 10L                                          }
};

ircd::m::event::id::buf
IRCD_MODULE_EXPORT
ircd::m::user::profile::set(const string_view &key,
                            const string_view &val)
const
{
	const m::user::room user_room
	{
		user
	};

	return m::send(user_room, user, "ircd.profile", key,
	{
		{ "text", val }
	});
}

ircd::string_view
IRCD_MODULE_EXPORT
ircd::m::user::profile::get(const mutable_buffer &out,
                            const string_view &key)
const
{
	string_view ret;
	get(std::nothrow, key, [&out, &ret]
	(const string_view &key, const string_view &val)
	{
		ret = { data(out), copy(out, val) };
	});

	return ret;
}

void
IRCD_MODULE_EXPORT
ircd::m::user::profile::get(const string_view &key,
                            const closure &closure)
const
{
	if(!get(std::nothrow, key, closure))
		throw m::NOT_FOUND
		{
			"Property %s in profile for %s not found",
			key,
			string_view{user.user_id}
		};
}

bool
IRCD_MODULE_EXPORT
ircd::m::user::profile::get(std::nothrow_t,
                            const string_view &key,
                            const closure &closure)
const
{
	const user::room user_room{user};
	const room::state state{user_room};
	const event::idx event_idx
	{
		state.get(std::nothrow, "ircd.profile", key)
	};

	return event_idx && m::get(std::nothrow, event_idx, "content", [&closure, &key]
	(const json::object &content)
	{
		const string_view &value
		{
			content.get("text")
		};

		closure(key, value);
	});
}

bool
IRCD_MODULE_EXPORT
ircd::m::user::profile::for_each(const closure_bool &closure)
const
{
	const user::room user_room{user};
	const room::state state{user_room};
	return state.for_each("ircd.profile", [&closure]
	(const string_view &type, const string_view &state_key, const event::idx &event_idx)
	{
		bool ret{true};
		m::get(std::nothrow, event_idx, "content", [&closure, &state_key, &ret]
		(const json::object &content)
		{
			const string_view &value
			{
				content.get("text")
			};

			ret = closure(state_key, value);
		});

		return ret;
	});
}

void
IRCD_MODULE_EXPORT
ircd::m::user::profile::fetch(const m::user &user,
                              const string_view &remote,
                              const string_view &key)
{
	const unique_buffer<mutable_buffer> buf
	{
		64_KiB
	};

	m::fed::query::opts opts;
	opts.remote = remote?: user.user_id.host();
	opts.dynamic = true;
	m::fed::query::profile federation_request
	{
		user.user_id, key, buf, std::move(opts)
	};

	federation_request.wait(seconds(remote_request_timeout));
	const http::code &code{federation_request.get()};
	const json::object response
	{
		federation_request
	};

	if(!exists(user))
		create(user);

	const m::user::profile profile{user};
	for(const auto &member : response)
	{
		bool exists{false};
		profile.get(std::nothrow, member.first, [&exists, &member]
		(const string_view &key, const string_view &val)
		{
			exists = member.second == val;
		});

		if(!exists)
			profile.set(member.first, member.second);
	}
}
