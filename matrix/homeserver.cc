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
	static void signon(homeserver &), signoff(homeserver &) noexcept;

	extern conf::item<std::string> online_status_msg;
	extern conf::item<std::string> offline_status_msg;
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
ircd::m::myself(const m::user::id &user_id)
{
	return !for_each([&user_id]
	(const auto &homeserver)
	{
		return homeserver.self != user_id;
	});
}

bool
ircd::m::my_origin(const string_view &origin)
{
	return !for_each([&origin]
	(const auto &homeserver)
	{
		return m::origin(homeserver) != origin;
	});
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

namespace ircd::m
{
	std::unique_ptr<fetch::init> _fetch;
	std::unique_ptr<vm::init> _vm;
}

decltype(ircd::m::homeserver::primary)
ircd::m::homeserver::primary;

IRCD_MODULE_EXPORT
ircd::m::homeserver *
ircd::m::homeserver::init(const struct opts *const opts)
try
{
	assert(opts);
	rfc3986::valid_host(opts->origin);
	rfc3986::valid_host(opts->server_name);

	return new homeserver
	{
		opts
	};
}
catch(const m::error &e)
{
	assert(opts);
	// Don't rethrow m::error so the catcher doesn't depend on
	// RTTI/personality from this shlib after it unloads.
	throw ircd::error
	{
		"Failed to initialize homeserver %s of %s :%s :%s",
		opts->server_name,
		opts->origin,
		e.errcode(),
		e.errstr(),
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
try
:instance_multimap
{
	string_view{opts->origin}
}
,opts{[this, &opts]
{
	primary = primary?: this; //TODO: xxx
	return opts;
}()}
,key
{
	std::make_unique<struct key>(*opts)
}
,database
{
	std::make_shared<dbs::init>(opts->server_name)
}
,self
{
	"ircd", opts->origin
}
,conf
{
	std::make_unique<struct conf>(*opts)
}
,modules
{
	begin(matrix::module_names), end(matrix::module_names)
}
{
	if(ircd::mods::autoload)
		for(const auto &name : modules)
			mods::imports.emplace(std::string{name}, name);

	if(conf && !ircd::defaults)
		conf->load();

	_fetch = std::make_unique<fetch::init>();
	_vm = std::make_unique<vm::init>();
	const unwind_exceptional exceptional{[]
	{
		_fetch.reset(nullptr);
		_vm.reset(nullptr);
	}};

	const bool need_bootstrap
	{
		(sequence(*dbs::events) == 0 || opts->bootstrap_vector_path)
		&& dbs::events
	};

	if(need_bootstrap)
		bootstrap();

	// If the database is empty here there's nothing left to do; this is not
	// an error. When something tries to use this empty homeserver that is
	// where the error will be.
	if(sequence(*dbs::events) == 0)
		return;

	mods::imports.emplace("net_dns_cache"s, "net_dns_cache");

	if(!ircd::write_avoid)
		if(key && !key->verify_keys.empty())
			m::keys::cache::set(key->verify_keys);

	if(opts->autoapps)
		m::app::init();

	if(!ircd::maintenance)
		signon(*this);

	if(!ircd::maintenance && opts->backfill)
		m::init::backfill::init();
}
catch(const std::exception &e)
{
	log::logf
	{
		log, log::level::CRITICAL,
		"Failed to start server '%s' on network '%s' :%s",
		opts->server_name,
		opts->origin,
		e.what(),
	};
}

ircd::m::homeserver::~homeserver()
noexcept try
{
	///TODO: XXX primary
	server::init::interrupt();
	client::terminate_all();         //TODO: XXX
	server::init::close();
	client::close_all();
	m::init::backfill::fini();
	client::wait_all();
	server::init::wait();
	m::sync::pool.join();

	if(!ircd::maintenance && _vm)
		signoff(*this);

	///TODO: XXX primary
	mods::imports.erase("net_dns_cache"s);
	_fetch.reset(nullptr);
	_vm.reset(nullptr);
	m::app::fini();
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "Homeserver shutdown failed :%s",
		e.what()
	};

	return;
}

//
// homeserver modules
//

ircd::m::homeserver::modules::~modules()
noexcept
{
	for(auto rit(std::rbegin(*this)); rit != std::rend(*this); ++rit)
		mods::imports.erase(*rit);
}

//
// homeserver::key
//

namespace ircd::m
{
	extern conf::item<std::string> ed25519_key_dir;
	extern conf::item<milliseconds> ed25519_key_valid_for;
}

decltype(ircd::m::ed25519_key_dir)
ircd::m::ed25519_key_dir
{
	{ "name",     "ircd.keys.ed25519_key_dir"  },
	{ "default",  fs::cwd()                    },
};

decltype(ircd::m::ed25519_key_valid_for)
ircd::m::ed25519_key_valid_for
{
	{ "name",     "ircd.keys.ed25519_valid_for"     },
	{ "default",  1000 * 60 * 60 * 24 * 7 * 52L     },
};

ircd::m::homeserver::key::key(const struct opts &opts)
:secret_key_path{[&opts]
() -> std::string
{
	const std::string path_parts[]
	{
		std::string{ed25519_key_dir},
		std::string{opts.origin} + ".ed25519"s,
	};

	const std::string sk_file
	{
		ircd::string(fs::PATH_MAX_LEN, [&](const mutable_buffer &buf)
		{
			return fs::path(buf, path_parts);
		})
	};

	if(!fs::exists(sk_file) && !ircd::write_avoid)
		log::notice
		{
			m::log, "Creating ed25519 secret key @ `%s'", sk_file
		};

	return sk_file;
}()}
,secret_key
{
	secret_key_path, &public_key, !ircd::write_avoid
}
,public_key_b64
{
	ircd::string(96, [this](const mutable_buffer &buf)
	{
		return b64::encode_unpadded(buf, public_key);
	})
}
,public_key_id
{
	"ed25519:"s + std::string{trunc(public_key_b64, 8)}
}
,verify_keys{[this, &opts]
() -> std::string
{
	const json::strung verify_keys
	{
		json::members
		{
			{ public_key_id, json::member
			{
				"key", string_view{public_key_b64}
			}}
		}
	};

	const auto valid_until
	{
		ircd::now<system_point>() + milliseconds(ed25519_key_valid_for)
	};

	const auto valid_until_ts
	{
		duration_cast<milliseconds>(tse(valid_until))
	};

	m::keys key;
	json::get<"server_name"_>(key) = opts.origin;
	json::get<"old_verify_keys"_>(key) = "{}";
	json::get<"verify_keys"_>(key) = verify_keys;
	json::get<"valid_until_ts"_>(key) = valid_until_ts.count();
	json::strung ret
	{
		key
	};

	if(!secret_key)
		return std::move(ret);

	const ed25519::sig sig
	{
		secret_key.sign(const_buffer(ret))
	};

	char buf[2][512];
	const json::object sigs
	{
		json::stringify(mutable_buffer(buf[0]), json::members
		{
			{ opts.origin, json::member
			{
				public_key_id, b64::encode_unpadded(buf[1], sig)
			}}
		})
	};

	json::get<"signatures"_>(key) = sigs;
	ret = json::strung
	{
		key
	};

	return std::move(ret);
}()}
{
	if(!secret_key)
	{
		log::warning
		{
			m::log, "Cannot issue events originating from '%s' :No signing key available.",
			opts.origin,
		};

		return;
	}

	log::info
	{
		m::log, "Secret key for %s at `%s'. Public key is %s identified as '%s'",
		opts.origin,
		secret_key_path,
		public_key_b64,
		public_key_id,
	};
}

//
// homeserver::conf
//

namespace ircd::m
{
	static bool load_conf_item(const event &);
	static bool load_conf_item(const event::idx &);
	static size_t load_conf_items(const room &, const string_view &prefix);

	static void handle_conf_room_hook(const event &, vm::eval &);
	static void handle_item_init(const room &, conf::item<> &);
}

//
// homeserver::conf::conf
//

ircd::m::homeserver::conf::conf(const struct opts &opts)
:room_id
{
	"conf", opts.origin
}
,room
{
	room_id
}
,item_init
{
	ircd::conf::on_init, [this](ircd::conf::item<> &item)
	{
		handle_item_init(room, item);
	}
}
,conf_updated
{
	handle_conf_room_hook,
	{
		{ "_site",    "vm.effect"       },
		{ "room_id",  room_id           },
		{ "type",     "ircd.conf.item"  },
	}
}
{
	for(const auto &[key, item] : ircd::conf::items) try
	{
		assert(item);
		if(item->set_cb)
			item->set_cb();
	}
	catch(const std::exception &e)
	{
		log::error
		{
			"Failed to initialize conf item '%s' :%s",
			key,
			e.what()
		};

		throw;
	}
}

size_t
ircd::m::homeserver::conf::store(const string_view &prefix,
                                 const bool &force)
const
{
	size_t ret(0);
	for(const auto &[key, item] : ircd::conf::items) try
	{
		if(prefix && !startswith(key, prefix))
			continue;

		// Conf items marked with a persist=false property are not written
		// to the conf room; regardless of force=true
		if(!item->feature.get("persist", true))
			continue;

		thread_local char buf[4_KiB];
		const auto &val
		{
			item->get(buf)
		};

		bool dup{false}, exists{false};
		if(!force)
			get(key, [&exists, &dup, &val](const string_view &val_)
			{
				exists = true;
				dup = val == val_;
			});

		// No reason to store the same value
		if(!force && dup)
			continue;

		const json::string &default_value
		{
			item->feature["default"]
		};

		// When the !conf room has nothing for a key, and this store request
		// is asking us to write the default value, that is rejected here.
		if(!force && !exists && val == default_value)
			continue;

		set(key, val);
		ret++;
	}
	catch(const std::exception &e)
	{
		log::error
		{
			"Failed to create conf item '%s' :%s",
			key,
			e.what()
		};
	}

	return ret;
}

size_t
ircd::m::homeserver::conf::load(const string_view &prefix)
const
{
	return load_conf_items(room, prefix);
}

size_t
ircd::m::homeserver::conf::defaults(const string_view &prefix)
const
{
	size_t ret(0);
	for(const auto &[key, item] : ircd::conf::items)
	{
		if(prefix && !startswith(key, prefix))
			continue;

		assert(item);
		const json::string &default_value
		{
			item->feature["default"]
		};

		ret += ircd::conf::set(key, default_value);
	}

	return ret;
}

ircd::m::event::id::buf
ircd::m::homeserver::conf::set(const string_view &key,
                               const string_view &val)
const
{
	// Branch for conf items that do not persist. We don't send a message to
	// the conf room to update them; the value is put directly into the item.
	if(ircd::conf::exists(key) && !ircd::conf::persists(key))
	{
		ircd::conf::set(key, val);
		return {};
	}

	const m::user::id::buf sender
	{
		"ircd", room_id.hostname()
	};

	return send(room, sender, "ircd.conf.item", key, json::members
	{
		{ "value", val }
	});
}

bool
ircd::m::homeserver::conf::get(const string_view &key,
                               const std::function<void (const string_view &)> &closure)
const
{
	const auto &event_idx
	{
		room.get("ircd.conf.item", key)
	};

	return m::get(std::nothrow, event_idx, "content", [&closure]
	(const json::object &content)
	{
		const json::string &value
		{
			content["value"]
		};

		closure(value);
	});
}

void
ircd::m::handle_item_init(const room &room,
                          conf::item<> &item)
{
	const auto event_idx
	{
		room.get(std::nothrow, "ircd.conf.item", item.name)
	};

	if(!event_idx)
		return;

	load_conf_item(event_idx);
}

void
ircd::m::handle_conf_room_hook(const event &event,
                               vm::eval &eval)
{
	if(!homeserver::primary)
		return;

	assert(homeserver::primary->conf);
	const m::room::id &primary_room
	{
		homeserver::primary->conf->room_id
	};

	// Only the primary homeserver controls the global conf items.
	if(json::get<"room_id"_>(event) != primary_room)
		return;

	load_conf_item(event);
}

size_t
ircd::m::load_conf_items(const m::room &room,
                         const string_view &prefix)
{
	const m::room::state state
	{
		room
	};

	state.for_each("ircd.conf.item", [&prefix]
	(const auto &, const auto &state_key, const auto &event_idx)
	{
		static const m::event::fetch::opts fopts
		{
			m::event::keys::include { "content", "state_key" }
		};

		if(prefix && !startswith(state_key, prefix))
			return true;

		m::prefetch(event_idx, fopts);
		return true;
	});

	size_t ret(0);
	state.for_each("ircd.conf.item", [&ret, &prefix]
	(const auto &, const auto &state_key, const auto &event_idx)
	{
		if(prefix && !startswith(state_key, prefix))
			return true;

		if(!conf::exists(state_key))
			return true;

		ret += load_conf_item(event_idx);
		return true;
	});

	return ret;
}

bool
ircd::m::load_conf_item(const m::event::idx &event_idx)
{
	static const m::event::fetch::opts fopts
	{
		m::event::keys::include { "content", "state_key" }
	};

	const m::event::fetch event
	{
		std::nothrow, event_idx, fopts
	};

	return event.valid && load_conf_item(event);
}

bool
ircd::m::load_conf_item(const m::event &event)
try
{
	const auto &key
	{
		at<"state_key"_>(event)
	};

	const auto &content
	{
		at<"content"_>(event)
	};

	const json::string &value
	{
		content.get("value")
	};

	// Conf items marked with a persist=false property are not read from
	// the conf room into the item, even if the value exists in the room.
	if(conf::exists(key) && !conf::persists(key))
		return false;

	log::debug
	{
		"Updating conf [%s] => [%s]", key, value
	};

	ircd::conf::set(key, value);
	return true;
}
catch(const std::exception &e)
{
	log::error
	{
		"Failed to set conf item '%s' :%s",
		json::get<"state_key"_>(event),
		e.what()
	};

	return false;
}

//
// signon/signoff greetings
//

decltype(ircd::m::online_status_msg)
ircd::m::online_status_msg
{
	{ "name",     "ircd.me.online.status_msg"          },
	{ "default",  "Wanna chat? IRCd at your service!"  }
};

decltype(ircd::m::offline_status_msg)
ircd::m::offline_status_msg
{
	{ "name",     "ircd.me.offline.status_msg"     },
	{ "default",  "Catch ya on the flip side..."   }
};

void
ircd::m::signon(homeserver &homeserver)
{
	if(vm::sequence::retired != 0)
		presence::set(homeserver.self, "online", online_status_msg);
}

void
ircd::m::signoff(homeserver &homeserver)
noexcept try
{
	if(!std::uncaught_exceptions() && vm::sequence::retired != 0)
		presence::set(homeserver.self, "offline", offline_status_msg);
}
catch(const std::exception &e)
{
	log::error
	{
		log, "homeserver signoff failure :%s",
		e.what(),
	};
}
catch(...)
{
	log::critical
	{
		log, "Unknown error during homeserver signoff"
	};
}

///////////////////////////////////////////////////////////////////////////////
//
// m/self.h
//
//
// !!! DEPRECATED !!!
//
// These items are being replaced, but their widespread use throughout the
// codebase is keeping them here for now.
//

namespace ircd::m::self
{
	static bool match(const net::hostport &a, const net::hostport &b) noexcept;
}

/// Get network name (origin) of the primary homeserver. Use of this function
/// is discouraged, though it's not marked as deprecated to reduce warnings
/// for now until an actual effort is made to eliminate all callsites. Instead
/// of using this function, try to obtain a more specific homeserver instance
/// being hosted from this server based on the context of the callsite.
ircd::string_view
ircd::m::self::my_host()
{
	return origin(my());
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

/// Determine if argument string is one of my homeserver's network names. This
/// is not a simple string comparison; strings postfixed with port :8448 are
/// compared equal to strings without a port.
bool
ircd::m::self::host(const string_view &other)
{
	assert(m::canon_port == 8448);
	const net::hostport other_host{other};
	for(const auto &[my_network, hs_p] : homeserver::map)
		if(match(my_network, other))
			return true;

	return false;
}

bool
ircd::m::self::match(const net::hostport &a,
                     const net::hostport &b)
noexcept
{
	// port() is 0 when the origin has no port (and implies 8448)
	const auto my_port
	{
		port(a)?: m::canon_port
	};

	// If my_host has a non-canonical port number, then the argument must
	// also have the same port number, or there is no possible match.
	if(my_port != m::canon_port)
		return my_port == port(b) && host(a) == host(b);

	// Since my host is on the canonical port, if other host has some
	// different port number, there is no possible match.
	if(port(b) && port(b) != m::canon_port)
		return false;

	// Both myself and input are using 8448; now the name has to match.
	return host(a) == host(b);
}
