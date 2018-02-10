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
	vm::fronts.map.clear();
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

	m::modules.emplace("root.so"s, "root.so"s);
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

	create(my_room, me.user_id);
	send(my_room, me.user_id, "m.room.name", "",
	{
		{ "name", "IRCd's Room" }
	});

	create(control, me.user_id);
	send(control, me.user_id, "m.room.name", "",
	{
		{ "name", "Control Room" }
	});

	create(user::accounts, me.user_id);
	join(user::accounts, me.user_id);
	send(user::accounts, me.user_id, "m.room.name", "",
	{
		{ "name", "User Accounts" }
	});

	create(user::sessions, me.user_id);
	send(user::sessions, me.user_id, "m.room.name", "",
	{
		{ "name", "User Sessions" }
	});

	create(filter::filters, me.user_id);
	send(filter::filters, me.user_id, "m.room.name", "",
	{
		{ "name", "User Filters Database" }
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
	my_room.get([](const auto &event)
	{
		std::cout << "mr G: " << event << std::endl;
	});

	my_room.prev([](const auto &event)
	{
		std::cout << "mr P: " << event << std::endl;
	});
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
// m/filter.h
//

const ircd::m::room::id::buf
filters_room_id
{
	"filters", ircd::my_host()
};

ircd::m::room
ircd::m::filter::filters
{
	filters_room_id
};

ircd::m::filter::filter(const string_view &filter_id,
                        const mutable_buffer &buf)
{
	const m::vm::query<m::vm::where::equal> query
	{
		{ "room_id",      filters.room_id   },
		{ "type",        "ircd.filter"      },
		{ "state_key",    filter_id         },
	};

	size_t len{0};
	m::vm::test(query, [&buf, &len]
	(const auto &event)
	{
		len = copy(buf, json::get<"content"_>(event));
		return true;
	});

	new (this) filter{json::object{buf}};
}

size_t
ircd::m::filter::size(const string_view &filter_id)
{
	const m::vm::query<m::vm::where::equal> query
	{
		{ "room_id",      filters.room_id   },
		{ "type",        "ircd.filter"      },
		{ "state_key",    filter_id         },
	};

	size_t ret{0};
	m::vm::test(query, [&ret]
	(const auto &event)
	{
		const string_view content
		{
			json::get<"content"_>(event)
		};

		ret = content.size();
		return true;
	});

	return ret;
}

///////////////////////////////////////////////////////////////////////////////
//
// m/user.h
//

const ircd::m::room::id::buf
accounts_room_id
{
	"accounts", ircd::my_host()
};

ircd::m::room
ircd::m::user::accounts
{
	accounts_room_id
};

const ircd::m::room::id::buf
sessions_room_id
{
	"sessions", ircd::my_host()
};

ircd::m::room
ircd::m::user::sessions
{
	sessions_room_id
};

/// Register the user by joining them to the accounts room.
///
/// The content of the join event may store keys including the registration
/// options. Once this call completes the join was successful and the user is
/// registered, otherwise throws.
void
ircd::m::user::activate(const json::members &contents)
try
{
	json::iov content;
	json::iov::push push[]
	{
		{ content, { "membership", "join" }},
	};

	size_t i(0);
	json::iov::push _content[contents.size()];
	for(const auto &member : contents)
		new (_content + i++) json::iov::push(content, member);

	send(accounts, me.user_id, "ircd.user", user_id, content);
	join(control, user_id);
}
catch(const m::ALREADY_MEMBER &e)
{
	throw m::error
	{
		http::CONFLICT, "M_USER_IN_USE", "The desired user ID is already in use."
	};
}

void
ircd::m::user::deactivate(const json::members &contents)
{
	json::iov content;
	json::iov::push push[]
	{
		{ content, { "membership", "leave" }},
	};

	size_t i(0);
	json::iov::push _content[contents.size()];
	for(const auto &member : contents)
		new (_content + i++) json::iov::push(content, member);

	send(accounts, me.user_id, "ircd.user", user_id, content);
}

void
ircd::m::user::password(const string_view &password)
try
{
	//TODO: ADD SALT
	char b64[64], hash[32];
	sha256{hash, password};
	const auto digest{b64encode_unpadded(b64, hash)};
	send(accounts, me.user_id, "ircd.password", user_id,
	{
		{ "sha256", digest }
	});
}
catch(const m::ALREADY_MEMBER &e)
{
	throw m::error
	{
		http::CONFLICT, "M_USER_IN_USE", "The desired user ID is already in use."
	};
}

bool
ircd::m::user::is_password(const string_view &supplied_password)
const
{
	//TODO: ADD SALT
	char b64[64], hash[32];
	sha256{hash, supplied_password};
	const auto supplied_hash
	{
		b64encode_unpadded(b64, hash)
	};

	static const string_view type{"ircd.password"};
	const string_view &state_key{user_id};
	const room room
	{
		accounts.room_id
	};

	bool ret{false};
	room::state{room}.get(type, state_key, [&supplied_hash, &ret]
	(const m::event &event)
	{
		const json::object &content
		{
			json::at<"content"_>(event)
		};

		const auto &correct_hash
		{
			unquote(content.at("sha256"))
		};

		ret = supplied_hash == correct_hash;
	});

	return ret;
}

bool
ircd::m::user::is_active()
const
{
	bool ret{false};
	room::state{accounts}.get("ircd.user", user_id, [&ret]
	(const m::event &event)
	{
		const json::object &content
		{
			at<"content"_>(event)
		};

		ret = unquote(content.at("membership")) == "join";
	});

	return ret;
}
