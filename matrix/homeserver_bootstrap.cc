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
		bootstrap_event_vector(*this);

	assert(this->self);
	const m::user::id &my_id
	{
		this->self
	};

	if(my_id.hostname() == "localhost")
		log::warning
		{
			"The server's name is configured to localhost. This is probably not what you want."
		};

	m::user me
	{
		my_id
	};

	if(!exists(me))
	{
		create(me);
		me.activate();
	}

	const m::node my_node
	{
		origin(*this)
	};

	const m::node::room node_room
	{
		my_node
	};

	if(!exists(node_room))
		create(node_room, me);

	const m::room::id::buf my_room_id
	{
		"ircd", origin(*this)
	};

	const m::room my_room
	{
		my_room_id
	};

	if(!exists(my_room))
		create(my_room, me);

	if(!membership(my_room, me, "join"))
		join(my_room, me);

	if(!my_room.has("m.room.name", ""))
		send(my_room, me, "m.room.name", "",
		{
			{ "name", "IRCd's Room" }
		});

	if(!my_room.has("m.room.topic", ""))
		send(my_room, me, "m.room.topic", "",
		{
			{ "topic", "The daemon's den." }
		});

	const m::room::id::buf conf_room_id
	{
		"conf", origin(*this)
	};

	const m::room conf_room
	{
		conf_room_id
	};

	if(!exists(conf_room))
		create(conf_room, me);

	if(!conf_room.has("m.room.name",""))
		send(conf_room, me, "m.room.name", "",
		{
			{ "name", "Server Configuration" }
		});

	const m::room::id::buf tokens_room_id
	{
		"tokens", origin(*this)
	};

	const m::room tokens_room
	{
		tokens_room_id
	};

	if(!exists(tokens_room))
		create(tokens_room, me);

	if(!tokens_room.has("m.room.name",""))
		send(tokens_room, me, "m.room.name", "",
		{
			{ "name", "User Tokens" }
		});

	const m::room::id::buf public_room_id
	{
		"public", origin(*this)
	};

	const m::room public_room
	{
		public_room_id
	};

	if(!exists(public_room))
		create(public_room, me);

	const m::room::id::buf alias_room_id
	{
		"alias", origin(*this)
	};

	const m::room alias_room
	{
		alias_room_id
	};

	if(!exists(alias_room))
		create(alias_room, me);

	const m::room::id::buf control_room_id
	{
		"control", origin(*this)
	};

	const m::room control_room
	{
		control_room_id
	};

	if(!exists(control_room))
		create(control_room, me);

	if(!control_room.has("m.room.name", ""))
		send(control_room, me, "m.room.name", "",
		{
			{ "name", "Control Room" }
		});

	const m::room::id::buf bridge_room_id
	{
		"bridge", origin(*this)
	};

	const m::room bridge_room
	{
		bridge_room_id
	};

	if(!exists(bridge_room))
		create(bridge_room, me);

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

	log::notice
	{
		log, "Bootstrapping database from event vector @ `%s'",
		path,
	};

	const unique_mutable_buffer mb
	{
		4_MiB
	};

	const_buffer buf;
	fs::read_opts ropts;
	size_t reads(0), events(0);
	while(!!(buf = read(file, mb, ropts)))
	{
		++reads;
		const json::vector vec{buf};
		const size_t _events(events);
		for(auto it(begin(vec)); it != end(vec); ) try
		{
			ropts.offset += size(string_view(*it));
			const m::event event{*it};

			++events;
			++it;
		}
		catch(const json::parse_error &)
		{
			if(_events == events)
				throw;
			else
				break;
		}
		catch(const std::exception &e)
		{
			log::error
			{
				log, "Bootstrap event %zu :%s",
				events,
				e.what()
			};
		}

		char pbuf[48];
		log::info
		{
			log, "Bootstrapping progress %zu events; %s in %zu reads...",
			events,
			pretty(pbuf, iec(ropts.offset)),
			reads,
		};
	}

	char pbuf[48];
	log::info
	{
		log, "Bootstrapped %zu events in %s from %zu reads of `%s'",
		events,
		pretty(pbuf, iec(ropts.offset)),
		reads,
		path,
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
