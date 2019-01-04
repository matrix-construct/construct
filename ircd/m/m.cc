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
                    const string_view &hostname)
try
:_self
{
	origin
}
,_modules
{
	std::make_unique<modules>()
}
{
	presence::set(me, "online", me_online_status_msg);
}
catch(const m::error &e)
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
	if(!std::current_exception())
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
		create(user::users, me.user_id);

	if(!exists(my_room))
		create(my_room, me.user_id);

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
ircd::m::self::host(const string_view &s)
{
	return s == host();
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

ircd::m::self::init::init(const string_view &origin)
{
	self::origin = std::string{origin};

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
// m/sync.h
//

decltype(ircd::m::sync::log)
ircd::m::sync::log
{
	"sync", 's'
};

//
// response
//

struct ircd::m::sync::response
{
	static conf::item<size_t> flush_hiwat;

	sync::stats &stats;
	ircd::client &client;
	unique_buffer<mutable_buffer> buf;
	std::unique_ptr<resource::response::chunked> resp;
	bool committed;

	const_buffer flush(const const_buffer &buf);
	void commit();

	response(sync::stats &stats, ircd::client &client);
	~response() noexcept;
};

decltype(ircd::m::sync::response::flush_hiwat)
ircd::m::sync::response::flush_hiwat
{
    { "name",     "ircd.m.sync.flush.hiwat"  },
    { "default",  long(32_KiB)               },
};

//
// response::response
//

ircd::m::sync::response::response(sync::stats &stats,
                                  ircd::client &client)
:stats
{
	stats
}
,client
{
	client
}
,buf
{
	std::max(size_t(96_KiB), size_t(flush_hiwat))
}
,committed
{
	false
}
{
}

ircd::m::sync::response::~response()
noexcept
{
}

ircd::const_buffer
ircd::m::sync::response::flush(const const_buffer &buf)
{
	if(!committed)
		return buf;

	if(!resp)
		commit();

	stats.flush_bytes += resp->write(buf);
	stats.flush_count++;
	return buf;
}

void
ircd::m::sync::response::commit()
{
	static const string_view content_type
	{
		"application/json; charset=utf-8"
	};

	assert(!resp);
	resp = std::make_unique<resource::response::chunked>
	(
		client, http::OK, content_type
	);
}

//
// data
//

ircd::m::sync::data::data(sync::stats &stats,
                          ircd::client &client,
                          const m::user &user,
                          const std::pair<event::idx, event::idx> &range,
                          const string_view &filter_id)
:stats{stats}
,client{client}
,since
{
	range.first
}
,current
{
	range.second
}
,delta
{
	current - since
}
,user
{
	user
}
,user_room
{
	user
}
,user_rooms
{
	user
}
,filter_buf
{
	filter_id?
		user.filter(std::nothrow, filter_id):
		std::string{}
}
,filter
{
	json::object{filter_buf}
}
,resp
{
	std::make_unique<response>(stats, client)
}
,out
{
	resp->buf, std::bind(&response::flush, resp.get(), ph::_1), size_t(resp->flush_hiwat)
}
{
}

ircd::m::sync::data::~data()
noexcept
{
}

bool
ircd::m::sync::data::commit()
{
	assert(resp);
	const auto ret{resp->committed};
	resp->committed = true;
	return ret;
}

bool
ircd::m::sync::data::committed()
const
{
	assert(resp);
	return resp->committed;
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
                          handle linear)
:instance_multimap
{
	std::move(name)
}
,_polylog
{
	std::move(polylog)
}
,_linear
{
	std::move(linear)
}
{
	log::debug
	{
		log, "Registered sync item(%p) '%s'",
		this,
		this->name()
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
ircd::m::sync::item::linear(data &data,
                            const m::event &event)
try
{
	const scope_restore<decltype(data.event)> theirs
	{
		data.event, &event
	};

	const auto ret
	{
		_linear(data)
	};

	return ret;
}
catch(const std::bad_function_call &e)
{
	thread_local char rembuf[128];
	log::dwarning
	{
		log, "linear %s %s '%s' missing handler :%s",
		string(rembuf, ircd::remote(data.client)),
		string_view{data.user.user_id},
		name(),
		e.what()
	};

	return false;
}

bool
ircd::m::sync::item::polylog(data &data)
try
{
	#ifdef RB_DEBUG
	sync::stats stats{data.stats};
	stats.timer = {};
	#endif

	const auto ret
	{
		_polylog(data)
	};

	#ifdef RB_DEBUG
	thread_local char rembuf[128], iecbuf[64], tmbuf[32];
	log::debug
	{
		log, "polylog %s %s '%s' %s wc:%zu in %s",
		string(rembuf, ircd::remote(data.client)),
		string_view{data.user.user_id},
		name(),
		ircd::pretty(iecbuf, iec(data.stats.flush_bytes - stats.flush_bytes)),
		data.stats.flush_count - stats.flush_count,
		ircd::pretty(tmbuf, stats.timer.at<microseconds>(), true)
	};
	#endif

	return ret;
}
catch(const std::bad_function_call &e)
{
	thread_local char rembuf[128];
	log::dwarning
	{
		log, "polylog %s %s '%s' missing handler :%s",
		string(rembuf, ircd::remote(data.client)),
		string_view{data.user.user_id},
		name(),
		e.what()
	};

	return false;
}
catch(const std::exception &e)
{
	thread_local char rembuf[128], iecbuf[64], tmbuf[32];
	log::derror
	{
		log, "polylog %s %s '%s' %s wc:%zu in %s :%s",
		string(rembuf, ircd::remote(data.client)),
		string_view{data.user.user_id},
		name(),
		ircd::pretty(iecbuf, iec(data.stats.flush_bytes)),
		data.stats.flush_count,
		ircd::pretty(tmbuf, data.stats.timer.at<milliseconds>(), true),
		e.what()
	};

	throw;
}

ircd::string_view
ircd::m::sync::item::name()
const
{
	return this->instance_multimap::it->first;
}

///////////////////////////////////////////////////////////////////////////////
//
// m/feds.h
//

//
// state
//

ircd::m::feds::state::state(const m::room::id &room_id,
                            const m::event::id &event_id,
                            const closure &view)
:state
{
	//TODO: conf
	room_id, event_id, seconds(20), view
}
{
}

ircd::m::feds::state::state(const m::room::id &room_id,
                            const m::event::id &event_id,
                            const milliseconds &to,
                            const closure &view)
{
	using prototype = void (const m::room::id &,
	                        const m::event::id &,
	                        const milliseconds &,
	                        const closure &);

	static mods::import<prototype> feds__state
	{
		"federation_federation", "feds__state"
	};

	feds__state(room_id, event_id, to, view);
}

//
// head
//

ircd::m::feds::head::head(const m::room::id &room_id,
                          const closure &view)
:head
{
	room_id, m::me.user_id, view
}
{
}

ircd::m::feds::head::head(const m::room::id &room_id,
                          const m::user::id &user_id,
                          const closure &view)
:head
{
	//TODO: conf
	room_id, user_id, seconds(20), view
}
{
}

ircd::m::feds::head::head(const m::room::id &room_id,
                          const m::user::id &user_id,
                          const milliseconds &to,
                          const closure &view)
{
	using prototype = void (const m::room::id &,
	                        const m::user::id &,
	                        const milliseconds &,
	                        const closure &);

	static mods::import<prototype> feds__head
	{
		"federation_federation", "feds__head"
	};

	feds__head(room_id, user_id, to, view);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/vm.h
//

decltype(ircd::m::vm::log)
ircd::m::vm::log
{
	"vm", 'v'
};

decltype(ircd::m::vm::current_sequence)
ircd::m::vm::current_sequence
{};

decltype(ircd::m::vm::uncommitted_sequence)
ircd::m::vm::uncommitted_sequence
{};

decltype(ircd::m::vm::default_opts)
ircd::m::vm::default_opts
{};

decltype(ircd::m::vm::default_copts)
ircd::m::vm::default_copts
{};

const uint64_t &
ircd::m::vm::sequence(const eval &eval)
{
	return eval.sequence;
}

uint64_t
ircd::m::vm::retired_sequence()
{
	event::id::buf event_id;
	return retired_sequence(event_id);
}

uint64_t
ircd::m::vm::retired_sequence(event::id::buf &event_id)
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

ircd::string_view
ircd::m::vm::reflect(const enum fault &code)
{
	switch(code)
	{
		case fault::ACCEPT:       return "ACCEPT";
		case fault::EXISTS:       return "EXISTS";
		case fault::INVALID:      return "INVALID";
		case fault::DEBUGSTEP:    return "DEBUGSTEP";
		case fault::BREAKPOINT:   return "BREAKPOINT";
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
decltype(ircd::util::instance_list<ircd::m::vm::eval>::list)
ircd::util::instance_list<ircd::m::vm::eval>::list
{};

decltype(ircd::m::vm::eval::id_ctr)
ircd::m::vm::eval::id_ctr
{};

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

	static mods::import<prototype> function
	{
		"vm", "eval__commit_room"
	};

	return function(*this, room, event, contents);
}

/// Inject a new event originating from this server.
///
enum ircd::m::vm::fault
ircd::m::vm::eval::operator()(json::iov &event,
                              const json::iov &contents)
{
	using prototype = fault (eval &, json::iov &, const json::iov &);

	static mods::import<prototype> function
	{
		"vm", "eval__commit"
	};

	return function(*this, event, contents);
}

enum ircd::m::vm::fault
ircd::m::vm::eval::operator()(const event &event)
{
	using prototype = fault (eval &, const m::event &);

	static mods::import<prototype> function
	{
		"vm", "eval__event"
	};

	const vm::fault ret
	{
		function(*this, event)
	};

	return ret;
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

	static mods::import<prototype> function
	{
		"m_room_history_visibility", "visible"
	};

	return function(event, mxid);
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
ircd::m::receipt::read(const id::room &room_id,
                       const id::user &user_id,
                       const id::event &event_id,
                       const time_t &ms)
{
	using prototype = event::id::buf (const id::room &, const id::user &, const id::event &, const time_t &);

	static mods::import<prototype> function
	{
		"client_rooms", "commit__m_receipt_m_read"
	};

	return function(room_id, user_id, event_id, ms);
}

ircd::m::event::id
ircd::m::receipt::read(id::event::buf &out,
                       const id::room &room_id,
                       const id::user &user_id)
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
ircd::m::receipt::read(const id::room &room_id,
                       const id::user &user_id,
                       const event::id::closure &closure)
{
	using prototype = bool (const id::room &, const id::user &, const id::event::closure &);

	static mods::import<prototype> function
	{
		"m_receipt", "last_receipt__event_id"
	};

	return function(room_id, user_id, closure);
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
		"m_typing", "for_each"
	};

	return function(closure);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/presence.h
//

ircd::m::presence::presence(const user &user,
                            const mutable_buffer &buf)
:presence
{
	get(user, buf)
}
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
		"m_presence", "commit__m_presence"
	};

	return function(object);
}

ircd::json::object
ircd::m::presence::get(const user &user,
                       const mutable_buffer &buffer)
{
	json::object ret;
	get(std::nothrow, user, [&ret, &buffer]
	(const json::object &object)
	{
		ret = { data(buffer), copy(buffer, object) };
	});

	return ret;
}

void
ircd::m::presence::get(const user &user,
                       const closure &closure)
{
	get(user, [&closure]
	(const m::event &event, const json::object &content)
	{
		closure(content);
	});
}

void
ircd::m::presence::get(const user &user,
                       const event_closure &closure)
{
	if(!get(std::nothrow, user, closure))
		throw m::NOT_FOUND
		{
			"No presence found for %s",
			string_view{user.user_id}
		};
}

bool
ircd::m::presence::get(std::nothrow_t,
                       const user &user,
                       const closure &closure)
{
	return get(std::nothrow, user, [&closure]
	(const m::event &event, const json::object &content)
	{
		closure(content);
	});
}

bool
ircd::m::presence::get(std::nothrow_t,
                       const user &user,
                       const event_closure &closure)
{
	using prototype = bool (std::nothrow_t, const m::user &, const event_closure &);

	static mods::import<prototype> function
	{
		"m_presence", "get__m_presence"
	};

	return function(std::nothrow, user, closure);
}

bool
ircd::m::presence::valid_state(const string_view &state)
{
	using prototype = bool (const string_view &);

	static mods::import<prototype> function
	{
		"m_presence", "presence_valid_state"
	};

	return function(state);
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
ircd::m::events::rfor_each(const event::idx &start,
                           const event_filter &filter,
                           const closure_bool &closure)
{
	auto limit
	{
		json::get<"limit"_>(filter)?: 32L
	};

	return rfor_each(start, [&filter, &closure, &limit]
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
ircd::m::events::rfor_each(const event::idx &start,
                           const closure_bool &closure)
{
	event::fetch event;
	return rfor_each(start, id_closure_bool{[&event, &closure]
	(const event::idx &event_idx, const event::id &event_id)
	{
		if(!seek(event, event_idx, std::nothrow))
			return true;

		return closure(event_idx, event);
	}});
}

bool
ircd::m::events::rfor_each(const event::idx &start,
                           const id_closure_bool &closure)
{
	static const db::gopts opts
	{
		db::get::NO_CACHE
	};

	static constexpr auto column_idx
	{
		json::indexof<event, "event_id"_>()
	};

	auto &column
	{
		dbs::event_column.at(column_idx)
	};

	if(start == uint64_t(-1))
	{
		for(auto it(column.rbegin(opts)); it; ++it)
			if(!closure(byte_view<event::idx>(it->first), it->second))
				return false;

		return true;
	}

	auto it
	{
		column.lower_bound(byte_view<string_view>(start), opts)
	};

	for(; it; --it)
		if(!closure(byte_view<event::idx>(it->first), it->second))
			return false;

	return true;
}

bool
ircd::m::events::for_each(const event::idx &start,
                          const event_filter &filter,
                          const closure_bool &closure)
{
	auto limit
	{
		json::get<"limit"_>(filter)?: 32L
	};

	return for_each(start, [&filter, &closure, &limit]
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
ircd::m::events::for_each(const event::idx &start,
                          const closure_bool &closure)
{
	event::fetch event;
	return for_each(start, id_closure_bool{[&event, &closure]
	(const event::idx &event_idx, const event::id &event_id)
	{
		if(!seek(event, event_idx, std::nothrow))
			return true;

		return closure(event_idx, event);
	}});
}

bool
ircd::m::events::for_each(const event::idx &start,
                          const id_closure_bool &closure)
{
	static const db::gopts opts
	{
		db::get::NO_CACHE
	};

	static constexpr auto column_idx
	{
		json::indexof<event, "event_id"_>()
	};

	auto &column
	{
		dbs::event_column.at(column_idx)
	};

	auto it
	{
		start > 0?
			column.lower_bound(byte_view<string_view>(start), opts):
			column.begin(opts)
	};

	for(; it; ++it)
		if(!closure(byte_view<event::idx>(it->first), it->second))
			return false;

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

ircd::m::filter::filter(const user &user,
                        const string_view &filter_id,
                        const mutable_buffer &buf)
{
	get(user, filter_id, [this, &buf]
	(const json::object &filter)
	{
		const size_t len
		{
			copy(buf, string_view{filter})
		};

		new (this) m::filter
		{
			json::object
			{
				data(buf), len
			}
		};
	});
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

void
ircd::m::rooms::for_each(const user &user,
                         const user::rooms::closure &closure)
{
	const m::user::rooms rooms{user};
	rooms.for_each(closure);
}

bool
ircd::m::rooms::for_each(const user &user,
                         const user::rooms::closure_bool &closure)
{
	const m::user::rooms rooms{user};
	return rooms.for_each(closure);
}

void
ircd::m::rooms::for_each(const user &user,
                         const string_view &membership,
                         const user::rooms::closure &closure)
{
	const m::user::rooms rooms{user};
	rooms.for_each(membership, closure);
}

bool
ircd::m::rooms::for_each(const user &user,
                         const string_view &membership,
                         const user::rooms::closure_bool &closure)
{
	const m::user::rooms rooms{user};
	return rooms.for_each(membership, closure);
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

bool
ircd::m::rooms::for_each_public(const room::id::closure_bool &closure)
{
	return for_each_public(string_view{}, closure);
}

bool
ircd::m::rooms::for_each_public(const string_view &key,
                                const room::id::closure_bool &closure)
{
	using prototype = bool (const string_view &, const room::id::closure_bool &);

	static mods::import<prototype> function
	{
		"m_rooms", "_for_each_public"
	};

	return function(key, closure);
}

void
ircd::m::rooms::for_each(const room::closure &closure)
{
	for_each(room::closure_bool{[&closure]
	(const room &room)
	{
		closure(room);
		return true;
	}});
}

bool
ircd::m::rooms::for_each(const room::closure_bool &closure)
{
	return for_each(room::id::closure_bool{[&closure]
	(const m::room::id &room_id)
	{
		return closure(room_id);
	}});
}

void
ircd::m::rooms::for_each(const room::id::closure &closure)
{
	for_each(string_view{}, [&closure]
	(const m::room::id &room_id)
	{
		closure(room_id);
		return true;
	});
}

bool
ircd::m::rooms::for_each(const room::id::closure_bool &closure)
{
	return for_each(string_view{}, closure);
}

bool
ircd::m::rooms::for_each(const string_view &room_id_lb,
                         const room::id::closure_bool &closure)
{
	using prototype = bool (const string_view &, const room::id::closure_bool &);

	static mods::import<prototype> function
	{
		"m_rooms", "_for_each"
	};

	return function(room_id_lb, closure);
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
ircd::m::user::filter(const json::object &filter,
                      const mutable_buffer &idbuf)
{
	using prototype = event::id::buf (const m::user &, const json::object &, const mutable_buffer &);

	static mods::import<prototype> function
	{
		"client_user", "filter_set"
	};

	return function(*this, filter, idbuf);
}

std::string
ircd::m::user::filter(const string_view &filter_id)
const
{
	std::string ret;
	filter(filter_id, [&ret]
	(const json::object &filter)
	{
		ret.assign(data(filter), size(filter));
	});

	return ret;
}

std::string
ircd::m::user::filter(std::nothrow_t,
                      const string_view &filter_id)
const
{
	std::string ret;
	filter(std::nothrow, filter_id, [&ret]
	(const json::object &filter)
	{
		ret.assign(data(filter), size(filter));
	});

	return ret;
}

void
ircd::m::user::filter(const string_view &filter_id,
                      const filter_closure &closure)
const
{
	if(!filter(std::nothrow, filter_id, closure))
		throw m::NOT_FOUND
		{
			"Filter '%s' not found", filter_id
		};
}

bool
ircd::m::user::filter(std::nothrow_t,
                      const string_view &filter_id,
                      const filter_closure &closure)
const
{
	using prototype = bool (std::nothrow_t, const m::user &, const string_view &, const filter_closure &);

	static mods::import<prototype> function
	{
		"client_user", "filter_get"
	};

	return function(std::nothrow, *this, filter_id, closure);
}

ircd::m::event::id::buf
ircd::m::user::account_data(const m::room &room,
                            const m::user &sender,
                            const string_view &type,
                            const json::object &val)
{
	using prototype = event::id::buf (const m::user &, const m::room &, const m::user &, const string_view &, const json::object &);

	static mods::import<prototype> function
	{
		"client_user", "room_account_data_set"
	};

	return function(*this, room, sender, type, val);
}

ircd::m::event::id::buf
ircd::m::user::account_data(const m::user &sender,
                            const string_view &type,
                            const json::object &val)
{
	using prototype = event::id::buf (const m::user &, const m::user &, const string_view &, const json::object &);

	static mods::import<prototype> function
	{
		"client_user", "account_data_set"
	};

	return function(*this, sender, type, val);
}

ircd::json::object
ircd::m::user::account_data(const mutable_buffer &out,
                            const m::room &room,
                            const string_view &type)
const
{
	json::object ret;
	account_data(std::nothrow, room, type, [&out, &ret]
	(const json::object &val)
	{
		ret = string_view { data(out), copy(out, val) };
	});

	return ret;
}

ircd::json::object
ircd::m::user::account_data(const mutable_buffer &out,
                            const string_view &type)
const
{
	json::object ret;
	account_data(std::nothrow, type, [&out, &ret]
	(const json::object &val)
	{
		ret = string_view { data(out), copy(out, val) };
	});

	return ret;
}

bool
ircd::m::user::account_data(std::nothrow_t,
                            const m::room &room,
                            const string_view &type,
                            const account_data_closure &closure)
const try
{
	account_data(room, type, closure);
	return true;
}
catch(const std::exception &e)
{
	return false;
}

bool
ircd::m::user::account_data(std::nothrow_t,
                            const string_view &type,
                            const account_data_closure &closure)
const try
{
	account_data(type, closure);
	return true;
}
catch(const std::exception &e)
{
	return false;
}

void
ircd::m::user::account_data(const m::room &room,
                            const string_view &type,
                            const account_data_closure &closure)
const
{
	using prototype = void (const m::user &, const m::room &, const string_view &, const account_data_closure &);

	static mods::import<prototype> function
	{
		"client_user", "room_account_data_get"
	};

	return function(*this, room, type, closure);
}

void
ircd::m::user::account_data(const string_view &type,
                            const account_data_closure &closure)
const
{
	using prototype = void (const m::user &, const string_view &, const account_data_closure &);

	static mods::import<prototype> function
	{
		"client_user", "account_data_get"
	};

	return function(*this, type, closure);
}

ircd::string_view
ircd::m::user::_account_data_type(const mutable_buffer &out,
                                  const m::room::id &room_id)
{
	using prototype = string_view (const mutable_buffer &, const m::room::id &);

	static mods::import<prototype> function
	{
		"client_user", "room_account_data_type"
	};

	return function(out, room_id);
}

ircd::m::event::id::buf
ircd::m::user::profile(const m::user &sender,
                       const string_view &key,
                       const string_view &val)
{
	using prototype = event::id::buf (const m::user &, const m::user &, const string_view &, const string_view &);

	static mods::import<prototype> function
	{
		"client_profile", "profile_set"
	};

	return function(*this, sender, key, val);
}

ircd::string_view
ircd::m::user::profile(const mutable_buffer &out,
                       const string_view &key)
const
{
	string_view ret;
	profile(std::nothrow, key, [&out, &ret]
	(const string_view &val)
	{
		ret = { data(out), copy(out, val) };
	});

	return ret;
}

bool
ircd::m::user::profile(std::nothrow_t,
                       const string_view &key,
                       const profile_closure &closure)
const try
{
	profile(key, closure);
	return true;
}
catch(const std::exception &e)
{
	return false;
}

void
ircd::m::user::profile(const string_view &key,
                       const profile_closure &closure)
const
{
	using prototype = void (const m::user &, const string_view &, const profile_closure &);

	static mods::import<prototype> function
	{
		"client_profile", "profile_get"
	};

	return function(*this, key, closure);
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
	// Setup the list of event fields to fetch for the closure
	static const event::keys keys
	{
		event::keys::include
		{
			"state_key",
			"content",
		}
    };

	const m::event::fetch::opts fopts
	{
		keys, user_room.fopts? user_room.fopts->gopts : db::gopts{}
	};

	const m::room::state state
	{
		user_room, &fopts
	};

	return state.for_each("ircd.member", event::closure_bool{[&membership, &closure]
	(const m::event &event)
	{
		const string_view &membership_
		{
			unquote(at<"content"_>(event).at("membership"))
		};

		if(membership && membership_ != membership)
			return true;

		const m::room::id &room_id
		{
			at<"state_key"_>(event)
		};

		return closure(room_id, membership);
	}});
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
	(const m::room &room, const string_view &)
	{
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

///////////////////////////////////////////////////////////////////////////////
//
// m/room.h
//

size_t
ircd::m::room::state::prefetch(const event::idx &start,
                               const event::idx &stop)
const
{
	return prefetch(string_view{}, start, stop);
}

size_t
ircd::m::room::state::prefetch(const string_view &type,
                               const event::idx &start,
                               const event::idx &stop)
const
{
	using prototype = size_t (const state &, const string_view &, const std::pair<event::idx, event::idx> &);

	static mods::import<prototype> function
	{
		"m_room", "state__prefetch"
	};

	return function(*this, type, std::pair<event::idx, event::idx>{start, stop});
}

ircd::m::room
ircd::m::create(const id::room &room_id,
                const id::user &creator,
                const string_view &type)
{
	using prototype = room (const id::room &, const id::user &, const string_view &);

	static mods::import<prototype> function
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

	static mods::import<prototype> function
	{
		"client_createroom", "createroom__parent_type"
	};

	return function(room_id, creator, parent, type);
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
	using prototype = event::id::buf (const m::room &, const id::user &, const id::user &);

	static mods::import<prototype> function
	{
		"client_rooms", "invite__room_user"
	};

	return function(room, target, sender);
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
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const string_view &state_key,
              const json::members &contents)
{
	json::iov _content;
	json::iov::push content[contents.size()];
	return send(room, sender, type, state_key, make_iov(_content, content, contents.size(), contents));
}

ircd::m::event::id::buf
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const string_view &state_key,
              const json::object &contents)
{
	json::iov _content;
	json::iov::push content[contents.size()];
	return send(room, sender, type, state_key, make_iov(_content, content, contents.size(), contents));
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
		"client_rooms", "state__iov"
	};

	return function(room, sender, type, state_key, content);
}

ircd::m::event::id::buf
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const json::members &contents)
{
	json::iov _content;
	json::iov::push content[contents.size()];
	return send(room, sender, type, make_iov(_content, content, contents.size(), contents));
}

ircd::m::event::id::buf
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const json::object &contents)
{
	json::iov _content;
	json::iov::push content[contents.size()];
	return send(room, sender, type, make_iov(_content, content, contents.size(), contents));
}

ircd::m::event::id::buf
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const json::iov &content)
{
	using prototype = event::id::buf (const m::room &, const id::user &, const string_view &, const json::iov &);

	static mods::import<prototype> function
	{
		"client_rooms", "send__iov"
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
	using prototype = bool (const room &,
	                        const event::idx &,
	                        const event::idx &);

	static mods::import<prototype> _count_since
	{
		"m_room", "count_since"
	};

	return _count_since(r, std::min(a, b), std::max(a, b));
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

		default:
			return room_id(out, id::room_alias{room_id_or_alias});
	}
}

ircd::m::id::room
ircd::m::room_id(const mutable_buffer &out,
                 const id::room_alias &room_alias)
{
	using prototype = id::room (const mutable_buffer &, const id::room_alias &);

	static mods::import<prototype> function
	{
		"client_directory_room", "room_id__room_alias"
	};

	return function(out, room_alias);
}

bool
ircd::m::exists(const id::room_alias &room_alias,
                const bool &remote_query)
{
	using prototype = bool (const id::room_alias, const bool &);

	static mods::import<prototype> function
	{
		"client_directory_room", "room_alias_exists"
	};

	return function(room_alias, remote_query);
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
decltype(ircd::util::instance_list<ircd::m::hook::base>::list)
ircd::util::instance_list<ircd::m::hook::base>::list
{};

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
	throw assertive
	{
		"Hook %p must name a '_site' to register with.", this
	};
}

//
// hook::site
//

/// Instance list linkage for all hook sites
template<>
decltype(ircd::util::instance_list<ircd::m::hook::base::site>::list)
ircd::util::instance_list<ircd::m::hook::base::site>::list
{};

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
	throw assertive
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
