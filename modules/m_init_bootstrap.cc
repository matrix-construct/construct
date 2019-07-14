// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::mapi::header
IRCD_MODULE
{
	"Matrix initial bootstrap support."
};

void
IRCD_MODULE_EXPORT
ircd::m::init::bootstrap()
try
{
	assert(dbs::events);
	assert(db::sequence(*dbs::events) == 0);
	if(me.user_id.hostname() == "localhost")
		log::warning
		{
			"The ircd.origin is configured to localhost. This is probably not"
			" what you want. To fix this now, you will have to remove the "
			" database and start over."
		};

	if(!exists(user::users))
		create(user::users, me.user_id, "internal");

	if(!exists(my_room))
		create(my_room, me.user_id, "internal");

	if(!exists(me))
	{
		create(me.user_id);
		me.activate();
	}

	if(!my_room.membership(me.user_id, "join"))
		join(my_room, me.user_id);

	if(!my_room.has("m.room.name", ""))
		send(my_room, me.user_id, "m.room.name", "",
		{
			{ "name", "IRCd's Room" }
		});

	if(!my_room.has("m.room.topic", ""))
		send(my_room, me.user_id, "m.room.topic", "",
		{
			{ "topic", "The daemon's den." }
		});

	if(!user::users.has("m.room.name", ""))
		send(user::users, me.user_id, "m.room.name", "",
		{
			{ "name", "Users" }
		});

	if(!exists(user::tokens))
		create(user::tokens, me.user_id);

	if(!user::tokens.has("m.room.name",""))
		send(user::tokens, me.user_id, "m.room.name", "",
		{
			{ "name", "User Tokens" }
		});

	log::info
	{
		log, "Bootstrap event generation completed nominally."
	};
}
catch(const std::exception &e)
{
	throw ircd::panic
	{
		"bootstrap error :%s", e.what()
	};
}
