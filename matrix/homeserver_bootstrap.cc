// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	static void bootstrap_event_vector(homeserver &);
}

void
ircd::m::homeserver::bootstrap()
try
{
	assert(dbs::events);
	assert(db::sequence(*dbs::events) == 0);

	assert(opts);
	if(opts->bootstrap_vector_path)
		return bootstrap_event_vector(*this);

	assert(this->self);
	const m::user::id &my_id
	{
		this->self
	};

	const m::user me
	{
		my_id
	};

	const m::node my_node
	{
		origin(*this)
	};

	const m::node::room node_room
	{
		my_node
	};

	const m::room::id::buf my_room_id
	{
		"ircd", origin(*this)
	};

	const m::room my_room
	{
		my_room_id
	};

	const m::room::id::buf conf_room_id
	{
		"conf", origin(*this)
	};

	const m::room conf_room
	{
		conf_room_id
	};

	const m::room::id::buf tokens_room_id
	{
		"tokens", origin(*this)
	};

	const m::room tokens_room
	{
		tokens_room_id
	};

	const m::room::id::buf public_room_id
	{
		"public", origin(*this)
	};

	const m::room public_room
	{
		public_room_id
	};

	const m::room::id::buf alias_room_id
	{
		"alias", origin(*this)
	};

	const m::room alias_room
	{
		alias_room_id
	};

	const m::room::id::buf control_room_id
	{
		"control", origin(*this)
	};

	const m::room control_room
	{
		control_room_id
	};

	const m::room::id::buf bridge_room_id
	{
		"bridge", origin(*this)
	};

	const m::room bridge_room
	{
		bridge_room_id
	};

	if(my_id.hostname() == "localhost")
		log::warning
		{
			"The server's name is configured to localhost. This is probably not what you want."
		};

	assert(key);
	assert(!key->verify_keys.empty());
	m::keys::cache::set(key->verify_keys);

	create(me);
	const_cast<m::user &>(me).activate();

	create(my_room, me);
	create(conf_room, me);
	create(tokens_room, me);
	create(public_room, me);
	create(alias_room, me);
	create(control_room, me);
	create(bridge_room, me);

	send(my_room, me, "m.room.name", "",
	{
		{ "name", "IRCd's Room" }
	});

	send(my_room, me, "m.room.topic", "",
	{
		{ "topic", "The daemon's den." }
	});

	send(conf_room, me, "m.room.name", "",
	{
		{ "name", "Server Configuration" }
	});

	send(tokens_room, me, "m.room.name", "",
	{
		{ "name", "User Tokens" }
	});

	send(control_room, me, "m.room.name", "",
	{
		{ "name", "Control Room" }
	});

	log::info
	{
		log, "Bootstrap event generation completed nominally."
	};
}
catch(const std::exception &e)
{
	log::logf
	{
		log, log::level::CRITICAL,
		"Failed to bootstrap server '%s' on network '%s' :%s",
		server_name(*this),
		origin(*this),
		e.what()
	};

	throw ircd::panic
	{
		"bootstrap %s error :%s",
		server_name(*this),
		e.what()
	};
}

void
ircd::m::bootstrap_event_vector(homeserver &homeserver)
try
{
	const string_view &path
	{
		homeserver.opts->bootstrap_vector_path
	};

	fs::fd::opts fileopts(std::ios::in);
	fileopts.sequential = true;
	const fs::fd file
	{
		path, fileopts
	};

	fs::map::opts map_opts(fileopts);
	const fs::map map
	{
		file, map_opts
	};

	char pbuf[2][48];
	log::notice
	{
		log, "Bootstrapping database from event vector @ `%s' %s",
		path,
		pretty(pbuf[0], iec(size(map))),
	};

	const json::array events
	{
		const_buffer{map}
	};

	vm::opts vmopts;
	vmopts.non_conform.set(event::conforms::MISMATCH_ORIGIN_SENDER);
	vmopts.phase.reset();
	vmopts.phase[vm::phase::CONFORM] = true;
	vmopts.phase[vm::phase::INDEX] = true;
	vmopts.phase[vm::phase::WRITE] = true;
	vmopts.infolog_accept = true;

	util::timer stopwatch;
	vm::eval eval
	{
		events, vmopts
	};

	log::notice
	{
		log, "Bootstrapped %zu in %s from `%s' in %s",
		vm::sequence::retired,
		pretty(pbuf[0], iec(size(string_view(events)))),
		path,
		stopwatch.pretty(pbuf[1]),
	};
}
catch(const std::exception &e)
{
	log::logf
	{
		log, log::level::CRITICAL,
		"Failed to bootstrap server '%s' on network '%s' :%s",
		server_name(homeserver),
		origin(homeserver),
		e.what(),
	};

	throw ircd::error
	{
		"bootstrap %s :%s",
		server_name(homeserver),
		e.what(),
	};
}
