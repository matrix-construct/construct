// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/m/m.h>

namespace ircd::m
{
	struct log::log log
	{
		"matrix", 'm'
	};

	std::map<std::string, ircd::module> modules;
	std::list<ircd::net::listener> listeners;

	static void leave_ircd_room();
	static void join_ircd_room();
}

const ircd::m::user::id::buf
ircd_user_id
{
	"ircd", ircd::my_host()  //TODO: hostname
};

ircd::m::user
ircd::m::me
{
	ircd_user_id
};

const ircd::m::room::id::buf
ircd_room_id
{
	"ircd", ircd::my_host()
};

ircd::m::room
ircd::m::my_room
{
	ircd_room_id
};

const ircd::m::room::id::buf
control_room_id
{
	"control", ircd::my_host()
};

ircd::m::room
ircd::m::control
{
	control_room_id
};

//
// init
//

ircd::m::init::init()
try
:conf
{
	ircd::conf
}
,_keys
{
	conf
}
{
	modules();
	if(db::sequence(*dbs::events) == 0)
		bootstrap();

	listeners();
	join_ircd_room();
}
catch(const m::error &e)
{
	log.error("%s %s", e.what(), e.content);
	throw;
}
catch(const std::exception &e)
{
	log.error("%s", e.what());
	throw;
}

ircd::m::init::~init()
noexcept try
{
	leave_ircd_room();
	m::listeners.clear();
	m::modules.clear();
}
catch(const m::error &e)
{
	log.critical("%s %s", e.what(), e.content);
	ircd::terminate();
}

void
ircd::m::init::modules()
{
	const string_view prefixes[]
	{
		"m_", "client_", "key_", "federation_", "media_"
	};

	for(const auto &name : mods::available())
		if(startswith_any(name, std::begin(prefixes), std::end(prefixes)))
			m::modules.emplace(name, name);

	m::modules.emplace("root"s, "root"s);
}

namespace ircd::m
{
	static void init_listener(const json::object &conf, const json::object &opts, const string_view &bindaddr);
	static void init_listener(const json::object &conf, const json::object &opts);
}

void
ircd::m::init::listeners()
{
	const json::array listeners
	{
		conf["listeners"]
	};

	if(m::listeners.empty())
		init_listener(conf, {});
	else
		for(const json::object opts : listeners)
			init_listener(conf, opts);
}

static void
ircd::m::init_listener(const json::object &conf,
                       const json::object &opts)
{
	const json::array binds
	{
		opts["bind_addresses"]
	};

	if(binds.empty())
		init_listener(conf, opts, "0.0.0.0");
	else
		for(const auto &bindaddr : binds)
			init_listener(conf, opts, unquote(bindaddr));
}

static void
ircd::m::init_listener(const json::object &conf,
                       const json::object &opts,
                       const string_view &host)
{
	const json::array resources
	{
		opts["resources"]
	};

	// resources has multiple names with different configs which are being
	// ignored :-/
	const std::string name{"Matrix"s};

	// Translate synapse options to our options (which reflect asio::ssl)
	const json::strung options{json::members
	{
		{ "name",                      name                          },
		{ "host",                      host                          },
		{ "port",                      opts.get("port", 8448L)       },
		{ "ssl_certificate_file_pem",  conf["tls_certificate_path"]  },
		{ "ssl_private_key_file_pem",  conf["tls_private_key_path"]  },
		{ "ssl_tmp_dh_file",           conf["tls_dh_params_path"]    },
	}};

	m::listeners.emplace_back(options);
}

void
ircd::m::init::bootstrap()
{
	assert(dbs::events);
	assert(db::sequence(*dbs::events) == 0);

	ircd::log::notice
	(
		"This appears to be your first time running IRCd because the events "
		"database is empty. I will be bootstrapping it with initial events now..."
	);

	create(user::users, me.user_id);
	me.activate();

	create(my_room, me.user_id);
	send(my_room, me.user_id, "m.room.name", "",
	{
		{ "name", "IRCd's Room" }
	});

	send(my_room, me.user_id, "m.room.topic", "",
	{
		{ "topic", "The daemon's den." }
	});

	create(control, me.user_id);
	join(control, me.user_id);
	send(control, me.user_id, "m.room.name", "",
	{
		{ "name", "Control Room" }
	});

	send(user::users, me.user_id, "m.room.name", "",
	{
		{ "name", "Users" }
	});

	create(user::tokens, me.user_id);
	send(user::tokens, me.user_id, "m.room.name", "",
	{
		{ "name", "User Tokens" }
	});

	_keys.bootstrap();

	message(control, me.user_id, "Welcome to the control room.");
	message(control, me.user_id, "I am the daemon. You can talk to me in this room by highlighting me.");
}

bool
ircd::m::self::host(const string_view &s)
{
	return s == host();
}

ircd::string_view
ircd::m::self::host()
{
	return "zemos.net"; //me.user_id.host();
}

void
ircd::m::join_ircd_room()
try
{
	join(my_room, me.user_id);
}
catch(const m::ALREADY_MEMBER &e)
{
	log.warning("IRCd did not shut down correctly...");
}

void
ircd::m::leave_ircd_room()
{
	leave(my_room, me.user_id);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/room.h
//

ircd::m::room
ircd::m::create(const id::room &room_id,
                const id::user &creator,
                const string_view &type)
{
	using prototype = room (const id::room &, const id::user &, const string_view &);

	static import<prototype> function
	{
		"client_createroom", "createroom__type"
	};

	return function(room_id, creator, type);
}

ircd::m::room
ircd::m::create(const id::room &room_id,
                const id::user &creator,
                const id::room &parent,
                const string_view &type)
{
	using prototype = room (const id::room &, const id::user &, const id::room &, const string_view &);

	static import<prototype> function
	{
		"client_createroom", "createroom__parent_type"
	};

	return function(room_id, creator, parent, type);
}

ircd::m::event::id::buf
ircd::m::join(const room &room,
              const id::user &user_id)
{
	using prototype = event::id::buf (const m::room &, const id::user &);

	static import<prototype> function
	{
		"client_rooms", "join__room_user"
	};

	return function(room, user_id);
}
