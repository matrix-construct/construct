// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::log)
ircd::m::log
{
	"matrix", 'm'
};

//
// init
//

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

//
// init::init
//

ircd::m::init::init(const string_view &origin,
                    const string_view &servername)
try
:_self
{
	origin, servername
}
,_modules
{
	std::make_unique<modules>()
}
{
	if(!ircd::write_avoid)
		presence::set(me, "online", me_online_status_msg);

	if(room::state::disable_history)
		log::warning
		{
			m::log, "Room state history is disabled by the configuration"
			" (ircd.m.room.state.disable_history). This is for development"
			" only. You must change this manually when instructed to do so"
			" by the developer before production use."
		};
}
catch(const http::error &e)
{
	log::critical
	{
		log, "Failed to start matrix :%s %s", e.what(), e.content
	};

	throw;
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "Failed to start matrix :%s", e.what()
	};

	throw;
}

ircd::m::init::~init()
noexcept try
{
	m::sync::pool.join();

	if(!std::uncaught_exceptions() && !ircd::write_avoid)
		presence::set(me, "offline", me_offline_status_msg);
}
catch(const m::error &e)
{
	log::critical
	{
		log, "%s %s", e.what(), e.content
	};

	ircd::terminate();
}

void
ircd::m::init::close()
{
	mods::imports.erase("s_listen"s);
}

ircd::m::init::modules::modules()
try
{
	init_keys();
	init_imports();
}
catch(const m::error &e)
{
	const std::string what(e.what());
	const std::string content(e.content);
	const ctx::exception_handler eh;
	log::critical
	{
		log, "%s %s", what, content
	};

	mods::imports.clear();
	throw m::error
	{
		"M_INIT_ERROR", "Failed to start :%s :%s", what, content
	};
}
catch(const std::exception &e)
{
	const std::string what(e.what());
	const ctx::exception_handler eh;
	log::critical
	{
		log, "%s", what
	};

	mods::imports.clear();
	throw m::error
	{
		"M_INIT_ERROR", "Failed to start :%s", what
	};
}
catch(const ctx::terminated &)
{
	const ctx::exception_handler eh;
	mods::imports.clear();
	throw ctx::terminated{};
}

void
ircd::m::init::modules::init_keys()
{
	mods::imports.emplace("s_keys"s, "s_keys"s);
	mods::import<void ()> init_my_keys
	{
		"s_keys", "init_my_keys"
	};

	init_my_keys();
}

void
ircd::m::init::modules::init_imports()
{
	if(!bool(ircd::mods::autoload))
	{
		log::warning
		{
			"Not loading modules because noautomod flag is set. "
			"You may still load modules manually."
		};

		return;
	}

	// Manually load first modules
	mods::imports.emplace("vm"s, "vm"s);

	// The order of these prefixes will be the loading order. Order of
	// specific modules within a prefix is not determined here.
	static const string_view prefixes[]
	{
		"s_", "m_", "key_", "media_", "client_", "federation_"
	};

	// Load modules by prefix.
	for(const auto &prefix : prefixes)
		for(const auto &name : mods::available())
			if(startswith(name, prefix))
				mods::imports.emplace(name, name);

	// Manually load last modules
	mods::imports.emplace("well_known"s, "well_known"s);
	mods::imports.emplace("webroot"s, "webroot"s);

	if(db::sequence(*dbs::events) == 0)
		bootstrap();
}

ircd::m::init::modules::~modules()
noexcept
{
	mods::imports.clear();
}

void
ircd::m::init::bootstrap()
{
	assert(dbs::events);
	assert(db::sequence(*dbs::events) == 0);

	log::notice
	{
		log, "This appears to be your first time running IRCd because the events "
		"database is empty. I will be bootstrapping it with initial events now..."
	};

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

///////////////////////////////////////////////////////////////////////////////
//
// m/self.h
//

std::string
ircd::m::self::origin
{};

std::string
ircd::m::self::servername
{};

ircd::ed25519::sk
ircd::m::self::secret_key
{};

ircd::ed25519::pk
ircd::m::self::public_key
{};

std::string
ircd::m::self::public_key_b64
{};

std::string
ircd::m::self::public_key_id
{};

std::string
ircd::m::self::tls_cert_der
{};

std::string
ircd::m::self::tls_cert_der_sha256_b64
{};

//
// my user
//

ircd::m::user::id::buf
ircd_user_id
{
	"ircd", "localhost"  // gets replaced after conf init
};

ircd::m::user
ircd::m::me
{
	ircd_user_id
};

//
// my room
//

ircd::m::room::id::buf
ircd_room_id
{
	"ircd", "localhost" // replaced after conf init
};

ircd::m::room
ircd::m::my_room
{
	ircd_room_id
};

//
// my node
//

ircd::m::node::id::buf
ircd_node_id
{
	ircd::m::node::id::origin, "localhost" // replaced after conf init
};

ircd::m::node
ircd::m::my_node
{
	ircd_node_id
};

bool
ircd::m::self::host(const string_view &other)
{
	// port() is 0 when the origin has no port (and implies 8448)
	const auto port
	{
		me.user_id.port()
	};

	// If my_host has a port number, then the argument must also have the
	// same port number.
	if(port)
		return host() == other;

	/// If my_host has no port number, then the argument can have port
	/// 8448 or no port number, which will initialize net::hostport.port to
	/// the "canon_port" of 8448.
	assert(net::canon_port == 8448);
	const net::hostport _other{other};
	if(net::port(_other) != net::canon_port)
		return false;

	if(host() != host(_other))
		return false;

	return true;
}

ircd::string_view
ircd::m::self::host()
{
	return me.user_id.host();
}

//
// init
//

//TODO: XXX
extern ircd::m::room::id::buf users_room_id;
extern ircd::m::room::id::buf tokens_room_id;
extern ircd::m::room::id::buf nodes_room_id;

ircd::m::self::init::init(const string_view &origin,
                          const string_view &servername)
{
	self::origin = origin;
	self::servername = servername;

	ircd_user_id = {"ircd", origin};
	m::me = {ircd_user_id};

	ircd_room_id = {"ircd", origin};
	m::my_room = {ircd_room_id};

	ircd_node_id = {node::id::origin, origin};
	m::my_node = {ircd_node_id};

	users_room_id = {"users", origin};
	m::user::users = {users_room_id};

	tokens_room_id = {"tokens", origin};
	m::user::tokens = {tokens_room_id};

	nodes_room_id = {"nodes", origin};
	m::nodes = {nodes_room_id};

	if(origin == "localhost")
		log::warning
		{
			"The origin is configured or has defaulted to 'localhost'"
		};
}

///////////////////////////////////////////////////////////////////////////////
//
// m/fetch.h
//

decltype(ircd::m::fetch::log)
ircd::m::fetch::log
{
	"matrix.fetch"
};

void
ircd::m::fetch::headfill(const room &r)
{
	using prototype = void (const room &);

	static mods::import<prototype> call
	{
		"s_fetch", "ircd::m::fetch::headfill"
	};

	call(r);
}

void
ircd::m::fetch::backfill(const room &r)
{
	using prototype = void (const room &);

	static mods::import<prototype> call
	{
		"s_fetch", "ircd::m::fetch::backfill"
	};

	call(r);
}

void
ircd::m::fetch::backfill(const room &r,
                         const net::hostport &hp)
{
	using prototype = void (const room &, const net::hostport &);

	static mods::import<prototype> call
	{
		"s_fetch", "ircd::m::fetch::backfill"
	};

	call(r, hp);
}

void
ircd::m::fetch::frontfill(const room &r)
{
	using prototype = void (const room &);

	static mods::import<prototype> call
	{
		"s_fetch", "ircd::m::fetch::frontfill"
	};

	call(r);
}

void
ircd::m::fetch::frontfill(const room &r,
                         const net::hostport &hp)
{
	using prototype = void (const room &, const net::hostport &);

	static mods::import<prototype> call
	{
		"s_fetch", "ircd::m::fetch::frontfill"
	};

	call(r, hp);
}

void
ircd::m::fetch::state_ids(const room &r)
{
	using prototype = void (const room &);

	static mods::import<prototype> call
	{
		"s_fetch", "ircd::m::fetch::state_ids"
	};

	call(r);
}

void
ircd::m::fetch::state_ids(const room &r,
                          const net::hostport &hp)
{
	using prototype = void (const room &, const net::hostport &);

	static mods::import<prototype> call
	{
		"s_fetch", "ircd::m::fetch::state_ids"
	};

	call(r, hp);
}

void
ircd::m::fetch::auth_chain(const room &r,
                           const net::hostport &hp)
{
	using prototype = void (const room &, const net::hostport &);

	static mods::import<prototype> call
	{
		"s_fetch", "ircd::m::fetch::auth_chain"
	};

	call(r, hp);
}

bool
ircd::m::fetch::prefetch(const m::room::id &room_id,
                         const m::event::id &event_id)
{
	using prototype = bool (const m::room::id &, const m::event::id &);

	static mods::import<prototype> call
	{
		"s_fetch", "ircd::m::fetch::prefetch"
	};

	return call(room_id, event_id);
}

bool
ircd::m::fetch::start(const m::room::id &room_id,
                      const m::event::id &event_id)
{
	using prototype = bool (const m::room::id &, const m::event::id &);

	static mods::import<prototype> call
	{
		"s_fetch", "ircd::m::fetch::start"
	};

	return call(room_id, event_id);
}

bool
ircd::m::fetch::cancel(request &r)
{
	using prototype = bool (request &);

	static mods::import<prototype> call
	{
		"s_fetch", "ircd::m::fetch::cancel"
	};

	return call(r);
}

bool
ircd::m::fetch::exists(const m::event::id &event_id)
{
	using prototype = bool (const m::event::id &);

	static mods::import<prototype> call
	{
		"s_fetch", "ircd::m::fetch::exists"
	};

	return call(event_id);
}

bool
ircd::m::fetch::for_each(const std::function<bool (request &)> &closure)
{
	using prototype = bool (const std::function<bool (request &)> &);

	static mods::import<prototype> call
	{
		"s_fetch", "ircd::m::fetch::for_each"
	};

	return call(closure);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/sync.h
//

decltype(ircd::m::sync::log)
ircd::m::sync::log
{
	"matrix.sync", 's'
};

namespace ircd::m::sync
{
	const ctx::pool::opts pool_opts
	{
		ctx::DEFAULT_STACK_SIZE,
		0,
		-1,
		0
	};
}

decltype(ircd::m::sync::pool)
ircd::m::sync::pool
{
	"sync", pool_opts
};

decltype(ircd::m::sync::stats_info)
ircd::m::sync::stats_info
{
	{ "name",     "ircd.m.sync.stats.info" },
	{ "default",  false                    },
};

bool
ircd::m::sync::for_each(const item_closure_bool &closure)
{
	auto it(begin(item::map));
	for(; it != end(item::map); ++it)
		if(!closure(*it->second))
			return false;

	return true;
}

bool
ircd::m::sync::for_each(const string_view &prefix,
                        const item_closure_bool &closure)
{
	const auto depth
	{
		token_count(prefix, '.')
	};

	auto it
	{
		item::map.lower_bound(prefix)
	};

	for(; it != end(item::map); ++it)
	{
		const auto item_depth
		{
			token_count(it->first, '.')
		};

		if(item_depth > depth + 1)
			continue;

		if(it->first == prefix)
			continue;

		if(item_depth < depth + 1)
			break;

		if(!closure(*it->second))
			return false;
	}

	return true;
}

bool
ircd::m::sync::apropos(const data &d,
                       const event &event)
{
	return apropos(d, index(event, std::nothrow));
}

bool
ircd::m::sync::apropos(const data &d,
                       const event::id &event_id)
{
	return apropos(d, index(event_id, std::nothrow));
}

bool
ircd::m::sync::apropos(const data &d,
                       const event::idx &event_idx)
{
	return event_idx >= d.range.first &&
	       event_idx < d.range.second;
}

ircd::string_view
ircd::m::sync::loghead(const data &data)
{
	thread_local char headbuf[256], rembuf[128], iecbuf[2][64], tmbuf[32];

	const auto remstr
	{
		data.client?
			string(rembuf, ircd::remote(*data.client)):
			string_view{}
	};

	const auto flush_bytes
	{
		data.stats?
			data.stats->flush_bytes:
			0U
	};

	const auto flush_count
	{
		data.stats?
			data.stats->flush_count:
			0U
	};

	const auto tmstr
	{
		data.stats?
			ircd::pretty(tmbuf, data.stats->timer.at<milliseconds>(), true):
			string_view{}
	};

	return fmt::sprintf
	{
		headbuf, "%s %s %ld:%lu|%lu chunk:%zu sent:%s of %s in %s",
		remstr,
		string_view{data.user.user_id},
		data.range.first,
		data.range.second,
		vm::sequence::retired,
		flush_count,
		ircd::pretty(iecbuf[1], iec(flush_bytes)),
		data.out?
			ircd::pretty(iecbuf[0], iec(flush_bytes + size(data.out->completed()))):
			string_view{},
		tmstr
	};
}

//
// data
//

ircd::m::sync::data::data
(
	const m::user &user,
	const m::events::range &range,
	ircd::client *const &client,
	json::stack *const &out,
	sync::stats *const &stats,
	const string_view &filter_id
)
:range
{
	range
}
,stats
{
	stats
}
,client
{
	client
}
,user
{
	user
}
,user_room
{
	user
}
,user_state
{
	user_room
}
,user_rooms
{
	user
}
,filter_buf
{
	m::filter::get(filter_id, user)
}
,filter
{
	json::object{filter_buf}
}
,out
{
	out
}
{
}

ircd::m::sync::data::~data()
noexcept
{
}

//
// item
//

template<>
decltype(ircd::m::sync::item::instance_multimap::map)
ircd::m::sync::item::instance_multimap::map
{};

//
// item::item
//

ircd::m::sync::item::item(std::string name,
                          handle polylog,
                          handle linear,
                          const json::members &feature)
:instance_multimap
{
	std::move(name)
}
,conf_name
{
	fmt::snstringf{128, "ircd.m.sync.%s.enable", this->name()},
	fmt::snstringf{128, "ircd.m.sync.%s.stats.debug", this->name()},
}
,enable
{
	{ "name",     conf_name[0] },
	{ "default",  true         },
}
,stats_debug
{
	{ "name",     conf_name[1] },
	{ "default",  false        },
}
,_polylog
{
	std::move(polylog)
}
,_linear
{
	std::move(linear)
}
,feature
{
	feature
}
,opts
{
	this->feature
}
,phased
{
	opts.get<bool>("phased", false)
}
,initial
{
	opts.get<bool>("initial", false)
}
{
	log::debug
	{
		log, "Registered sync item(%p) '%s' (%zu features)",
		this,
		this->name(),
		opts.size(),
	};
}

ircd::m::sync::item::~item()
noexcept
{
	log::debug
	{
		log, "Unregistered sync item(%p) '%s'",
		this,
		this->name()
	};
}

bool
ircd::m::sync::item::polylog(data &data)
try
{
	// Skip the item if disabled by configuration
	if(!enable)
		return false;

	// Skip the item for phased-sync ranges if it's not phased-sync aware.
	if(data.phased && !phased && !initial)
		return false;

	// Skip the item for phased-sync ranges after initial sync if it has initial=true
	if(data.phased && initial && int64_t(data.range.first) < 0L)
		return false;

	#ifdef RB_DEBUG
	sync::stats stats
	{
		data.stats && (stats_info || stats_debug)?
			*data.stats:
			sync::stats{}
	};

	if(data.stats && (stats_info || stats_debug))
		stats.timer = {};
	#endif

	const bool ret
	{
		_polylog(data)
	};

	#ifdef RB_DEBUG
	if(data.stats && (stats_info || stats_debug))
	{
		//data.out.flush();
		thread_local char tmbuf[32];
		log::debug
		{
			log, "polylog %s commit:%b '%s' %s",
			loghead(data),
			ret,
			name(),
			ircd::pretty(tmbuf, stats.timer.at<microseconds>(), true)
		};
	}
	#endif

	return ret;
}
catch(const std::bad_function_call &e)
{
	log::dwarning
	{
		log, "polylog %s '%s' missing handler :%s",
		loghead(data),
		name(),
		e.what()
	};

	return false;
}
catch(const m::error &e)
{
	log::derror
	{
		log, "polylog %s '%s' :%s %s",
		loghead(data),
		name(),
		e.what(),
		e.content
	};

	return false;
}
catch(const std::exception &e)
{
	log::derror
	{
		log, "polylog %s '%s' :%s",
		loghead(data),
		name(),
		e.what()
	};

	throw;
}

bool
ircd::m::sync::item::linear(data &data)
try
{
	if(!enable)
		return false;

	return _linear(data);
}
catch(const std::bad_function_call &e)
{
	thread_local char rembuf[128];
	log::dwarning
	{
		log, "linear %s '%s' missing handler :%s",
		loghead(data),
		name(),
		e.what()
	};

	return false;
}
catch(const m::error &e)
{
	log::derror
	{
		log, "linear %s '%s' :%s %s",
		loghead(data),
		name(),
		e.what(),
		e.content
	};

	return false;
}
catch(const std::exception &e)
{
	log::derror
	{
		log, "linear %s '%s' :%s",
		loghead(data),
		name(),
		e.what()
	};

	throw;
}

bool
ircd::m::sync::item::poll(data &data,
                          const m::event &event)
try
{
	const scope_restore theirs
	{
		data.event, &event
	};

	return _linear(data);
}
catch(const std::bad_function_call &e)
{
	thread_local char rembuf[128];
	log::dwarning
	{
		log, "poll %s '%s' missing handler :%s",
		loghead(data),
		name(),
		e.what()
	};

	return false;
}

ircd::string_view
ircd::m::sync::item::member_name()
const
{
	return token_last(name(), '.');
}

ircd::string_view
ircd::m::sync::item::name()
const
{
	return this->instance_multimap::it->first;
}

///////////////////////////////////////////////////////////////////////////////
//
// m/app.h
//

decltype(ircd::m::app::log)
ircd::m::app::log
{
	"matrix.app"
};

std::string
ircd::m::app::config::get(const string_view &id)
{
	using prototype = std::string (const string_view &);

	static mods::import<prototype> call
	{
		"app_app", "ircd::m::app::config::get"
	};

	return call(id);
}

std::string
ircd::m::app::config::get(std::nothrow_t,
                       const string_view &id)
{
	using prototype = std::string (std::nothrow_t, const string_view &);

	static mods::import<prototype> call
	{
		"app_app", "ircd::m::app::config::get"
	};

	return call(std::nothrow, id);
}

void
ircd::m::app::config::get(const string_view &id,
                       const event::fetch::view_closure &closure)
{
	using prototype = void (const string_view &, const event::fetch::view_closure &);

	static mods::import<prototype> call
	{
		"app_app", "ircd::m::app::config::get"
	};

	return call(id, closure);
}

bool
ircd::m::app::config::get(std::nothrow_t,
                          const string_view &id,
                          const event::fetch::view_closure &closure)
{
	using prototype = bool (std::nothrow_t, const string_view &, const event::fetch::view_closure &);

	static mods::import<prototype> call
	{
		"app_app", "ircd::m::app::config::get"
	};

	return call(std::nothrow, id, closure);
}

ircd::m::event::idx
ircd::m::app::config::idx(const string_view &id)
{
	using prototype = event::idx (const string_view &);

	static mods::import<prototype> call
	{
		"app_app", "ircd::m::app::config::idx"
	};

	return call(id);
}

ircd::m::event::idx
ircd::m::app::config::idx(std::nothrow_t,
                          const string_view &id)
{
	using prototype = event::idx (std::nothrow_t, const string_view &);

	static mods::import<prototype> call
	{
		"app_app", "ircd::m::app::config::idx"
	};

	return call(std::nothrow, id);
}

bool
ircd::m::app::exists(const string_view &id)
{
	using prototype = bool (const string_view &);

	static mods::import<prototype> call
	{
		"app_app", "exists"
	};

	return call(id);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/feds.h
//

ircd::m::feds::acquire::acquire(const opts &o,
                                const closure &c)
:acquire
{
	vector_view<const opts>{&o, 1}, c
}
{
}

ircd::m::feds::acquire::acquire(const vector_view<const opts> &o,
                                const closure &c)
{
	using prototype = bool (const vector_view<const opts> &, const closure &);

	static mods::import<prototype> call
	{
		"federation_federation", "ircd::m::feds::execute"
	};

	call(o, c);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/vm.h
//

decltype(ircd::m::vm::default_opts)
ircd::m::vm::default_opts;

decltype(ircd::m::vm::default_copts)
ircd::m::vm::default_copts;

decltype(ircd::m::vm::log)
ircd::m::vm::log
{
	"matrix.vm", 'v'
};

decltype(ircd::m::vm::dock)
ircd::m::vm::dock;

decltype(ircd::m::vm::ready)
ircd::m::vm::ready;

ircd::string_view
ircd::m::vm::loghead(const eval &eval)
{
	thread_local char buf[128];
	return loghead(buf, eval);
}

ircd::string_view
ircd::m::vm::loghead(const mutable_buffer &buf,
                     const eval &eval)
{
	return fmt::sprintf
	{
		buf, "vm:%lu:%lu:%lu eval:%lu seq:%lu share:%lu:%lu] %s",
		sequence::uncommitted,
		sequence::committed,
		sequence::retired,
		eval.id,
		sequence::get(eval),
		eval.sequence_shared[0],
		eval.sequence_shared[1],
		eval.event_?
			string_view{json::get<"event_id"_>(*eval.event_)}:
			"<unidentified>"_sv,
	};
}

ircd::http::code
ircd::m::vm::http_code(const fault &code)
{
	switch(code)
	{
		case fault::ACCEPT:       return http::OK;
		case fault::EXISTS:       return http::CONFLICT;
		case fault::INVALID:      return http::BAD_REQUEST;
		case fault::GENERAL:      return http::FORBIDDEN;
		case fault::STATE:        return http::NOT_FOUND;
		default:                  return http::INTERNAL_SERVER_ERROR;
	}
}

ircd::string_view
ircd::m::vm::reflect(const enum fault &code)
{
	switch(code)
	{
		case fault::ACCEPT:       return "ACCEPT";
		case fault::EXISTS:       return "EXISTS";
		case fault::INVALID:      return "INVALID";
		case fault::GENERAL:      return "GENERAL";
		case fault::EVENT:        return "EVENT";
		case fault::STATE:        return "STATE";
		case fault::INTERRUPT:    return "INTERRUPT";
	}

	return "??????";
}

//
// Eval
//
// Processes any event from any place from any time and does whatever is
// necessary to validate, reject, learn from new information, ignore old
// information and advance the state of IRCd as best as possible.

/// Instance list linkage for all of the evaluations.
template<>
decltype(ircd::util::instance_list<ircd::m::vm::eval>::allocator)
ircd::util::instance_list<ircd::m::vm::eval>::allocator
{};

template<>
decltype(ircd::util::instance_list<ircd::m::vm::eval>::list)
ircd::util::instance_list<ircd::m::vm::eval>::list
{
	allocator
};

decltype(ircd::m::vm::eval::id_ctr)
ircd::m::vm::eval::id_ctr;

decltype(ircd::m::vm::eval::executing)
ircd::m::vm::eval::executing;

decltype(ircd::m::vm::eval::injecting)
ircd::m::vm::eval::injecting;

decltype(ircd::m::vm::eval::injecting_room)
ircd::m::vm::eval::injecting_room;

void
ircd::m::vm::eval::seqsort()
{
	eval::list.sort([]
	(const auto *const &a, const auto *const &b)
	{
		if(sequence::get(*a) == 0)
			return false;

		if(sequence::get(*b) == 0)
			return true;

		return sequence::get(*a) < sequence::get(*b);
	});
}

ircd::m::vm::eval *
ircd::m::vm::eval::seqmin()
{
	const auto it
	{
		std::min_element(begin(eval::list), end(eval::list), []
		(const auto *const &a, const auto *const &b)
		{
			if(sequence::get(*a) == 0)
				return false;

			if(sequence::get(*b) == 0)
				return true;

			return sequence::get(*a) < sequence::get(*b);
		})
	};

	if(it == end(eval::list))
		return nullptr;

	if(sequence::get(**it) == 0)
		return nullptr;

	return *it;
}

ircd::m::vm::eval *
ircd::m::vm::eval::seqmax()
{
	const auto it
	{
		std::max_element(begin(eval::list), end(eval::list), []
		(const auto *const &a, const auto *const &b)
		{
			return sequence::get(*a) < sequence::get(*b);
		})
	};

	if(it == end(eval::list))
		return nullptr;

	if(sequence::get(**it) == 0)
		return nullptr;

	return *it;
}

ircd::m::vm::eval *
ircd::m::vm::eval::seqnext(const uint64_t &seq)
{
	eval *ret{nullptr};
	for(auto *const &eval : eval::list)
	{
		if(sequence::get(*eval) <= seq)
			continue;

		if(!ret || sequence::get(*eval) < sequence::get(*ret))
			ret = eval;
	}

	assert(!ret || sequence::get(*ret) > seq);
	return ret;
}

bool
ircd::m::vm::eval::sequnique(const uint64_t &seq)
{
	return 1 == std::count_if(begin(eval::list), end(eval::list), [&seq]
	(const auto *const &eval)
	{
		return sequence::get(*eval) == seq;
	});
}

ircd::m::vm::eval &
ircd::m::vm::eval::get(const event::id &event_id)
{
	auto *const ret
	{
		find(event_id)
	};

	if(unlikely(!ret))
		throw std::out_of_range
		{
			"eval::get(): event_id not being evaluated."
		};

	return *ret;
}

ircd::m::vm::eval *
ircd::m::vm::eval::find(const event::id &event_id)
{
	eval *ret{nullptr};
	for_each([&event_id, &ret](eval &e)
	{
		if(e.event_)
		{
			if(json::get<"event_id"_>(*e.event_) == event_id)
				ret = &e;
		}
		else if(e.issue)
		{
			if(e.issue->has("event_id"))
				if(string_view{e.issue->at("event_id")} == event_id)
					ret = &e;
		}
		else if(e.event_id == event_id)
			ret = &e;

		return ret == nullptr;
	});

	return ret;
}

size_t
ircd::m::vm::eval::count(const ctx::ctx *const &c)
{
	return std::count_if(begin(eval::list), end(eval::list), [&c]
	(const eval *const &eval)
	{
		return eval->ctx == c;
	});
}

bool
ircd::m::vm::eval::for_each(const std::function<bool (eval &)> &closure)
{
	for(eval *const &eval : eval::list)
		if(!closure(*eval))
			return false;

	return true;
}

bool
ircd::m::vm::eval::for_each(const ctx::ctx *const &c,
                            const std::function<bool (eval &)> &closure)
{
	for(eval *const &eval : eval::list)
		if(eval->ctx == c)
			if(!closure(*eval))
				return false;

	return true;
}

//
// eval::eval
//

ircd::m::vm::eval::eval(const room &room,
                        json::iov &event,
                        const json::iov &content)
:eval{}
{
	operator()(room, event, content);
}

ircd::m::vm::eval::eval(json::iov &event,
                        const json::iov &content,
                        const vm::copts &opts)
:eval{opts}
{
	operator()(event, content);
}

ircd::m::vm::eval::eval(const event &event,
                        const vm::opts &opts)
:eval{opts}
{
	operator()(event);
}

ircd::m::vm::eval::eval(const json::array &event,
                        const vm::opts &opts)
:opts{&opts}
,pdus{event}
{
	for(const json::object &pdu : this->pdus)
		operator()(pdu);
}

ircd::m::vm::eval::eval(const vm::copts &opts)
:opts{&opts}
,copts{&opts}
{
}

ircd::m::vm::eval::eval(const vm::opts &opts)
:opts{&opts}
{
}

ircd::m::vm::eval::~eval()
noexcept
{
}

ircd::m::vm::eval::operator
const event::id::buf &()
const
{
	return event_id;
}

bool
ircd::m::vm::eval::for_each_pdu(const std::function<bool (const json::object &)> &closure)
{
	return for_each([&closure](eval &e)
	{
		for(const json::object &pdu : e.pdus)
			if(!closure(pdu))
				return false;

		return true;
	});
}

///
/// Figure 1:
///          in     .  <-- injection
///    ===:::::::==//
///    |  ||||||| //   <-- these functions
///    |   \\|// //|
///    |    ||| // |   |  acceleration
///    |    |||//  |   |
///    |    |||/   |   |
///    |    |||    |   V
///    |    !!!    |
///    |     *     |   <----- nozzle
///    | ///|||\\\ |
///    |/|/|/|\|\|\|   <---- propagation cone
///  _/|/|/|/|\|\|\|\_
///         out
///

/// Inject a new event in a room originating from this server.
///
enum ircd::m::vm::fault
ircd::m::vm::eval::operator()(const room &room,
                              json::iov &event,
                              const json::iov &contents)
{
	using prototype = fault (eval &, const m::room &, json::iov &, const json::iov &);

	static mods::import<prototype> call
	{
		"vm", "ircd::m::vm::inject"
	};

	vm::dock.wait([]
	{
		return vm::ready;
	});

	return call(*this, room, event, contents);
}

/// Inject a new event originating from this server.
///
enum ircd::m::vm::fault
ircd::m::vm::eval::operator()(json::iov &event,
                              const json::iov &contents)
{
	using prototype = fault (eval &, json::iov &, const json::iov &);

	static mods::import<prototype> call
	{
		"vm", "ircd::m::vm::inject"
	};

	vm::dock.wait([]
	{
		return vm::ready;
	});

	return call(*this, event, contents);
}

enum ircd::m::vm::fault
ircd::m::vm::eval::operator()(const event &event)
{
	using prototype = fault (eval &, const m::event &);

	static mods::import<prototype> call
	{
		"vm", "ircd::m::vm::execute"
	};

	vm::dock.wait([]
	{
		return vm::ready;
	});

	return call(*this, event);
}

//
// sequence
//

decltype(ircd::m::vm::sequence::dock)
ircd::m::vm::sequence::dock;

decltype(ircd::m::vm::sequence::retired)
ircd::m::vm::sequence::retired;

decltype(ircd::m::vm::sequence::committed)
ircd::m::vm::sequence::committed;

decltype(ircd::m::vm::sequence::uncommitted)
ircd::m::vm::sequence::uncommitted;

uint64_t
ircd::m::vm::sequence::min()
{
	const auto *const e
	{
		eval::seqmin()
	};

	return e? get(*e) : 0;
}

uint64_t
ircd::m::vm::sequence::max()
{
	const auto *const e
	{
		eval::seqmax()
	};

	return e? get(*e) : 0;
}

uint64_t
ircd::m::vm::sequence::get(id::event::buf &event_id)
{
	static constexpr auto column_idx
	{
		json::indexof<event, "event_id"_>()
	};

	auto &column
	{
		dbs::event_column.at(column_idx)
	};

	const auto it
	{
		column.rbegin()
	};

	if(!it)
	{
		// If this iterator is invalid the events db should
		// be completely fresh.
		assert(db::sequence(*dbs::events) == 0);
		return 0;
	}

	const auto &ret
	{
		byte_view<uint64_t>(it->first)
	};

	event_id = it->second;
	return ret;
}

const uint64_t &
ircd::m::vm::sequence::get(const eval &eval)
{
	return eval.sequence;
}


///////////////////////////////////////////////////////////////////////////////
//
// m/keys.h
//

bool
ircd::m::verify(const m::keys &keys)
{
	using prototype = bool (const m::keys &) noexcept;

	static mods::import<prototype> function
	{
		"s_keys", "verify__keys"
	};

	return function(keys);
}

//
// keys
//

void
ircd::m::keys::get(const string_view &server_name,
                   const closure &closure)
{
	return get(server_name, "", closure);
}

void
ircd::m::keys::get(const string_view &server_name,
                   const string_view &key_id,
                   const closure &closure_)
{
	using prototype = void (const string_view &, const string_view &, const closure &);

	static mods::import<prototype> function
	{
		"s_keys", "get__keys"
	};

	return function(server_name, key_id, closure_);
}

bool
ircd::m::keys::query(const string_view &query_server,
                     const queries &queries_,
                     const closure_bool &closure)
{
	using prototype = bool (const string_view &, const queries &, const closure_bool &);

	static mods::import<prototype> function
	{
		"s_keys", "query__keys"
	};

	return function(query_server, queries_, closure);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/visible.h
//

bool
ircd::m::visible(const event::id &event_id,
                 const string_view &mxid)
{
	m::room::id::buf room_id
	{
		get(event_id, "room_id", room_id)
	};

	const m::event event
	{
		{ "event_id",  event_id  },
		{ "room_id",   room_id   }
	};

	return visible(event, mxid);
}

bool
ircd::m::visible(const event &event,
                 const string_view &mxid)
{
	using prototype = bool (const m::event &, const string_view &);

	static mods::import<prototype> call
	{
		"m_room_history_visibility", "ircd::m::visible"
	};

	return call(event, mxid);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/receipt.h
//

ircd::m::event::id::buf
ircd::m::receipt::read(const id::room &room_id,
                       const id::user &user_id,
                       const id::event &event_id)
{
	return read(room_id, user_id, event_id, ircd::time<milliseconds>());
}

ircd::m::event::id::buf
ircd::m::receipt::read(const room::id &room_id,
                       const user::id &user_id,
                       const event::id &event_id,
                       const time_t &ms)
{
	using prototype = event::id::buf (const room::id &, const user::id &, const event::id &, const time_t &);

	static mods::import<prototype> function
	{
		"m_receipt", "ircd::m::receipt::read"
	};

	return function(room_id, user_id, event_id, ms);
}

ircd::m::event::id
ircd::m::receipt::read(event::id::buf &out,
                       const room::id &room_id,
                       const user::id &user_id)
{
	const event::id::closure copy{[&out]
	(const event::id &event_id)
	{
		out = event_id;
	}};

	return read(room_id, user_id, copy)?
		event::id{out}:
		event::id{};
}

bool
ircd::m::receipt::read(const room::id &room_id,
                       const user::id &user_id,
                       const event::id::closure &closure)
{
	using prototype = bool (const room::id &, const user::id &, const event::id::closure &);

	static mods::import<prototype> function
	{
		"m_receipt", "ircd::m::receipt::read"
	};

	return function(room_id, user_id, closure);
}

bool
ircd::m::receipt::ignoring(const user &user,
                           const room::id &room_id)
{
	using prototype = bool (const m::user &, const m::room::id &);

	static mods::import<prototype> function
	{
		"m_receipt", "ircd::m::receipt::ignoring"
	};

	return function(user, room_id);
}

bool
ircd::m::receipt::ignoring(const user &user,
                           const event::id &event_id)
{
	using prototype = bool (const m::user &, const m::event::id &);

	static mods::import<prototype> function
	{
		"m_receipt", "ircd::m::receipt::ignoring"
	};

	return function(user, event_id);
}

bool
ircd::m::receipt::freshest(const room::id &room_id,
                           const user::id &user_id,
                           const event::id &event_id)
{
	using prototype = bool (const m::room::id &, const m::user::id &, const m::event::id &);

	static mods::import<prototype> function
	{
		"m_receipt", "ircd::m::receipt::freshest"
	};

	return function(room_id, user_id, event_id);
}

bool
ircd::m::receipt::exists(const room::id &room_id,
                         const user::id &user_id,
                         const event::id &event_id)
{
	using prototype = bool (const m::room::id &, const m::user::id &, const m::event::id &);

	static mods::import<prototype> function
	{
		"m_receipt", "ircd::m::receipt::exists"
	};

	return function(room_id, user_id, event_id);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/typing.h
//

//
// m::typing::commit::commit
//

ircd::m::typing::commit::commit(const m::typing &object)
:ircd::m::event::id::buf{[&object]
{
	using prototype = m::event::id::buf (const m::typing &);

	static mods::import<prototype> function
	{
		"m_typing", "commit"
	};

	return function(object);
}()}
{
}

//
// m::typing util
//

void
ircd::m::typing::for_each(const closure &closure)
{
	for_each(closure_bool{[&closure]
	(const auto &event)
	{
		closure(event);
		return true;
	}});
}

bool
ircd::m::typing::for_each(const closure_bool &closure)
{
	using prototype = bool (const m::typing::closure_bool &);

	static mods::import<prototype> function
	{
		"m_typing", "ircd::m::typing::for_each"
	};

	return function(closure);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/presence.h
//

ircd::m::presence::presence(const user &user,
                            const mutable_buffer &buf)
:edu::m_presence{[&user, &buf]
{
	json::object ret;
	get(user, [&ret, &buf]
	(const json::object &content)
	{
		ret =
		{
			data(buf), copy(buf, string_view{content})
		};
	});

	return ret;
}()}
{
}

ircd::m::event::id::buf
ircd::m::presence::set(const user &user,
                       const string_view &presence,
                       const string_view &status_msg)
{
	return set(m::presence
	{
		{ "user_id",     user.user_id  },
		{ "presence",    presence      },
		{ "status_msg",  status_msg    },
	});
}

ircd::m::event::id::buf
ircd::m::presence::set(const presence &object)
{
	using prototype = event::id::buf (const presence &);

	static mods::import<prototype> function
	{
		"m_presence", "ircd::m::presence::set"
	};

	return function(object);
}

void
ircd::m::presence::get(const user &user,
                       const closure &closure)
{
	if(!get(std::nothrow, user, closure))
		throw m::NOT_FOUND
		{
			"No presence found for %s", string_view{user.user_id}
		};
}

bool
ircd::m::presence::get(std::nothrow_t,
                       const user &user,
                       const closure &closure)
{
	static const m::event::fetch::opts fopts
	{
		m::event::keys::include {"content"}
	};

	const auto reclosure{[&closure]
	(const m::event &event)
	{
		closure(json::get<"content"_>(event));
	}};

	return get(std::nothrow, user, reclosure, &fopts);
}

bool
ircd::m::presence::get(std::nothrow_t,
                       const user &user,
                       const closure_event &closure,
                       const event::fetch::opts *const &opts)
{
	using prototype = bool (std::nothrow_t, const m::user &, const closure_event &, const event::fetch::opts *const &);

	static mods::import<prototype> function
	{
		"m_presence", "ircd::m::presence::get"
	};

	return function(std::nothrow, user, closure, opts);
}

ircd::m::event::idx
ircd::m::presence::get(const user &user)
{
	const event::idx ret
	{
		get(std::nothrow, user)
	};

	if(!ret)
		throw m::NOT_FOUND
		{
			"No presence found for %s", string_view{user.user_id}
		};

	return ret;
}

ircd::m::event::idx
ircd::m::presence::get(std::nothrow_t,
                       const user &user)
{
	using prototype = event::idx (std::nothrow_t, const m::user &);

	static mods::import<prototype> function
	{
		"m_presence", "ircd::m::presence::get"
	};

	return function(std::nothrow, user);
}

bool
ircd::m::presence::valid_state(const string_view &state)
{
	using prototype = bool (const string_view &);

	static mods::import<prototype> function
	{
		"m_presence", "ircd::m::presence::valid_state"
	};

	return function(state);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/device.h
//

bool
ircd::m::device::set(const m::user &user,
                     const device &device_)
{
	using prototype = bool (const m::user &, const device &);

	static mods::import<prototype> function
	{
		"m_device", "ircd::m::device::set"
	};

	return function(user, device_);
}

bool
ircd::m::device::set(const m::user &user,
                     const string_view &id,
                     const string_view &prop,
                     const string_view &val)
{
	using prototype = bool (const m::user &, const string_view &, const string_view &, const string_view &);

	static mods::import<prototype> function
	{
		"m_device", "ircd::m::device::set"
	};

	return function(user, id, prop, val);
}

bool
ircd::m::device::del(const m::user &user,
                     const string_view &id)
{
	using prototype = bool (const m::user &, const string_view &);

	static mods::import<prototype> function
	{
		"m_device", "ircd::m::device::del"
	};

	return function(user, id);
}

bool
ircd::m::device::has(const m::user &user,
                     const string_view &id)
{
	using prototype = bool (const m::user &, const string_view &id);

	static mods::import<prototype> function
	{
		"m_device", "ircd::m::device::has"
	};

	return function(user, id);
}

bool
ircd::m::device::has(const m::user &user,
                     const string_view &id,
                     const string_view &prop)
{
	using prototype = bool (const m::user &, const string_view &id, const string_view &prop);

	static mods::import<prototype> function
	{
		"m_device", "ircd::m::device::has"
	};

	return function(user, id, prop);
}

bool
ircd::m::device::get(const m::user &user,
                     const string_view &id,
                     const string_view &prop,
                     const closure &c)
{
	const bool ret
	{
		get(std::nothrow, user, id, prop, c)
	};

	if(!ret)
		throw m::NOT_FOUND
		{
			"Property '%s' for device '%s' for user %s not found",
			id,
			prop,
			string_view{user.user_id}
		};

	return ret;
}

bool
ircd::m::device::get(std::nothrow_t,
                     const m::user &user,
                     const string_view &id,
                     const string_view &prop,
                     const closure &c)
{
	using prototype = bool (std::nothrow_t,
	                        const m::user &,
	                        const string_view &,
	                        const string_view &,
	                        const closure &);

	static mods::import<prototype> function
	{
		"m_device", "ircd::m::device::get"
	};

	return function(std::nothrow, user, id, prop, c);
}

bool
ircd::m::device::for_each(const m::user &user,
                          const string_view &id,
                          const closure_bool &c)
{
	using prototype = bool (const m::user &, const string_view &id, const closure_bool &);

	static mods::import<prototype> function
	{
		"m_device", "ircd::m::device::for_each"
	};

	return function(user, id, c);
}

bool
ircd::m::device::for_each(const m::user &user,
                          const closure_bool &c)
{
	using prototype = bool (const m::user &, const closure_bool &);

	static mods::import<prototype> function
	{
		"m_device", "ircd::m::device::for_each"
	};

	return function(user, c);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/node.h
//

/// ID of the room which indexes all nodes (an instance of the room is
/// provided below).
ircd::m::room::id::buf
nodes_room_id
{
	"nodes", ircd::my_host()
};

/// The nodes room is the database of all nodes. It primarily serves as an
/// indexing mechanism and for top-level node related keys.
///
ircd::m::room
ircd::m::nodes
{
	nodes_room_id
};

ircd::m::node
ircd::m::create(const id::node &node_id,
                const json::members &args)
{
	using prototype = node (const id::node &, const json::members &);

	static mods::import<prototype> function
	{
		"s_node", "create_node"
	};

	assert(node_id);
	return function(node_id, args);
}

bool
ircd::m::exists(const node::id &node_id)
{
	using prototype = bool (const node::id &);

	static mods::import<prototype> function
	{
		"s_node", "exists__nodeid"
	};

	return function(node_id);
}

bool
ircd::m::my(const node &node)
{
	return my(node.node_id);
}

//
// node
//

void
ircd::m::node::key(const string_view &key_id,
                   const ed25519_closure &closure)
const
{
	key(key_id, key_closure{[&closure]
	(const string_view &keyb64)
	{
		const ed25519::pk pk
		{
			[&keyb64](auto &buf)
			{
				b64decode(buf, unquote(keyb64));
			}
		};

		closure(pk);
	}});
}

void
ircd::m::node::key(const string_view &key_id,
                   const key_closure &closure)
const
{
	const auto &server_name
	{
		node_id.hostname()
	};

	m::keys::get(server_name, key_id, [&closure, &key_id]
	(const json::object &keys)
	{
		const json::object &vks
		{
			keys.at("verify_keys")
		};

		const json::object &vkk
		{
			vks.at(key_id)
		};

		const string_view &key
		{
			vkk.at("key")
		};

		closure(key);
	});
}

/// Generates a node-room ID into buffer; see room_id() overload.
ircd::m::id::room::buf
ircd::m::node::room_id()
const
{
	ircd::m::id::room::buf buf;
	return buf.assigned(room_id(buf));
}

/// This generates a room mxid for the "node's room" essentially serving as
/// a database mechanism for this specific node. This room_id is a hash of
/// the node's full mxid.
///
ircd::m::id::room
ircd::m::node::room_id(const mutable_buffer &buf)
const
{
	assert(!empty(node_id));
	const sha256::buf hash
	{
		sha256{node_id}
	};

	char b58[size(hash) * 2];
	return
	{
		buf, b58encode(b58, hash), my_host()
	};
}

//
// node::room
//

ircd::m::node::room::room(const m::node::id &node_id)
:room{m::node{node_id}}
{}

ircd::m::node::room::room(const m::node &node)
:node{node}
,room_id{node.room_id()}
{
	static_cast<m::room &>(*this) = room_id;
}

///////////////////////////////////////////////////////////////////////////////
//
// m/events.h
//

bool
ircd::m::events::for_each(const range &range,
                          const event_filter &filter,
                          const closure_bool &closure)
{
	auto limit
	{
		json::get<"limit"_>(filter)?: 32L
	};

	return for_each(range, [&filter, &closure, &limit]
	(const event::idx &event_idx, const m::event &event)
	-> bool
	{
		if(!match(filter, event))
			return true;

		if(!closure(event_idx, event))
			return false;

		return --limit;
	});
}

bool
ircd::m::events::for_each(const range &range,
                          const closure_bool &closure)
{
	event::fetch event
	{
		range.fopts? *range.fopts : event::fetch::default_opts
	};

	const bool ascending
	{
		range.first <= range.second
	};

	auto start
	{
		ascending?
			range.first:
			std::min(range.first, vm::sequence::retired)
	};

	const auto stop
	{
		ascending?
			std::min(range.second, vm::sequence::retired + 1):
			range.second
	};

	for(; start != stop; ascending? ++start : --start)
		if(seek(event, start, std::nothrow))
			if(!closure(start, event))
				return false;

	return true;
}

///TODO: This impl is temp. Need better dispatching based on filter before
///TODO: fetching event.
bool
ircd::m::events::for_each(const range &range,
                          const event_filter &filter,
                          const event::closure_idx_bool &closure)
{
	auto limit
	{
		json::get<"limit"_>(filter)?: 32L
	};

	return for_each(range, event::closure_idx_bool{[&filter, &closure, &limit, &range]
	(const event::idx &event_idx)
	-> bool
	{
		const m::event::fetch event
		{
			event_idx, std::nothrow, range.fopts? *range.fopts : event::fetch::default_opts
		};

		if(!event.valid)
			return true;

		if(!match(filter, event))
			return true;

		if(!closure(event_idx))
			return false;

		return --limit;
	}});

	return true;
}

bool
ircd::m::events::for_each(const range &range,
                          const event::closure_idx_bool &closure)
{
	const bool ascending
	{
		range.first <= range.second
	};

	auto start
	{
		ascending?
			range.first:
			std::min(range.first, vm::sequence::retired)
	};

	const auto stop
	{
		ascending?
			std::min(range.second, vm::sequence::retired + 1):
			range.second
	};

	auto &column
	{
		dbs::event_json
	};

	auto it
	{
		column.lower_bound(byte_view<string_view>(start))
	};

	for(; bool(it); ascending? ++it : --it)
	{
		const event::idx event_idx
		{
			byte_view<event::idx>(it->first)
		};

		if(ascending && event_idx >= stop)
			break;

		if(!ascending && event_idx <= stop)
			break;

		if(!closure(event_idx))
			return false;
	}

	return true;
}

bool
ircd::m::events::for_each_in_origin(const string_view &origin,
                                    const closure_sender_bool &closure)
{
	auto &column
	{
		dbs::event_sender
	};

	char buf[dbs::EVENT_SENDER_KEY_MAX_SIZE];
	const string_view &key
	{
		dbs::event_sender_key(buf, origin)
	};

	auto it
	{
		column.begin(key)
	};

	for(; bool(it); ++it)
	{
		const auto &keyp
		{
			dbs::event_sender_key(it->first)
		};

		char _buf[m::id::MAX_SIZE];
		mutable_buffer buf{_buf};
		consume(buf, copy(buf, std::get<0>(keyp)));
		consume(buf, copy(buf, ":"_sv));
		consume(buf, copy(buf, origin));
		const string_view &user_id
		{
			_buf, data(buf)
		};

		assert(valid(id::USER, user_id));
		if(!closure(user_id, std::get<1>(keyp)))
			return false;
	}

	return true;
}

bool
ircd::m::events::for_each_in_sender(const id::user &user,
                                    const closure_sender_bool &closure)
{
	auto &column
	{
		dbs::event_sender
	};

	char buf[dbs::EVENT_SENDER_KEY_MAX_SIZE];
	const string_view &key
	{
		dbs::event_sender_key(buf, user, 0)
	};

	auto it
	{
		column.begin(key)
	};

	for(; bool(it); ++it)
	{
		const auto &keyp
		{
			dbs::event_sender_key(it->first)
		};

		if(std::get<0>(keyp) != user.local())
			break;

		if(!closure(user, std::get<1>(keyp)))
			return false;
	}

	return true;
}

bool
ircd::m::events::for_each_in_type(const string_view &type,
                                  const closure_type_bool &closure)
{
	auto &column
	{
		dbs::event_type
	};

	char buf[dbs::EVENT_TYPE_KEY_MAX_SIZE];
	const string_view &key
	{
		dbs::event_type_key(buf, type)
	};

	auto it
	{
		column.begin(key)
	};

	for(; bool(it); ++it)
	{
		const auto &keyp
		{
			dbs::event_type_key(it->first)
		};

		if(!closure(type, std::get<0>(keyp)))
			return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
//
// m/filter.h
//

//TODO: globular expression
//TODO: tribool for contains_url; we currently ignore the false value.
bool
ircd::m::match(const room_event_filter &filter,
               const event &event)
{
	if(json::get<"contains_url"_>(filter) == true)
		if(!at<"content"_>(event).has("url"))
			return false;

	for(const auto &room_id : json::get<"not_rooms"_>(filter))
		if(at<"room_id"_>(event) == unquote(room_id))
			return false;

	if(empty(json::get<"rooms"_>(filter)))
		return match(event_filter{filter}, event);

	for(const auto &room_id : json::get<"rooms"_>(filter))
		if(at<"room_id"_>(event) == unquote(room_id))
			return match(event_filter{filter}, event);

	return false;
}

//TODO: globular expression
bool
ircd::m::match(const event_filter &filter,
               const event &event)
{
	for(const auto &type : json::get<"not_types"_>(filter))
		if(at<"type"_>(event) == unquote(type))
			return false;

	for(const auto &sender : json::get<"not_senders"_>(filter))
		if(at<"sender"_>(event) == unquote(sender))
			return false;

	if(empty(json::get<"senders"_>(filter)) && empty(json::get<"types"_>(filter)))
		return true;

	if(empty(json::get<"senders"_>(filter)))
	{
		for(const auto &type : json::get<"types"_>(filter))
			if(at<"type"_>(event) == unquote(type))
				return true;

		return false;
	}

	if(empty(json::get<"types"_>(filter)))
	{
		for(const auto &sender : json::get<"senders"_>(filter))
			if(at<"sender"_>(event) == unquote(sender))
				return true;

		return false;
	}

	return true;
}

//
// filter
//

/// Convenience interface for filters out of common `?filter=` query string
/// arguments. This function expects a raw urlencoded value of the filter
/// query parameter. It detects if the value is an "inline" filter by being
/// a valid JSON object; otherwise it considers the value an ID and fetches
/// the filter stored previously by the user.
std::string
ircd::m::filter::get(const string_view &val,
                     const m::user &user)
{
	if(!val)
		return {};

	const bool is_inline
	{
		startswith(val, "{") || startswith(val, "%7B")
	};

	if(is_inline)
		return util::string(val.size(), [&val]
		(const mutable_buffer &buf)
		{
			return url::decode(buf, val);
		});

	if(!user.user_id)
		return {};

	char idbuf[m::event::STATE_KEY_MAX_SIZE];
	const string_view &id
	{
		url::decode(idbuf, val)
	};

	const m::user::filter filter
	{
		user
	};

	return filter.get(id);
}

//
// filter::filter
//

ircd::m::filter::filter(const user &user,
                        const string_view &filter_id,
                        const mutable_buffer &buf)
{
	const json::object &obj
	{
		user::filter(user).get(buf, filter_id)
	};

	new (this) m::filter
	{
		obj
	};
}

//
// room_filter
//

ircd::m::room_filter::room_filter(const mutable_buffer &buf,
                                  const json::members &members)
:super_type::tuple
{
	json::stringify(mutable_buffer{buf}, members)
}
{
}

//
// state_filter
//

ircd::m::state_filter::state_filter(const mutable_buffer &buf,
                                    const json::members &members)
:super_type::tuple
{
	json::stringify(mutable_buffer{buf}, members)
}
{
}

//
// room_event_filter
//

ircd::m::room_event_filter::room_event_filter(const mutable_buffer &buf,
                                              const json::members &members)
:super_type::tuple
{
	json::stringify(mutable_buffer{buf}, members)
}
{
}

//
// event_filter
//

ircd::m::event_filter::event_filter(const mutable_buffer &buf,
                                    const json::members &members)
:super_type::tuple
{
	json::stringify(mutable_buffer{buf}, members)
}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// m/rooms.h
//

ircd::m::event::id::buf
ircd::m::rooms::summary_del(const m::room &r)
{
	using prototype = event::id::buf (const m::room &);

	static mods::import<prototype> call
	{
		"m_rooms", "ircd::m::rooms::summary_del"
	};

	return call(r);
}

ircd::m::event::id::buf
ircd::m::rooms::summary_set(const m::room &room)
{
	if(!exists(room))
		throw m::NOT_FOUND
		{
			"Cannot set a summary for room '%s' which I have no state for",
			string_view{room.room_id}
		};

	const unique_buffer<mutable_buffer> buf
	{
		48_KiB
	};

	const json::object summary
	{
		summary_chunk(room, buf)
	};

	return summary_set(room.room_id, summary);
}

ircd::m::event::id::buf
ircd::m::rooms::summary_set(const m::room::id &room_id,
                            const json::object &summary)
{
	using prototype = event::id::buf (const m::room::id &, const json::object &);

	static mods::import<prototype> function
	{
		"m_rooms", "_summary_set"
	};

	return function(room_id, summary);
}

ircd::json::object
ircd::m::rooms::summary_chunk(const m::room &room,
                              const mutable_buffer &buf)
{
	json::stack out{buf};
	{
		json::stack::object obj{out};
		summary_chunk(room, obj);
	}

	return json::object
	{
		out.completed()
	};
}

void
ircd::m::rooms::summary_chunk(const m::room &room,
                              json::stack::object &chunk)
{
	using prototype = void (const m::room &, json::stack::object &);

	static mods::import<prototype> function
	{
		"m_rooms", "_summary_chunk"
	};

	return function(room, chunk);
}

bool
ircd::m::rooms::for_each(const each_opts &opts)
{
	using prototype = bool (const each_opts &);

	static mods::import<prototype> call
	{
		"m_rooms", "ircd::m::rooms::for_each"
	};

	return call(opts);
}

bool
ircd::m::rooms::is_public(const room::id &room_id)
{
	using prototype = bool (const room::id &);

	static mods::import<prototype> call
	{
		"m_rooms", "ircd::m::rooms::is_public"
	};

	return call(room_id);
}

size_t
ircd::m::rooms::count_public(const string_view &server)
{
	using prototype = size_t (const string_view &);

	static mods::import<prototype> function
	{
		"m_rooms", "_count_public"
	};

	return function(server);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/users.h
//

void
ircd::m::users::for_each(const user::closure &closure)
{
	for_each(user::closure_bool{[&closure]
	(const m::user &user)
	{
		closure(user);
		return true;
	}});
}

bool
ircd::m::users::for_each(const user::closure_bool &closure)
{
	return for_each(string_view{}, closure);
}

bool
ircd::m::users::for_each(const string_view &lower_bound,
                         const user::closure_bool &closure)
{
	const m::room::state state
	{
		user::users
	};

	return state.for_each("ircd.user", lower_bound, m::room::state::keys_bool{[&closure]
	(const string_view &user_id)
	{
		const m::user &user
		{
			user_id
		};

		return closure(user);
	}});
}

///////////////////////////////////////////////////////////////////////////////
//
// m/user.h
//

/// ID of the room which indexes all users (an instance of the room is
/// provided below).
ircd::m::room::id::buf
users_room_id
{
	"users", ircd::my_host()
};

/// The users room is the database of all users. It primarily serves as an
/// indexing mechanism and for top-level user related keys. Accounts
/// registered on this server will be among state events in this room.
/// Users do not have access to this room, it is used internally.
///
ircd::m::room
ircd::m::user::users
{
	users_room_id
};

/// ID of the room which stores ephemeral tokens (an instance of the room is
/// provided below).
ircd::m::room::id::buf
tokens_room_id
{
	"tokens", ircd::my_host()
};

/// The tokens room serves as a key-value lookup for various tokens to
/// users, etc. It primarily serves to store access tokens for users. This
/// is a separate room from the users room because in the future it may
/// have an optimized configuration as well as being more easily cleared.
///
ircd::m::room
ircd::m::user::tokens
{
	tokens_room_id
};

ircd::m::user
ircd::m::create(const id::user &user_id,
                const json::members &contents)
{
	using prototype = user (const id::user &, const json::members &);

	static mods::import<prototype> function
	{
		"m_user", "user_create"
	};

	return function(user_id, contents);
}

bool
ircd::m::exists(const user::id &user_id)
{
	return user::users.has("ircd.user", user_id);
}

bool
ircd::m::exists(const user &user)
{
	return exists(user.user_id);
}

bool
ircd::m::my(const user &user)
{
	return my(user.user_id);
}

//
// user
//

/// Generates a user-room ID into buffer; see room_id() overload.
ircd::m::id::room::buf
ircd::m::user::room_id()
const
{
	ircd::m::id::room::buf buf;
	return buf.assigned(room_id(buf));
}

/// This generates a room mxid for the "user's room" essentially serving as
/// a database mechanism for this specific user. This room_id is a hash of
/// the user's full mxid.
///
ircd::m::id::room
ircd::m::user::room_id(const mutable_buffer &buf)
const
{
	assert(!empty(user_id));
	const ripemd160::buf hash
	{
		ripemd160{user_id}
	};

	char b58[size(hash) * 2];
	return
	{
		buf, b58encode(b58, hash), my_host()
	};
}

ircd::m::device::id::buf
ircd::m::user::get_device_from_access_token(const string_view &token)
{
	const event::idx event_idx
	{
		user::tokens.get("ircd.access_token", token)
	};

	device::id::buf ret;
	m::get(event_idx, "content", [&ret]
	(const json::object &content)
	{
		ret = unquote(content.at("device_id"));
	});

	return ret;
}

ircd::string_view
ircd::m::user::gen_access_token(const mutable_buffer &buf)
{
	static const size_t token_max{32};
	static const auto &token_dict{rand::dict::alpha};

	const mutable_buffer out
	{
		data(buf), std::min(token_max, size(buf))
	};

	return rand::string(token_dict, out);
}

ircd::m::event::id::buf
ircd::m::user::activate()
{
	using prototype = event::id::buf (const m::user &);

	static mods::import<prototype> function
	{
		"client_account", "activate__user"
	};

	return function(*this);
}

ircd::m::event::id::buf
ircd::m::user::deactivate()
{
	using prototype = event::id::buf (const m::user &);

	static mods::import<prototype> function
	{
		"client_account", "deactivate__user"
	};

	return function(*this);
}

bool
ircd::m::user::is_active()
const
{
	using prototype = bool (const m::user &);

	static mods::import<prototype> function
	{
		"client_account", "is_active__user"
	};

	return function(*this);
}

ircd::m::event::id::buf
ircd::m::user::password(const string_view &password)
{
	using prototype = event::id::buf (const m::user::id &, const string_view &) noexcept;

	static mods::import<prototype> function
	{
		"client_account", "set_password"
	};

	return function(user_id, password);
}

bool
ircd::m::user::is_password(const string_view &password)
const noexcept try
{
	using prototype = bool (const m::user::id &, const string_view &) noexcept;

	static mods::import<prototype> function
	{
		"client_account", "is_password"
	};

	return function(user_id, password);
}
catch(const std::exception &e)
{
	log::critical
	{
		"user::is_password(): %s %s",
		string_view{user_id},
		e.what()
	};

	return false;
}

//
// user::room
//

ircd::m::user::room::room(const m::user::id &user_id,
                          const vm::copts *const &copts,
                          const event::fetch::opts *const &fopts)
:room
{
	m::user{user_id}, copts, fopts
}
{
}

ircd::m::user::room::room(const m::user &user,
                          const vm::copts *const &copts,
                          const event::fetch::opts *const &fopts)
:user{user}
,room_id{user.room_id()}
{
	static_cast<m::room &>(*this) =
	{
		room_id, copts, fopts
	};
}

//
// user::rooms
//

ircd::m::user::rooms::rooms(const m::user &user)
:user_room{user}
{
}

size_t
ircd::m::user::rooms::count()
const
{
	size_t ret{0};
	for_each([&ret]
	(const m::room &, const string_view &membership)
	{
		++ret;
	});

	return ret;
}

size_t
ircd::m::user::rooms::count(const string_view &membership)
const
{
	size_t ret{0};
	for_each(membership, [&ret]
	(const m::room &, const string_view &membership)
	{
		++ret;
	});

	return ret;
}

void
ircd::m::user::rooms::for_each(const closure &closure)
const
{
	for_each(closure_bool{[&closure]
	(const m::room &room, const string_view &membership)
	{
		closure(room, membership);
		return true;
	}});
}

bool
ircd::m::user::rooms::for_each(const closure_bool &closure)
const
{
	return for_each(string_view{}, closure);
}

void
ircd::m::user::rooms::for_each(const string_view &membership,
                               const closure &closure)
const
{
	for_each(membership, closure_bool{[&closure]
	(const m::room &room, const string_view &membership)
	{
		closure(room, membership);
		return true;
	}});
}

bool
ircd::m::user::rooms::for_each(const string_view &membership,
                               const closure_bool &closure)
const
{
	const m::room::state state
	{
		user_room
	};

	return state.for_each("ircd.member", [&membership, &closure]
	(const string_view &, const string_view &state_key, const m::event::idx &event_idx)
	{
		bool ret{true};
		m::get(std::nothrow, event_idx, "content", [&state_key, &membership, &closure, &ret]
		(const json::object &content)
		{
			const json::string &membership_
			{
				content.get("membership")
			};

			if(!membership || membership_ == membership)
			{
				const m::room::id &room_id{state_key};
				ret = closure(room_id, membership_);
			}
		});

		return ret;
	});
}

//
// user::rooms::origins
//

ircd::m::user::rooms::origins::origins(const m::user &user)
:user{user}
{
}

void
ircd::m::user::rooms::origins::for_each(const closure &closure)
const
{
	for_each(string_view{}, closure);
}

bool
ircd::m::user::rooms::origins::for_each(const closure_bool &closure)
const
{
	return for_each(string_view{}, closure);
}

void
ircd::m::user::rooms::origins::for_each(const string_view &membership,
                                        const closure &closure)
const
{
	for_each(membership, closure_bool{[&closure]
	(const string_view &origin)
	{
		closure(origin);
		return true;
	}});
}

bool
ircd::m::user::rooms::origins::for_each(const string_view &membership,
                                        const closure_bool &closure)
const
{
	const m::user::rooms rooms
	{
		user
	};

	std::set<std::string, std::less<>> seen;
	return rooms.for_each(membership, rooms::closure_bool{[&closure, &seen]
	(const m::room &room, const string_view &membership)
	{
		const m::room::origins origins{room};
		return origins.for_each(origins::closure_bool{[&closure, &seen]
		(const string_view &origin)
		{
			const auto it
			{
				seen.lower_bound(origin)
			};

			if(it != end(seen) && *it == origin)
				return true;

			seen.emplace_hint(it, std::string{origin});
			return closure(origin);
		}});
	}});
}

//
// user::mitsein
//

ircd::m::user::mitsein::mitsein(const m::user &user)
:user{user}
{
}

bool
ircd::m::user::mitsein::has(const m::user &other,
                            const string_view &membership)
const
{
	// Return true if broken out of loop.
	return !for_each(other, membership, rooms::closure_bool{[]
	(const m::room &, const string_view &)
	{
		// Break out of loop at first shared room
		return false;
	}});
}

size_t
ircd::m::user::mitsein::count(const string_view &membership)
const
{
	size_t ret{0};
	for_each(membership, [&ret](const m::user &)
	{
		++ret;
	});

	return ret;
}

size_t
ircd::m::user::mitsein::count(const m::user &user,
                              const string_view &membership)
const
{
	size_t ret{0};
	for_each(user, membership, [&ret](const m::room &, const string_view &)
	{
		++ret;
	});

	return ret;
}

void
ircd::m::user::mitsein::for_each(const closure &closure)
const
{
	for_each(string_view{}, closure);
}

bool
ircd::m::user::mitsein::for_each(const closure_bool &closure)
const
{
	return for_each(string_view{}, closure);
}

void
ircd::m::user::mitsein::for_each(const string_view &membership,
                                 const closure &closure)
const
{
	for_each(membership, closure_bool{[&closure]
	(const m::user &user)
	{
		closure(user);
		return true;
	}});
}

bool
ircd::m::user::mitsein::for_each(const string_view &membership,
                                 const closure_bool &closure)
const
{
	const m::user::rooms rooms
	{
		user
	};

	// here we gooooooo :/
	///TODO: ideal: db schema
	///TODO: minimally: custom alloc?
	std::set<std::string, std::less<>> seen;
	return rooms.for_each(membership, rooms::closure_bool{[&membership, &closure, &seen]
	(m::room room, const string_view &)
	{
		static const event::fetch::opts fopts
		{
			event::keys::include {"state_key"}
		};

		room.fopts = &fopts;
		const m::room::members members{room};
		return members.for_each(membership, event::closure_bool{[&seen, &closure]
		(const m::event &event)
		{
			const auto &other
			{
				at<"state_key"_>(event)
			};

			const auto it
			{
				seen.lower_bound(other)
			};

			if(it != end(seen) && *it == other)
				return true;

			seen.emplace_hint(it, std::string{other});
			return closure(m::user{other});
		}});
	}});
}

void
ircd::m::user::mitsein::for_each(const m::user &user,
                                 const rooms::closure &closure)
const
{
	for_each(user, string_view{}, closure);
}

bool
ircd::m::user::mitsein::for_each(const m::user &user,
                                 const rooms::closure_bool &closure)
const
{
	return for_each(user, string_view{}, closure);
}

void
ircd::m::user::mitsein::for_each(const m::user &user,
                                 const string_view &membership,
                                 const rooms::closure &closure)
const
{
	for_each(user, membership, rooms::closure_bool{[&membership, &closure]
	(const m::room &room, const string_view &)
	{
		closure(room, membership);
		return true;
	}});
}

bool
ircd::m::user::mitsein::for_each(const m::user &user,
                                 const string_view &membership,
                                 const rooms::closure_bool &closure)
const
{
	const m::user::rooms our_rooms{this->user};
	const m::user::rooms their_rooms{user};
	const bool use_our
	{
		our_rooms.count() <= their_rooms.count()
	};

	const m::user::rooms &rooms
	{
		use_our? our_rooms : their_rooms
	};

	const string_view &test_key
	{
		use_our? user.user_id : this->user.user_id
	};

	return rooms.for_each(membership, rooms::closure_bool{[&membership, &closure, &test_key]
	(const m::room &room, const string_view &)
	{
		if(!room.has("m.room.member", test_key))
			return true;

		return closure(room, membership);
	}});
}

//
// user::events
//

ircd::m::user::events::events(const m::user &user)
:user{user}
{
}

size_t
ircd::m::user::events::count()
const
{
	size_t ret{0};
	for_each([&ret](const event::idx &)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
ircd::m::user::events::for_each(const closure_bool &closure)
const
{
	m::event::fetch event;
	return for_each([&closure, &event]
	(const event::idx &event_idx)
	{
		if(!seek(event, event_idx, std::nothrow))
			return true;

		return closure(event);
	});
}

bool
ircd::m::user::events::for_each(const idx_closure_bool &closure)
const
{
	const m::user::rooms rooms
	{
		user
	};

	return rooms.for_each(rooms::closure_bool{[this, &closure]
	(const m::room &room, const string_view &membership)
	{
		m::room::messages it
		{
			room
		};

		bool ret{true};
		for(; ret && it; --it)
		{
			const auto &idx{it.event_idx()};
			m::get(std::nothrow, idx, "sender", [this, &closure, &idx, &ret]
			(const string_view &sender)
			{
				if(sender == this->user.user_id)
					ret = closure(idx);
			});
		}

		return ret;
	}});
}

//
// user::profile
//

ircd::m::event::id::buf
ircd::m::user::profile::set(const string_view &key,
                            const string_view &val)
const
{
	return set(user, key, val);
}

ircd::string_view
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
ircd::m::user::profile::get(std::nothrow_t,
                            const string_view &key,
                            const closure &closure)
const
{
	return get(std::nothrow, user, key, closure);
}

bool
ircd::m::user::profile::for_each(const closure_bool &closure)
const
{
	return for_each(user, closure);
}

ircd::m::event::id::buf
ircd::m::user::profile::set(const m::user &u,
                            const string_view &k,
                            const string_view &v)
{
	using prototype = event::id::buf (const m::user &, const string_view &, const string_view &);

	static mods::import<prototype> function
	{
		"client_profile", "ircd::m::user::profile::set"
	};

	return function(u, k, v);
}

bool
ircd::m::user::profile::get(std::nothrow_t,
                            const m::user &u,
                            const string_view &k,
                            const closure &c)
{
	using prototype = bool (std::nothrow_t, const m::user &, const string_view &, const closure &);

	static mods::import<prototype> function
	{
		"client_profile", "ircd::m::user::profile::get"
	};

	return function(std::nothrow, u, k, c);
}

bool
ircd::m::user::profile::for_each(const m::user &u,
                                 const closure_bool &c)
{
	using prototype = bool (const m::user &, const closure_bool &);

	static mods::import<prototype> function
	{
		"client_profile", "ircd::m::user::profile::for_each"
	};

	return function(u, c);
}

void
ircd::m::user::profile::fetch(const m::user &u,
                              const net::hostport &remote,
                              const string_view &key)
{
	using prototype = void (const m::user &, const net::hostport &, const string_view &);

	static mods::import<prototype> function
	{
		"client_profile", "ircd::m::user::profile::fetch"
	};

	return function(u, remote, key);
}

//
// user::account_data
//

ircd::m::event::id::buf
ircd::m::user::account_data::set(const string_view &type,
                                 const json::object &val)
const
{
	return set(user, type, val);
}

ircd::json::object
ircd::m::user::account_data::get(const mutable_buffer &out,
                                 const string_view &type)
const
{
	json::object ret;
	get(std::nothrow, type, [&out, &ret]
	(const string_view &type, const json::object &val)
	{
		ret = string_view { data(out), copy(out, val) };
	});

	return ret;
}

void
ircd::m::user::account_data::get(const string_view &type,
                                 const closure &closure)
const
{
	if(!get(std::nothrow, user, type, closure))
		throw m::NOT_FOUND
		{
			"account data type '%s' for user %s not found",
			type,
			string_view{user.user_id}
		};
}

bool
ircd::m::user::account_data::get(std::nothrow_t,
                                 const string_view &type,
                                 const closure &closure)
const
{
	return get(std::nothrow, user, type, closure);
}

bool
ircd::m::user::account_data::for_each(const closure_bool &closure)
const
{
	return for_each(user, closure);
}

ircd::m::event::id::buf
ircd::m::user::account_data::set(const m::user &u,
                                 const string_view &t,
                                 const json::object &v)
{
	using prototype = event::id::buf (const m::user &, const string_view &, const json::object &);

	static mods::import<prototype> function
	{
		"client_user", "ircd::m::user::account_data::set"
	};

	return function(u, t, v);
}

bool
ircd::m::user::account_data::get(std::nothrow_t,
                                 const m::user &u,
                                 const string_view &t,
                                 const closure &c)
{
	using prototype = bool (std::nothrow_t, const m::user &, const string_view &, const closure &);

	static mods::import<prototype> function
	{
		"client_user", "ircd::m::user::account_data::get"
	};

	return function(std::nothrow, u, t, c);
}

bool
ircd::m::user::account_data::for_each(const m::user &u,
                                      const closure_bool &c)
{
	using prototype = bool (const m::user &, const closure_bool &);

	static mods::import<prototype> function
	{
		"client_user", "ircd::m::user::account_data::for_each"
	};

	return function(u, c);
}

//
// user::room_account_data
//

ircd::m::event::id::buf
ircd::m::user::room_account_data::set(const string_view &type,
                                      const json::object &val)
const
{
	return set(user, room, type, val);
}

ircd::json::object
ircd::m::user::room_account_data::get(const mutable_buffer &out,
                                      const string_view &type)
const
{
	json::object ret;
	get(std::nothrow, type, [&out, &ret]
	(const string_view &type, const json::object &val)
	{
		ret = string_view { data(out), copy(out, val) };
	});

	return ret;
}

void
ircd::m::user::room_account_data::get(const string_view &type,
                                      const closure &closure)
const
{
	if(!get(std::nothrow, user, room, type, closure))
		throw m::NOT_FOUND
		{
			"account data type '%s' for user %s in room %s not found",
			type,
			string_view{user.user_id},
			string_view{room.room_id}
		};
}

bool
ircd::m::user::room_account_data::get(std::nothrow_t,
                                     const string_view &type,
                                     const closure &closure)
const
{
	return get(std::nothrow, user, room, type, closure);
}

bool
ircd::m::user::room_account_data::for_each(const closure_bool &closure)
const
{
	return for_each(user, room, closure);
}

ircd::m::event::id::buf
ircd::m::user::room_account_data::set(const m::user &u,
                                      const m::room &r,
                                      const string_view &t,
                                      const json::object &v)
{
	using prototype = event::id::buf (const m::user &, const m::room &, const string_view &, const json::object &);

	static mods::import<prototype> function
	{
		"client_user", "ircd::m::user::room_account_data::set"
	};

	return function(u, r, t, v);
}

bool
ircd::m::user::room_account_data::get(std::nothrow_t,
                                      const m::user &u,
                                      const m::room &r,
                                      const string_view &t,
                                      const closure &c)
{
	using prototype = bool (std::nothrow_t, const m::user &, const m::room &, const string_view &, const closure &);

	static mods::import<prototype> function
	{
		"client_user", "ircd::m::user::room_account_data::get"
	};

	return function(std::nothrow, u, r, t, c);
}

bool
ircd::m::user::room_account_data::for_each(const m::user &u,
                                           const m::room &r,
                                           const closure_bool &c)
{
	using prototype = bool (const m::user &, const m::room &, const closure_bool &);

	static mods::import<prototype> function
	{
		"client_user", "ircd::m::user::room_account_data::for_each"
	};

	return function(u, r, c);
}

ircd::string_view
ircd::m::user::room_account_data::_type(const mutable_buffer &out,
                                        const m::room::id &room_id)
{
	using prototype = string_view (const mutable_buffer &, const m::room::id &);

	static mods::import<prototype> function
	{
		"client_user", "ircd::m::user::room_account_data::_type"
	};

	return function(out, room_id);
}

//
// user::room_tags
//

bool
ircd::m::user::room_tags::del(const string_view &type)
const
{
	return del(user, room, type);
}

ircd::m::event::id::buf
ircd::m::user::room_tags::set(const string_view &type,
                              const json::object &val)
const
{
	return set(user, room, type, val);
}

ircd::json::object
ircd::m::user::room_tags::get(const mutable_buffer &out,
                              const string_view &type)
const
{
	json::object ret;
	get(std::nothrow, type, [&out, &ret]
	(const string_view &type, const json::object &val)
	{
		ret = string_view { data(out), copy(out, val) };
	});

	return ret;
}

void
ircd::m::user::room_tags::get(const string_view &type,
                              const closure &closure)
const
{
	if(!get(std::nothrow, user, room, type, closure))
		throw m::NOT_FOUND
		{
			"account data type '%s' for user %s in room %s not found",
			type,
			string_view{user.user_id},
			string_view{room.room_id}
		};
}

bool
ircd::m::user::room_tags::get(std::nothrow_t,
                              const string_view &type,
                              const closure &closure)
const
{
	return get(std::nothrow, user, room, type, closure);
}

bool
ircd::m::user::room_tags::for_each(const closure_bool &closure)
const
{
	return for_each(user, room, closure);
}

bool
ircd::m::user::room_tags::del(const m::user &u,
                              const m::room &r,
                              const string_view &t)
{
	using prototype = bool (const m::user &, const m::room &, const string_view &);

	static mods::import<prototype> function
	{
		"client_user", "ircd::m::user::room_tags::del"
	};

	return function(u, r, t);
}

ircd::m::event::id::buf
ircd::m::user::room_tags::set(const m::user &u,
                              const m::room &r,
                              const string_view &t,
                              const json::object &v)
{
	using prototype = event::id::buf (const m::user &, const m::room &, const string_view &, const json::object &);

	static mods::import<prototype> function
	{
		"client_user", "ircd::m::user::room_tags::set"
	};

	return function(u, r, t, v);
}

bool
ircd::m::user::room_tags::get(std::nothrow_t,
                              const m::user &u,
                              const m::room &r,
                              const string_view &t,
                              const closure &c)
{
	using prototype = bool (std::nothrow_t, const m::user &, const m::room &, const string_view &, const closure &);

	static mods::import<prototype> function
	{
		"client_user", "ircd::m::user::room_tags::get"
	};

	return function(std::nothrow, u, r, t, c);
}

bool
ircd::m::user::room_tags::for_each(const m::user &u,
                                   const m::room &r,
                                   const closure_bool &c)
{
	using prototype = bool (const m::user &, const m::room &, const closure_bool &);

	static mods::import<prototype> function
	{
		"client_user", "ircd::m::user::room_tags::for_each"
	};

	return function(u, r, c);
}

ircd::string_view
ircd::m::user::room_tags::_type(const mutable_buffer &out,
                                const m::room::id &room_id)
{
	using prototype = string_view (const mutable_buffer &, const m::room::id &);

	static mods::import<prototype> function
	{
		"client_user", "ircd::m::user::room_tags::_type"
	};

	return function(out, room_id);
}

//
// user::filter
//

ircd::string_view
ircd::m::user::filter::set(const mutable_buffer &buf,
                           const json::object &val)
const
{
	return set(buf, user, val);
}

std::string
ircd::m::user::filter::get(const string_view &id)
const
{
	std::string ret;
	get(std::nothrow, id, [&ret]
	(const string_view &id, const json::object &val)
	{
		ret.assign(data(val), size(val));
	});

	return ret;
}

ircd::json::object
ircd::m::user::filter::get(const mutable_buffer &out,
                           const string_view &id)
const
{
	json::object ret;
	get(std::nothrow, id, [&out, &ret]
	(const string_view &id, const json::object &val)
	{
		ret = string_view { data(out), copy(out, val) };
	});

	return ret;
}

void
ircd::m::user::filter::get(const string_view &id,
                           const closure &closure)
const
{
	if(!get(std::nothrow, user, id, closure))
		throw m::NOT_FOUND
		{
			"filter id '%s' for user %s not found",
			id,
			string_view{user.user_id}
		};
}

bool
ircd::m::user::filter::get(std::nothrow_t,
                           const string_view &id,
                           const closure &closure)
const
{
	return get(std::nothrow, user, id, closure);
}

bool
ircd::m::user::filter::for_each(const closure_bool &closure)
const
{
	return for_each(user, closure);
}

ircd::string_view
ircd::m::user::filter::set(const mutable_buffer &buf,
                           const m::user &u,
                           const json::object &v)
{
	using prototype = string_view (const mutable_buffer &, const m::user &, const json::object &);

	static mods::import<prototype> function
	{
		"client_user", "ircd::m::user::filter::set"
	};

	return function(buf, u, v);
}

bool
ircd::m::user::filter::get(std::nothrow_t,
                           const m::user &u,
                           const string_view &id,
                           const closure &c)
{
	using prototype = bool (std::nothrow_t, const m::user &, const string_view &, const closure &);

	static mods::import<prototype> function
	{
		"client_user", "ircd::m::user::filter::get"
	};

	return function(std::nothrow, u, id, c);
}

bool
ircd::m::user::filter::for_each(const m::user &u,
                                const closure_bool &c)
{
	using prototype = bool (const m::user &, const closure_bool &);

	static mods::import<prototype> function
	{
		"client_user", "ircd::m::user::filter::for_each"
	};

	return function(u, c);
}

//
// user::ignores
//

bool
ircd::m::user::ignores::has(const m::user::id &other)
const
{
	return has(user, other);
}

bool
ircd::m::user::ignores::for_each(const closure_bool &closure)
const
{
	return for_each(user, closure);
}

bool
ircd::m::user::ignores::has(const m::user &u,
                            const m::user::id &other)
{
	using prototype = bool (const m::user &, const m::user::id &);

	static mods::import<prototype> call
	{
		"m_ignored_user_list", "ircd::m::user::ignores::has"
	};

	return call(u, other);
}

bool
ircd::m::user::ignores::for_each(const m::user &u,
                                 const closure_bool &c)
{
	using prototype = bool (const m::user &, const closure_bool &);

	static mods::import<prototype> call
	{
		"m_ignored_user_list", "ircd::m::user::ignores::for_each"
	};

	return call(u, c);
}

bool
ircd::m::user::ignores::enforce(const string_view &type)
{
	using prototype = bool (const string_view &);

	static mods::import<prototype> call
	{
		"m_ignored_user_list", "ircd::m::user::ignores::enforce"
	};

	return call(type);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/room.h
//

size_t
ircd::m::room::purge(const room &room)
{
	using prototype = size_t (const m::room &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::room::purge"
	};

	return call(room);
}

size_t
ircd::m::room::state::purge_replaced(const state &state)
{
	using prototype = size_t (const room::state &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::room::state::purge_replaced"
	};

	return call(state);
}

bool
ircd::m::room::state::force_present(const event &event)
{
	using prototype = bool (const m::event &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::room::state::force_present"
	};

	return call(event);
}

size_t
ircd::m::room::state::rebuild_present(const state &state)
{
	using prototype = bool (const room::state &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::room::state::rebuild_present"
	};

	return call(state);
}

size_t
ircd::m::room::state::rebuild_history(const state &state)
{
	using prototype = bool (const room::state &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::room::state::rebuild_history"
	};

	return call(state);
}

size_t
ircd::m::room::state::clear_history(const state &state)
{
	using prototype = bool (const room::state &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::room::state::clear_history"
	};

	return call(state);
}

size_t
ircd::m::room::state::prefetch(const state &state,
                               const string_view &type,
                               const event::idx_range &range)
{
	using prototype = size_t (const room::state &, const string_view &, const event::idx_range &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::room::state::prefetch"
	};

	return call(state, type, range);
}

ircd::m::event::idx
ircd::m::room::state::next(const event::idx &event_idx)
{
	using prototype = event::idx (const event::idx &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::room::state::next"
	};

	return call(event_idx);
}

ircd::m::event::idx
ircd::m::room::state::prev(const event::idx &event_idx)
{
	using prototype = event::idx (const event::idx &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::room::state::prev"
	};

	return call(event_idx);
}

bool
ircd::m::room::state::next(const event::idx &event_idx,
                           const event::closure_idx_bool &closure)
{
	using prototype = bool (const event::idx &, const event::closure_idx_bool &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::room::state::next"
	};

	return call(event_idx, closure);
}

bool
ircd::m::room::state::prev(const event::idx &event_idx,
                           const event::closure_idx_bool &closure)
{
	using prototype = bool (const event::idx &, const event::closure_idx_bool &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::room::state::prev"
	};

	return call(event_idx, closure);
}

ircd::m::room
ircd::m::create(const id::room &room_id,
                const id::user &creator,
                const string_view &preset)
{
	return create(createroom
	{
		{ "room_id",  room_id },
		{ "creator",  creator },
		{ "preset",   preset  },
	});
}

ircd::m::room
ircd::m::create(const createroom &c,
                json::stack::array *const &errors)
{
	using prototype = room (const createroom &, json::stack::array *const &);

	static mods::import<prototype> call
	{
		"client_createroom", "ircd::m::create"
	};

	return call(c, errors);
}

ircd::m::event::id::buf
ircd::m::join(const id::room_alias &room_alias,
              const id::user &user_id)
{
	using prototype = event::id::buf (const id::room_alias &, const id::user &);

	static mods::import<prototype> function
	{
		"client_rooms", "join__alias_user"
	};

	return function(room_alias, user_id);
}

ircd::m::event::id::buf
ircd::m::join(const room &room,
              const id::user &user_id)
{
	using prototype = event::id::buf (const m::room &, const id::user &);

	static mods::import<prototype> function
	{
		"client_rooms", "join__room_user"
	};

	return function(room, user_id);
}

ircd::m::event::id::buf
ircd::m::leave(const room &room,
               const id::user &user_id)
{
	using prototype = event::id::buf (const m::room &, const id::user &);

	static mods::import<prototype> function
	{
		"client_rooms", "leave__room_user"
	};

	return function(room, user_id);
}

ircd::m::event::id::buf
ircd::m::invite(const room &room,
                const id::user &target,
                const id::user &sender)
{
	json::iov content;
	return invite(room, target, sender, content);
}

ircd::m::event::id::buf
ircd::m::invite(const room &room,
                const id::user &target,
                const id::user &sender,
                json::iov &content)
{
	using prototype = event::id::buf (const m::room &, const id::user &, const id::user &, json::iov &);

	static mods::import<prototype> call
	{
		"client_rooms", "ircd::m::invite"
	};

	return call(room, target, sender, content);
}

ircd::m::event::id::buf
ircd::m::redact(const room &room,
                const id::user &sender,
                const id::event &event_id,
                const string_view &reason)
{
	using prototype = event::id::buf (const m::room &, const id::user &, const id::event &, const string_view &);

	static mods::import<prototype> function
	{
		"client_rooms", "redact__"
	};

	return function(room, sender, event_id, reason);
}

ircd::m::event::id::buf
ircd::m::notice(const room &room,
                const string_view &body)
{
	return message(room, me.user_id, body, "m.notice");
}

ircd::m::event::id::buf
ircd::m::notice(const room &room,
                const m::id::user &sender,
                const string_view &body)
{
	return message(room, sender, body, "m.notice");
}

ircd::m::event::id::buf
ircd::m::msghtml(const room &room,
                 const m::id::user &sender,
                 const string_view &html,
                 const string_view &alt,
                 const string_view &msgtype)
{
	return message(room, sender,
	{
		{ "msgtype",         msgtype                       },
		{ "format",          "org.matrix.custom.html"      },
		{ "body",            { alt?: html, json::STRING }  },
		{ "formatted_body",  { html, json::STRING }        },
	});
}

ircd::m::event::id::buf
ircd::m::message(const room &room,
                 const m::id::user &sender,
                 const string_view &body,
                 const string_view &msgtype)
{
	return message(room, sender,
	{
		{ "body",     { body,    json::STRING } },
		{ "msgtype",  { msgtype, json::STRING } },
	});
}

ircd::m::event::id::buf
ircd::m::message(const room &room,
                 const m::id::user &sender,
                 const json::members &contents)
{
	return send(room, sender, "m.room.message", contents);
}

ircd::m::event::id::buf
__attribute__((stack_protect))
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const string_view &state_key,
              const json::members &contents)
{
	const size_t contents_count
	{
		std::min(contents.size(), json::object::max_sorted_members)
	};

	json::iov _content;
	json::iov::push content[contents_count]; // 48B each
	return send(room, sender, type, state_key, make_iov(_content, content, contents_count, contents));
}

ircd::m::event::id::buf
__attribute__((stack_protect))
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const string_view &state_key,
              const json::object &contents)
{
	const size_t contents_count
	{
		std::min(contents.size(), json::object::max_sorted_members)
	};

	json::iov _content;
	json::iov::push content[contents_count]; // 48B each
	return send(room, sender, type, state_key, make_iov(_content, content, contents_count, contents));
}

ircd::m::event::id::buf
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const string_view &state_key,
              const json::iov &content)
{
	using prototype = event::id::buf (const m::room &, const id::user &, const string_view &, const string_view &, const json::iov &);

	static mods::import<prototype> function
	{
		"m_room", "ircd::m::send"
	};

	return function(room, sender, type, state_key, content);
}

ircd::m::event::id::buf
__attribute__((stack_protect))
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const json::members &contents)
{
	const size_t contents_count
	{
		std::min(contents.size(), json::object::max_sorted_members)
	};

	json::iov _content;
	json::iov::push content[contents_count]; // 48B each
	return send(room, sender, type, make_iov(_content, content, contents_count, contents));
}

ircd::m::event::id::buf
__attribute__((stack_protect))
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const json::object &contents)
{
	const size_t contents_count
	{
		std::min(contents.size(), json::object::max_sorted_members)
	};

	json::iov _content;
	json::iov::push content[contents_count]; // 48B each
	return send(room, sender, type, make_iov(_content, content, contents_count, contents));
}

ircd::m::event::id::buf
ircd::m::send(const room &room,
              const id::user &sender,
              const string_view &type,
              const json::iov &content)
{
	using prototype = event::id::buf (const m::room &, const id::user &, const string_view &, const json::iov &);

	static mods::import<prototype> function
	{
		"m_room", "ircd::m::send"
	};

	return function(room, sender, type, content);
}

ircd::m::event::id::buf
ircd::m::commit(const room &room,
                json::iov &event,
                const json::iov &contents)
{
	vm::copts opts
	{
		room.copts?
			*room.copts:
			vm::default_copts
	};

	// Some functionality on this server may create an event on behalf
	// of remote users. It's safe for us to mask this here, but eval'ing
	// this event in any replay later will require special casing.
	opts.non_conform |= event::conforms::MISMATCH_ORIGIN_SENDER;

	 // Stupid protocol workaround
	opts.non_conform |= event::conforms::MISSING_PREV_STATE;

	// Don't need this here
	opts.verify = false;

	vm::eval eval
	{
		opts
	};

	eval(room, event, contents);
	return eval.event_id;
}

std::pair<bool, int64_t>
ircd::m::is_complete(const room &r)
{
	using prototype = std::pair<bool, int64_t> (const room &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::is_complete"
	};

	return call(r);
}

size_t
ircd::m::count_since(const m::event::id &a,
                     const m::event::id &b)
{
	return count_since(index(a), index(b));
}

size_t
ircd::m::count_since(const m::event::idx &a,
                     const m::event::idx &b)
{
	// Get the room_id from b here; a might not be in the same room but downstream
	// the counter seeks to a in the given room and will properly fail there.
	room::id::buf room_id
	{
		m::get(std::max(a, b), "room_id", room_id)
	};

	return count_since(room_id, a, b);
}

size_t
ircd::m::count_since(const room &room,
                     const m::event::id &a,
                     const m::event::id &b)
{
	return count_since(room, index(a), index(b));
}

size_t
ircd::m::count_since(const room &r,
                     const event::idx &a,
                     const event::idx &b)
{
	using prototype = size_t (const room &,
	                          const event::idx &,
	                          const event::idx &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::count_since"
	};

	return call(r, std::min(a, b), std::max(a, b));
}

ircd::m::id::room::buf
ircd::m::room_id(const id::room_alias &room_alias)
{
	char buf[m::id::MAX_SIZE + 1];
	static_assert(sizeof(buf) <= 256);
	return room_id(buf, room_alias);
}

ircd::m::id::room::buf
ircd::m::room_id(const string_view &room_id_or_alias)
{
	char buf[m::id::MAX_SIZE + 1];
	static_assert(sizeof(buf) <= 256);
	return room_id(buf, room_id_or_alias);
}

ircd::m::id::room
ircd::m::room_id(const mutable_buffer &out,
                 const string_view &room_id_or_alias)
{
	switch(m::sigil(room_id_or_alias))
	{
		case id::ROOM:
			return id::room{out, room_id_or_alias};

		case id::USER:
		{
			const m::user::room user_room(room_id_or_alias);
			return string_view{data(out), copy(out, user_room.room_id)};
		}

		default:
			return room_id(out, id::room_alias{room_id_or_alias});
	}
}

ircd::m::id::room
ircd::m::room_id(const mutable_buffer &out,
                 const id::room_alias &room_alias)
{
	room::id ret;
	room::aliases::cache::get(room_alias, [&out, &ret]
	(const room::id &room_id)
	{
		ret = string_view { data(out), copy(out, room_id) };
	});

	return ret;
}

bool
ircd::m::exists(const id::room_alias &room_alias,
                const bool &remote_query)
{
	if(room::aliases::cache::has(room_alias))
		return true;

	if(!remote_query)
		return false;

	return room::aliases::cache::get(std::nothrow, room_alias, [](const room::id &room_id) {});
}

///////////////////////////////////////////////////////////////////////////////
//
// m/txn.h
//

/// Returns the serial size of the JSON this txn would consume. Note: this
/// creates a json::iov involving a timestamp to figure out the total size
/// of the txn. When the user creates the actual txn a different timestamp
/// is created which may be a different size. Consider using the lower-level
/// create(closure) or add some pad to be sure.
///
size_t
ircd::m::txn::serialized(const array &pdu,
                         const array &edu,
                         const array &pdu_failure)
{
	size_t ret;
	const auto closure{[&ret]
	(const json::iov &iov)
	{
		ret = json::serialized(iov);
	}};

	create(closure, pdu, edu, pdu_failure);
	return ret;
}

/// Stringifies a txn from the inputs into the returned std::string
///
std::string
ircd::m::txn::create(const array &pdu,
                     const array &edu,
                     const array &pdu_failure)
{
	std::string ret;
	const auto closure{[&ret]
	(const json::iov &iov)
	{
		ret = json::strung(iov);
	}};

	create(closure, pdu, edu, pdu_failure);
	return ret;
}

/// Stringifies a txn from the inputs into the buffer
///
ircd::string_view
ircd::m::txn::create(const mutable_buffer &buf,
                     const array &pdu,
                     const array &edu,
                     const array &pdu_failure)
{
	string_view ret;
	const auto closure{[&buf, &ret]
	(const json::iov &iov)
	{
		ret = json::stringify(mutable_buffer{buf}, iov);
	}};

	create(closure, pdu, edu, pdu_failure);
	return ret;
}

/// Forms a txn from the inputs into a json::iov and presents that iov
/// to the user's closure.
///
void
ircd::m::txn::create(const closure &closure,
                     const array &pdu,
                     const array &edu,
                     const array &pdu_failure)
{
	using ircd::size;

	json::iov iov;
	const json::iov::push push[]
	{
		{ iov, { "origin",            my_host()                   }},
		{ iov, { "origin_server_ts",  ircd::time<milliseconds>()  }},
	};

	const json::iov::add _pdus
	{
		iov, !empty(pdu),
		{
			"pdus", [&pdu]() -> json::value
			{
				return { data(pdu), size(pdu) };
			}
		}
	};

	const json::iov::add _edus
	{
		iov, !empty(edu),
		{
			"edus", [&edu]() -> json::value
			{
				return { data(edu), size(edu) };
			}
		}
	};

	const json::iov::add _pdu_failures
	{
		iov, !empty(pdu_failure),
		{
			"pdu_failures", [&pdu_failure]() -> json::value
			{
				return { data(pdu_failure), size(pdu_failure) };
			}
		}
	};

	closure(iov);
}

ircd::string_view
ircd::m::txn::create_id(const mutable_buffer &out,
                        const string_view &txn)
{
	const sha256::buf hash
	{
		sha256{txn}
	};

	const string_view txnid
	{
		b58encode(out, hash)
	};

	return txnid;
}


///////////////////////////////////////////////////////////////////////////////
//
// m/request.h
//

//
// request
//

ircd::m::request::request(const string_view &method,
                          const string_view &uri,
                          const mutable_buffer &body_buf,
                          const json::members &body)
:request
{
	my_host(),
	string_view{},
	method,
	uri,
	json::stringify(mutable_buffer{body_buf}, body)
}
{}

ircd::m::request::request(const string_view &method,
                          const string_view &uri)
:request
{
	my_host(),
	string_view{},
	method,
	uri,
	json::object{}
}
{}

ircd::m::request::request(const string_view &method,
                          const string_view &uri,
                          const json::object &content)
:request
{
	my_host(),
	string_view{},
	method,
	uri,
	content
}
{}

ircd::m::request::request(const string_view &origin,
                          const string_view &destination,
                          const string_view &method,
                          const string_view &uri,
                          const json::object &content)
{
	json::get<"origin"_>(*this) = origin;
	json::get<"destination"_>(*this) = destination;
	json::get<"method"_>(*this) = method;
	json::get<"uri"_>(*this) = uri;
	json::get<"content"_>(*this) = content;
}

decltype(ircd::m::request::headers_max)
ircd::m::request::headers_max
{
	32UL
};

ircd::string_view
ircd::m::request::operator()(const mutable_buffer &out,
                             const vector_view<const http::header> &addl_headers)
const
{
	thread_local http::header header[headers_max];
	const ctx::critical_assertion ca;
	size_t headers{0};

	header[headers++] =
	{
		"User-Agent", info::user_agent
	};

	thread_local char x_matrix[2_KiB];
	if(startswith(at<"uri"_>(*this), "/_matrix/federation"))
	{
		const auto &sk{self::secret_key};
		const auto &pkid{self::public_key_id};
		header[headers++] =
		{
			"Authorization", generate(x_matrix, sk, pkid)
		};
	}

	assert(headers <= headers_max);
	assert(headers + addl_headers.size() <= headers_max);
	for(size_t i(0); i < addl_headers.size() && headers < headers_max; ++i)
		header[headers++] = addl_headers.at(i);

	static const string_view content_type
	{
		"application/json; charset=utf-8"_sv
	};

	const auto content_length
	{
		string_view(json::get<"content"_>(*this)).size()
	};

	window_buffer sb{out};
	http::request
	{
		sb,
		at<"destination"_>(*this),
		at<"method"_>(*this),
		at<"uri"_>(*this),
		content_length,
		content_type,
		{ header, headers }
	};

	return sb.completed();
}

decltype(ircd::m::request::generate_content_max)
ircd::m::request::generate_content_max
{
	{ "name",    "ircd.m.request.generate.content_max" },
	{ "default",  long(1_MiB)                          },
};

ircd::string_view
ircd::m::request::generate(const mutable_buffer &out,
                           const ed25519::sk &sk,
                           const string_view &pkid)
const
{
	const ctx::critical_assertion ca;
	thread_local unique_buffer<mutable_buffer> buf
	{
		size_t(generate_content_max)
	};

	if(unlikely(json::serialized(*this) > buffer::size(buf)))
		throw m::error
		{
			"M_REQUEST_TOO_LARGE", "This server generated a request of %zu bytes; limit is %zu",
			json::serialized(*this),
			buffer::size(buf)
		};

	const json::object object
	{
		stringify(mutable_buffer{buf}, *this)
	};

	const ed25519::sig sig
	{
		self::secret_key.sign(object)
	};

	const auto &origin
	{
		unquote(string_view{at<"origin"_>(*this)})
	};

	thread_local char sigb64[1_KiB];
	return fmt::sprintf
	{
		out, "X-Matrix origin=%s,key=\"%s\",sig=\"%s\"",
		origin,
		pkid,
		b64encode_unpadded(sigb64, sig)
	};
}

bool
ircd::m::request::verify(const string_view &key,
                         const string_view &sig_)
const
{
	const ed25519::sig sig
	{
		[&sig_](auto &buf)
		{
			b64decode(buf, sig_);
		}
	};

	const m::node::id::buf node_id
	{
		m::node::id::origin, unquote(at<"origin"_>(*this))
	};

	const m::node node
	{
		node_id
	};

	bool verified{false};
	node.key(key, [this, &verified, &sig]
	(const ed25519::pk &pk)
	{
		verified = verify(pk, sig);
	});

	return verified;
}

decltype(ircd::m::request::verify_content_max)
ircd::m::request::verify_content_max
{
	{ "name",    "ircd.m.request.verify.content_max" },
	{ "default",  long(1_MiB)                        },
};

bool
ircd::m::request::verify(const ed25519::pk &pk,
                         const ed25519::sig &sig)
const
{
	// Matrix spec sez that an empty content object {} is excluded entirely
	// from the verification. Our JSON only excludes members if they evaluate
	// to undefined i.e json::object{}/string_view{} but not json::object{"{}"}
	// or even json::object{""}; rather than burdening the caller with ensuring
	// their assignment conforms perfectly, we ensure correctness manually.
	auto _this(*this);
	if(empty(json::get<"content"_>(*this)))
		json::get<"content"_>(_this) = json::object{};

	const ctx::critical_assertion ca;
	thread_local unique_buffer<mutable_buffer> buf
	{
		size_t(verify_content_max)
	};

	const size_t request_size
	{
		json::serialized(_this)
	};

	if(unlikely(request_size > buffer::size(buf)))
		throw m::error
		{
			http::PAYLOAD_TOO_LARGE, "M_REQUEST_TOO_LARGE",
			"The request size %zu bytes exceeds maximum of %zu bytes",
			request_size,
			buffer::size(buf)
		};

	const json::object object
	{
		stringify(mutable_buffer{buf}, _this)
	};

	return verify(pk, sig, object);
}

bool
ircd::m::request::verify(const ed25519::pk &pk,
                         const ed25519::sig &sig,
                         const json::object &object)
{
	return pk.verify(object, sig);
}

//
// x_matrix
//

ircd::m::request::x_matrix::x_matrix(const string_view &input)
{
	string_view tokens[3];
	if(ircd::tokens(split(input, ' ').second, ',', tokens) != 3)
		throw std::out_of_range{"The x_matrix header is malformed"};

	for(const auto &token : tokens)
	{
		const auto &kv{split(token, '=')};
		const auto &val{unquote(kv.second)};
		switch(hash(kv.first))
		{
		    case hash("origin"):  origin = val;  break;
		    case hash("key"):     key = val;     break;
		    case hash("sig"):     sig = val;     break;
		}
	}

	if(empty(origin))
		throw std::out_of_range{"The x_matrix header is missing 'origin='"};

	if(empty(key))
		throw std::out_of_range{"The x_matrix header is missing 'key='"};

	if(empty(sig))
		throw std::out_of_range{"The x_matrix header is missing 'sig='"};
}

///////////////////////////////////////////////////////////////////////////////
//
// m/hook.h
//

// Internal utils
namespace ircd::m
{
	static bool _hook_match(const m::event &matching, const m::event &);
	static void _hook_fix_state_key(const json::members &, json::member &);
	static void _hook_fix_room_id(const json::members &, json::member &);
	static void _hook_fix_sender(const json::members &, json::member &);
	static json::strung _hook_make_feature(const json::members &);
}

//
// hook::maps
//

struct ircd::m::hook::maps
{
	std::multimap<string_view, base *> origin;
	std::multimap<string_view, base *> room_id;
	std::multimap<string_view, base *> sender;
	std::multimap<string_view, base *> state_key;
	std::multimap<string_view, base *> type;
	std::vector<base *> always;

	size_t match(const event &match, const std::function<bool (base &)> &) const;
	size_t add(base &hook, const event &matching);
	size_t del(base &hook, const event &matching);

	maps();
	~maps() noexcept;
};

ircd::m::hook::maps::maps()
{
}

ircd::m::hook::maps::~maps()
noexcept
{
}

size_t
ircd::m::hook::maps::add(base &hook,
                         const event &matching)
{
	size_t ret{0};
	const auto map{[&hook, &ret]
	(auto &map, const string_view &value)
	{
		map.emplace(value, &hook);
		++ret;
	}};

	if(json::get<"origin"_>(matching))
		map(origin, at<"origin"_>(matching));

	if(json::get<"room_id"_>(matching))
		map(room_id, at<"room_id"_>(matching));

	if(json::get<"sender"_>(matching))
		map(sender, at<"sender"_>(matching));

	if(json::get<"state_key"_>(matching))
		map(state_key, at<"state_key"_>(matching));

	if(json::get<"type"_>(matching))
		map(type, at<"type"_>(matching));

	// Hook had no mappings which means it will match everything.
	// We don't increment the matcher count for this case.
	if(!ret)
		always.emplace_back(&hook);

	return ret;
}

size_t
ircd::m::hook::maps::del(base &hook,
                         const event &matching)
{
	size_t ret{0};
	const auto unmap{[&hook, &ret]
	(auto &map, const string_view &value)
	{
		auto pit{map.equal_range(value)};
		while(pit.first != pit.second)
			if(pit.first->second == &hook)
			{
				pit.first = map.erase(pit.first);
				++ret;
			}
			else ++pit.first;
	}};

	// Unconditional attempt to remove from always.
	std::remove(begin(always), end(always), &hook);

	if(json::get<"origin"_>(matching))
		unmap(origin, at<"origin"_>(matching));

	if(json::get<"room_id"_>(matching))
		unmap(room_id, at<"room_id"_>(matching));

	if(json::get<"sender"_>(matching))
		unmap(sender, at<"sender"_>(matching));

	if(json::get<"state_key"_>(matching))
		unmap(state_key, at<"state_key"_>(matching));

	if(json::get<"type"_>(matching))
		unmap(type, at<"type"_>(matching));

	return ret;
}

size_t
ircd::m::hook::maps::match(const event &event,
                           const std::function<bool (base &)> &callback)
const
{
	std::set<base *> matching
	{
		begin(always), end(always)
	};

	const auto site_match{[&matching]
	(auto &map, const string_view &key)
	{
		auto pit{map.equal_range(key)};
		for(; pit.first != pit.second; ++pit.first)
			matching.emplace(pit.first->second);
	}};

	if(json::get<"origin"_>(event))
		site_match(origin, at<"origin"_>(event));

	if(json::get<"room_id"_>(event))
		site_match(room_id, at<"room_id"_>(event));

	if(json::get<"sender"_>(event))
		site_match(sender, at<"sender"_>(event));

	if(json::get<"type"_>(event))
		site_match(type, at<"type"_>(event));

	if(json::get<"state_key"_>(event))
		site_match(state_key, at<"state_key"_>(event));

	auto it(begin(matching));
	while(it != end(matching))
	{
		const base &hook(**it);
		if(!_hook_match(hook.matching, event))
			it = matching.erase(it);
		else
			++it;
	}

	size_t ret{0};
	for(auto it(begin(matching)); it != end(matching); ++it, ++ret)
		if(!callback(**it))
			return ret;

	return ret;
}

//
// hook::base
//

/// Instance list linkage for all hooks
template<>
decltype(ircd::util::instance_list<ircd::m::hook::base>::allocator)
ircd::util::instance_list<ircd::m::hook::base>::allocator
{};

template<>
decltype(ircd::util::instance_list<ircd::m::hook::base>::list)
ircd::util::instance_list<ircd::m::hook::base>::list
{
	allocator
};

/// Primary hook ctor
ircd::m::hook::base::base(const json::members &members)
try
:_feature
{
	_hook_make_feature(members)
}
,feature
{
	_feature
}
,matching
{
	feature
}
{
	site *site;
	if((site = find_site()))
		site->add(*this);
}
catch(...)
{
	if(!registered)
		throw;

	auto *const site(find_site());
	assert(site != nullptr);
	site->del(*this);
}

ircd::m::hook::base::~base()
noexcept
{
	if(!registered)
		return;

	auto *const site(find_site());
	assert(site != nullptr);
	site->del(*this);
}

ircd::m::hook::base::site *
ircd::m::hook::base::find_site()
const
{
	const auto &site_name
	{
		this->site_name()
	};

	if(!site_name)
		return nullptr;

	for(auto *const &site : m::hook::base::site::list)
		if(site->name() == site_name)
			return site;

	return nullptr;
}

ircd::string_view
ircd::m::hook::base::site_name()
const try
{
	return unquote(feature.at("_site"));
}
catch(const std::out_of_range &e)
{
	throw panic
	{
		"Hook %p must name a '_site' to register with.", this
	};
}

//
// hook::site
//

/// Instance list linkage for all hook sites
template<>
decltype(ircd::util::instance_list<ircd::m::hook::base::site>::allocator)
ircd::util::instance_list<ircd::m::hook::base::site>::allocator
{};

template<>
decltype(ircd::util::instance_list<ircd::m::hook::base::site>::list)
ircd::util::instance_list<ircd::m::hook::base::site>::list
{
	allocator
};

//
// hook::site::site
//

ircd::m::hook::base::site::site(const json::members &members)
:_feature
{
	members
}
,feature
{
	_feature
}
,maps
{
	std::make_unique<struct maps>()
}
,exceptions
{
	feature.get<bool>("exceptions", true)
}
{
	for(const auto &site : list)
		if(site->name() == name() && site != this)
			throw error
			{
				"Hook site '%s' already registered at %p",
				name(),
				site
			};

	// Find and register all of the orphan hooks which were constructed before
	// this site was constructed.
	for(auto *const &hook : m::hook::base::list)
		if(hook->site_name() == name())
			add(*hook);
}

ircd::m::hook::base::site::~site()
noexcept
{
	const std::vector<base *> hooks
	{
		begin(this->hooks), end(this->hooks)
	};

	for(auto *const hook : hooks)
		del(*hook);
}

void
ircd::m::hook::base::site::match(const event &event,
                                 const std::function<bool (base &)> &callback)
{
	maps->match(event, callback);
}

bool
ircd::m::hook::base::site::add(base &hook)
{
	assert(!hook.registered);
	assert(hook.site_name() == name());
	assert(hook.matchers == 0);

	if(!hooks.emplace(&hook).second)
	{
		log::warning
		{
			"Hook %p already registered to site %s", &hook, name()
		};

		return false;
	}

	assert(maps);
	const size_t matched
	{
		maps->add(hook, hook.matching)
	};

	hook.matchers = matched;
	hook.registered = true;
	matchers += matched;
	++count;
	return true;
}

bool
ircd::m::hook::base::site::del(base &hook)
{
	assert(hook.registered);
	assert(hook.site_name() == name());

	const size_t matched
	{
		maps->del(hook, hook.matching)
	};

	const auto erased
	{
		hooks.erase(&hook)
	};

	hook.matchers -= matched;
	hook.registered = false;
	matchers -= matched;
	--count;
	assert(hook.matchers == 0);
	assert(erased);
	return true;
}

ircd::string_view
ircd::m::hook::base::site::name()
const try
{
	return unquote(feature.at("name"));
}
catch(const std::out_of_range &e)
{
	throw panic
	{
		"Hook site %p requires a name", this
	};
}

//
// hook<void>
//

ircd::m::hook::hook<void>::hook(const json::members &feature,
                                decltype(function) function)
:base{feature}
,function{std::move(function)}
{
}

ircd::m::hook::hook<void>::hook(decltype(function) function,
                                const json::members &feature)
:base{feature}
,function{std::move(function)}
{
}

ircd::m::hook::site<void>::site(const json::members &feature)
:base::site{feature}
{
}

void
ircd::m::hook::site<void>::operator()(const event &event)
{
	match(event, [this, &event]
	(base &base)
	{
		call(dynamic_cast<hook<void> &>(base), event);
		return true;
	});
}

void
ircd::m::hook::site<void>::call(hook<void> &hfn,
                                const event &event)
try
{
	++hfn.calls;
	hfn.function(event);
}
catch(const std::exception &e)
{
	if(exceptions)
		throw;

	log::critical
	{
		"Unhandled hookfn(%p) %s error :%s",
		&hfn,
		string_view{hfn.feature},
		e.what()
	};
}

//
// hook internal
//

/// Internal interface which manipulates the initializer supplied by the
/// developer to the hook to create the proper JSON output. i.e They supply
/// a "room_id" of "!config" which has no hostname, that is added here
/// depending on my_host() in the deployment runtime...
///
ircd::json::strung
ircd::m::_hook_make_feature(const json::members &members)
{
	const ctx::critical_assertion ca;
	std::vector<json::member> copy
	{
		begin(members), end(members)
	};

	for(auto &member : copy) switch(hash(member.first))
	{
		case hash("room_id"):
			_hook_fix_room_id(members, member);
			continue;

		case hash("sender"):
			_hook_fix_sender(members, member);
			continue;

		case hash("state_key"):
			_hook_fix_state_key(members, member);
			continue;
	}

	return { copy.data(), copy.data() + copy.size() };
}

void
ircd::m::_hook_fix_sender(const json::members &members,
                          json::member &member)
{
	// Rewrite the sender if the supplied input has no hostname
	if(valid_local_only(id::USER, member.second))
	{
		assert(my_host());
		thread_local char buf[256];
		member.second = id::user { buf, member.second, my_host() };
	}

	validate(id::USER, member.second);
}

void
ircd::m::_hook_fix_room_id(const json::members &members,
                           json::member &member)
{
	// Rewrite the room_id if the supplied input has no hostname
	if(valid_local_only(id::ROOM, member.second))
	{
		assert(my_host());
		thread_local char buf[256];
		member.second = id::room { buf, member.second, my_host() };
	}

	validate(id::ROOM, member.second);
}

void
ircd::m::_hook_fix_state_key(const json::members &members,
                             json::member &member)
{
	const bool is_member_event
	{
		end(members) != std::find_if(begin(members), end(members), []
		(const auto &member)
		{
			return member.first == "type" && member.second == "m.room.member";
		})
	};

	// Rewrite the sender if the supplied input has no hostname
	if(valid_local_only(id::USER, member.second))
	{
		assert(my_host());
		thread_local char buf[256];
		member.second = id::user { buf, member.second, my_host() };
	}

	validate(id::USER, member.second);
}

bool
ircd::m::_hook_match(const m::event &matching,
                     const m::event &event)
{
	if(json::get<"origin"_>(matching))
		if(at<"origin"_>(matching) != json::get<"origin"_>(event))
			return false;

	if(json::get<"room_id"_>(matching))
		if(at<"room_id"_>(matching) != json::get<"room_id"_>(event))
			return false;

	if(json::get<"sender"_>(matching))
		if(at<"sender"_>(matching) != json::get<"sender"_>(event))
			return false;

	if(json::get<"type"_>(matching))
		if(at<"type"_>(matching) != json::get<"type"_>(event))
			return false;

	if(json::get<"state_key"_>(matching))
		if(at<"state_key"_>(matching) != json::get<"state_key"_>(event))
			return false;

	if(membership(matching))
		if(membership(matching) != membership(event))
			return false;

	if(json::get<"content"_>(matching))
		if(json::get<"type"_>(event) == "m.room.message")
			if(at<"content"_>(matching).has("msgtype"))
				if(at<"content"_>(matching).get("msgtype") != json::get<"content"_>(event).get("msgtype"))
					return false;

	return true;
}

///////////////////////////////////////////////////////////////////////////////
//
// m/error.h
//

namespace ircd::m
{
	const std::array<http::header, 1> _error_headers
	{{
		{ "Content-Type", "application/json; charset=utf-8" },
	}};
}

thread_local
decltype(ircd::m::error::fmtbuf)
ircd::m::error::fmtbuf
{};

ircd::m::error::error()
:http::error
{
	http::INTERNAL_SERVER_ERROR
}
{}

ircd::m::error::error(std::string c)
:http::error
{
	http::INTERNAL_SERVER_ERROR, std::move(c)
}
{}

ircd::m::error::error(const http::code &c)
:http::error
{
	c, std::string{}
}
{}

ircd::m::error::error(const http::code &c,
                      const json::members &members)
:error
{
	internal, c, json::strung{members}
}
{}

ircd::m::error::error(const http::code &c,
                      const json::iov &iov)
:error
{
	internal, c, json::strung{iov}
}
{}

ircd::m::error::error(const http::code &c,
                      const json::object &object)
:http::error
{
	c, std::string{object}, vector_view<const http::header>{_error_headers}
}
{}

ircd::m::error::error(internal_t,
                      const http::code &c,
                      const json::strung &object)
:http::error
{
	c, object, vector_view<const http::header>{_error_headers}
}
{}
