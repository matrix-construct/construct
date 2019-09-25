// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
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
	"Matrix profile."
};

m::hookfn<m::vm::eval &>
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
