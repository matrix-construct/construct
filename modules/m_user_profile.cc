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

static void _rejoin_room(const m::room &room, const m::user &user);
static void _rejoin_rooms(const m::user::id &user_id);
static void handle_my_profile_changed__displayname(const m::event &event);
static void handle_my_profile_changed__avatar_url(const m::event &event);
static void handle_my_profile_changed(const m::event &, m::vm::eval &);

mapi::header
IRCD_MODULE
{
	"Matrix user profiles."
};

const m::hookfn<m::vm::eval &>
my_profile_changed
{
	handle_my_profile_changed,
	{
		{ "_site",  "vm.effect"     },
		{ "type",   "ircd.profile"  },
		{ "origin",  my_host()      },
	}
};

void
handle_my_profile_changed(const m::event &event,
                          m::vm::eval &eval)
{
	const m::user::id &user_id
	{
		json::get<"sender"_>(event)
	};

	if(!my(event) || !my(user_id))
		return;

	// The event has to be an ircd.profile in the user's room, not just a
	// random ircd.profile typed event in some other room...
	const m::user::room user_room{user_id};
	if(json::get<"room_id"_>(event) != user_room.room_id)
		return;

	if(json::get<"state_key"_>(event) == "displayname")
		return handle_my_profile_changed__displayname(event);

	if(json::get<"state_key"_>(event) == "avatar_url")
		return handle_my_profile_changed__avatar_url(event);
}

void
handle_my_profile_changed__avatar_url(const m::event &event)
{
	_rejoin_rooms(at<"sender"_>(event));
}

void
handle_my_profile_changed__displayname(const m::event &event)
{
	_rejoin_rooms(at<"sender"_>(event));
}

void
_rejoin_rooms(const m::user::id &user_id)
{
	assert(my(user_id));
	const m::user::rooms &rooms
	{
		user_id
	};

	rooms.for_each("join", [&user_id]
	(const m::room &room, const string_view &)
	{
		_rejoin_room(room, user_id);
	});
}

void
_rejoin_room(const m::room &room,
             const m::user &user)
try
{
	m::join(room, user);
}
catch(const std::exception &e)
{
	log::error
	{
		"Failed to rejoin '%s' to room '%s' to update profile",
		string_view{user.user_id},
		string_view{room.room_id}
	};
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/m/user/profile.h
//

m::event::id::buf
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

conf::item<seconds>
remote_request_timeout
{
	{ "name",    "ircd.m.user.profile.remote_request.timeout" },
	{ "default", 10L                                          }
};

void
IRCD_MODULE_EXPORT
ircd::m::user::profile::fetch(const m::user &user,
                              const net::hostport &remote,
                              const string_view &key)
{
	const unique_buffer<mutable_buffer> buf
	{
		64_KiB
	};

	m::v1::query::opts opts;
	opts.remote = remote?: user.user_id.host();
	opts.dynamic = true;
	m::v1::query::profile federation_request
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
