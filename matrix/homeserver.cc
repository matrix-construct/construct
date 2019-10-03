// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	static void bootstrap(homeserver &);
}

// Linkage for the container of all active clients for iteration purposes.
template<>
decltype(ircd::util::instance_multimap<ircd::string_view, ircd::m::homeserver, std::less<>>::map)
ircd::util::instance_multimap<ircd::string_view, ircd::m::homeserver, std::less<>>::map
{};

[[gnu::hot]]
ircd::m::user::id
ircd::m::me()
{
	auto &my
	{
		m::my()
	};

	return my.self;
}

[[gnu::hot]]
ircd::m::user::id
ircd::m::me(const string_view &origin)
{
	auto &my
	{
		m::my(origin)
	};

	return my.self;
}

[[gnu::hot]]
ircd::m::homeserver &
ircd::m::my()
{
	if(unlikely(!homeserver::primary))
		throw m::NOT_A_HOMESERVER
		{
			"I do not host any homeserver here."
		};

	return *homeserver::primary;
}

[[gnu::hot]]
ircd::m::homeserver &
ircd::m::my(const string_view &name)
{
	const auto &it
	{
		homeserver::map.find(name)
	};

	if(unlikely(it == end(homeserver::map)))
		throw m::NOT_MY_HOMESERVER
		{
			"I do not host any '%s' homeserver here.",
			name,
		};

	const auto &ptr
	{
		it->second
	};

	assert(ptr);
	return *ptr;
}

bool
ircd::m::for_each(const std::function<bool (homeserver &)> &closure)
{
	for(auto &[name, hs_p] : homeserver::map)
		if(!closure(*hs_p))
			return false;

	return true;
}

const ircd::ed25519::sk &
ircd::m::secret_key(const homeserver &homeserver)
{
	assert(homeserver.key);
	return homeserver.key->secret_key;
}

ircd::string_view
ircd::m::public_key_id(const homeserver &homeserver)
{
	assert(homeserver.key);
	return homeserver.key->public_key_id;
}

bool
ircd::m::server_name(const homeserver &homeserver,
                     const string_view &server_name)
{
	return server_name == m::server_name(homeserver);
}

bool
ircd::m::origin(const homeserver &homeserver,
                      const string_view &origin)
{
    return origin == m::origin(homeserver);
}

ircd::string_view
ircd::m::server_name(const homeserver &homeserver)
{
    assert(homeserver.opts);
    return homeserver.opts->server_name;
}

ircd::string_view
ircd::m::origin(const homeserver &homeserver)
{
    assert(homeserver.opts);
    return homeserver.opts->origin;
}

//
// homeserver::homeserver
//

decltype(ircd::m::homeserver::primary)
ircd::m::homeserver::primary;

IRCD_MODULE_EXPORT
ircd::m::homeserver *
ircd::m::homeserver::init(const struct opts *const opts)
{
	return new homeserver
	{
		opts
	};
}

void
IRCD_MODULE_EXPORT
ircd::m::homeserver::fini(homeserver *const homeserver)
noexcept
{
	delete homeserver;
}

//
// homeserver::homeserver::homeserver
//

IRCD_MODULE_EXPORT
ircd::m::homeserver::homeserver(const struct opts *const &opts)
:instance_multimap
{
	string_view{opts->origin}
}
,opts
{
	opts
}
,key
{
	std::make_unique<struct key>(opts->origin)
}
,database
{
	std::make_shared<dbs::init>(opts->server_name, std::string{})
}
,rooms
{
	{ "ircd", opts->origin },
}
,self
{
	"ircd", opts->origin
}
{
	primary = primary?: this;

	modules =
	{
		begin(matrix::module_names), end(matrix::module_names)
	};

	if(primary == this && dbs::events && sequence(*dbs::events) == 0)
		bootstrap(*this);
}

ircd::m::homeserver::~homeserver()
noexcept
{
	while(!modules.empty())
		modules.pop_back();
}

//
// homeserver::keys
//

/*
namespace ircd::m
{
	extern conf::item<std::string> ed25519_key_dir;
}

decltype(ircd::m::ed25519_key_dir)
ircd::m::ed25519_key_dir
{
	{ "name",     "ircd.keys.ed25519_key_dir"  },
	{ "default",  fs::cwd()                    },
};

void
IRCD_MODULE_EXPORT
ircd::m::self::init::federation_ed25519()
{
	if(empty(m::self::origin))
		throw error
		{
			"The m::self::origin must be set to init my ed25519 key."
		};

	const std::string path_parts[]
	{
		std::string{ed25519_key_dir},
		m::self::origin + ".ed25519",
	};

	const std::string sk_file
	{
		ircd::string(fs::PATH_MAX_LEN, [&](const mutable_buffer &buf)
		{
			return fs::path(buf, path_parts);
		})
	};

	if(fs::exists(sk_file) || ircd::write_avoid)
		log::info
		{
			m::log, "Using ed25519 secret key @ `%s'", sk_file
		};
	else
		log::notice
		{
			m::log, "Creating ed25519 secret key @ `%s'", sk_file
		};

	m::self::secret_key = ed25519::sk
	{
		sk_file, &m::self::public_key
	};

	m::self::public_key_b64 = b64encode_unpadded(m::self::public_key);
	const fixed_buffer<const_buffer, sha256::digest_size> hash
	{
		sha256{m::self::public_key}
	};

	const auto public_key_hash_b58
	{
		b58encode(hash)
	};

	static const auto trunc_size{8};
	m::self::public_key_id = fmt::snstringf
	{
		32, "ed25519:%s", trunc(public_key_hash_b58, trunc_size)
	};

	log::info
	{
		m::log, "Current key is '%s' and the public key is: %s",
		m::self::public_key_id,
		m::self::public_key_b64
	};
}
*/

//
// create_my_key
//

ircd::m::homeserver::key::key(const string_view &origin)
{
/*
	const json::members verify_keys_
	{{
		string_view{m::self::public_key_id},
		{
			{ "key", m::self::public_key_b64 }
		}
	}};

	m::keys my_key;
	json::get<"server_name"_>(my_key) = my_host();
	json::get<"old_verify_keys"_>(my_key) = "{}";

	//TODO: conf
	json::get<"valid_until_ts"_>(my_key) =
		ircd::time<milliseconds>() + milliseconds(1000UL * 60 * 60 * 24 * 180).count();

	const json::strung verify_keys{verify_keys_}; // must be on stack until my_keys serialized.
	json::get<"verify_keys"_>(my_key) = verify_keys;

	const json::strung presig
	{
		my_key
	};

	const ed25519::sig sig
	{
		m::self::secret_key.sign(const_buffer{presig})
	};

	char signature[256];
	const json::strung signatures{json::members
	{
		{ my_host(),
		{
			{ string_view{m::self::public_key_id}, b64encode_unpadded(signature, sig) }
		}}
	}};

	json::get<"signatures"_>(my_key) = signatures;
	keys::cache::set(json::strung{my_key});
*/
}

///////////////////////////////////////////////////////////////////////////////
//
// m/self.h
//

ircd::string_view
ircd::m::self::my_host()
{
	return origin(my());
}

bool
ircd::m::self::host(const string_view &other)
{
	const net::hostport other_host{other};
	for(const auto &[my_network, hs_p] : homeserver::map)
	{
		// port() is 0 when the origin has no port (and implies 8448)
		const auto port
		{
			net::port(net::hostport(my_network))
		};

		// If my_host has a port number, then the argument must also have the
		// same port number.
		if(port && my_network == other)
			return true;
		else if(port)
			continue;

		/// If my_host has no port number, then the argument can have port
		/// 8448 or no port number, which will initialize net::hostport.port to
		/// the "canon_port" of 8448.
		assert(net::canon_port == 8448);
		if(net::port(other_host) != net::canon_port)
			continue;

		/// Both myself and input are using 8448; now the name has to match.
		if(my_network != host(other_host))
			continue;

		return true;
	}

	return false;
}

bool
ircd::m::self::my_host(const string_view &name)
{
	const auto it
	{
		homeserver::map.find(name)
	};

	return it != end(homeserver::map);
}

//
// signon/signoff greetings
//

/*
namespace ircd::m::self
{
	void signon(), signoff();
}

ircd::conf::item<std::string>
me_online_status_msg
{
	{ "name",     "ircd.me.online.status_msg"          },
	{ "default",  "Wanna chat? IRCd at your service!"  }
};

ircd::conf::item<std::string>
me_offline_status_msg
{
	{ "name",     "ircd.me.offline.status_msg"     },
	{ "default",  "Catch ya on the flip side..."   }
};

void
ircd::m::self::signon()
{
	if(!ircd::write_avoid && vm::sequence::retired != 0)
		presence::set(me, "online", me_online_status_msg);
}

void
ircd::m::self::signoff()
{
	if(!std::uncaught_exceptions() && !ircd::write_avoid)
		presence::set(me, "offline", me_offline_status_msg);
}

//
// init
//

ircd::m::self::init::init()
try
{
	// Sanity check that these are valid hostname strings. This was likely
	// already checked, so these validators will simply throw without very
	// useful error messages if invalid strings ever make it this far.
	rfc3986::valid_host(origin);
	rfc3986::valid_host(servername);

	ircd_user_id = {"ircd", origin};
	m::me = {ircd_user_id};

	ircd_room_id = {"ircd", origin};
	m::my_room = {ircd_room_id};

	if(origin == "localhost")
		log::warning
		{
			"The origin is configured or has defaulted to 'localhost'"
		};
}
catch(const std::exception &e)
{
	log::critical
	{
		m::log, "Failed to init self origin[%s] servername[%s]",
		origin,
		servername,
	};

	throw;
}

static const auto self_init{[]
{
	ircd::m::self::init();
	return true;
}()};
*/

void
ircd::m::bootstrap(homeserver &homeserver)
try
{
	assert(dbs::events);
	assert(db::sequence(*dbs::events) == 0);
	if(homeserver.self.hostname() == "localhost")
		log::warning
		{
			"The server's name is configured to localhost. This is probably not what you want."
		};

	if(!exists(homeserver.self))
	{
		create(homeserver.self);
		user(homeserver.self).activate();
	}

	const m::room::id::buf node_room_id
	{
		node(origin(homeserver)).room_id()
	};

	if(!exists(node_room_id))
		create(room(node_room_id));

	const m::room::id::buf my_room
	{
		"ircd", origin(homeserver)
	};

	if(!exists(my_room))
		create(my_room, homeserver.self, "internal");

	if(!membership(my_room, homeserver.self, "join"))
		join(room(my_room), user(homeserver.self));

	if(!room(my_room).has("m.room.name", ""))
		send(my_room, homeserver.self, "m.room.name", "",
		{
			{ "name", "IRCd's Room" }
		});

	if(!room(my_room).has("m.room.topic", ""))
		send(my_room, homeserver.self, "m.room.topic", "",
		{
			{ "topic", "The daemon's den." }
		});

	const m::room::id::buf tokens_room
	{
		"tokens", origin(homeserver)
	};

	if(!exists(tokens_room))
		create(tokens_room, homeserver.self);

	if(!room(tokens_room).has("m.room.name",""))
		send(tokens_room, homeserver.self, "m.room.name", "",
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
		"bootstrap %s error :%s",
		server_name(homeserver),
		e.what()
	};
}
