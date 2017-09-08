/*
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <ircd/m.h>

///////////////////////////////////////////////////////////////////////////////
//
// m/session.h
//

ircd::m::session::session(const host_port &host_port)
:client{host_port}
{
}

ircd::json::object
ircd::m::session::operator()(parse::buffer &pb,
                             request &r)
{
	parse::capstan pc
	{
		pb, read_closure(*this)
	};

	http::request
	{
		host(remote_addr(*this)),
		r.method,
		r.path,
		r.query,
		std::string(r),
		write_closure(*this),
		{
			{ "Content-Type"s, "application/json"s }
		}
	};

	http::code status;
	json::object object;
	http::response
	{
		pc,
		nullptr,
		[&pc, &status, &object](const http::response::head &head)
		{
			status = http::status(head.status);
			object = http::response::content{pc, head};
		}
	};

	if(status < 200 || status >= 300)
		throw m::error(status, object);

	return object;
}

///////////////////////////////////////////////////////////////////////////////
//
// m.h
//

namespace ircd {
namespace m    {

std::map<std::string, ircd::module> modules;
ircd::listener *listener;

void bootstrap();

} // namespace m
} // namespace ircd

ircd::m::init::init()
{
	if(db::sequence(*events::events) == 0)
		bootstrap();

	for(const auto &name : mods::available())
		if(startswith(name, "client_"))
			modules.emplace(name, name);

	modules.emplace("root.so"s, "root.so"s);

	//TODO: conf obviously
	listener = new ircd::listener
	{
		json::string(json::members
		{
			{ "name", "Chat Matrix" },
			{ "host", "0.0.0.0" },
			{ "port", 8447 },
			{ "ssl_certificate_file", "/home/jason/zemos.net.tls.crt" },
			{ "ssl_certificate_chain_file", "/home/jason/zemos.net.tls.crt" },
			{ "ssl_tmp_dh_file", "/home/jason/zemos.net.tls.dh" },
			{ "ssl_private_key_file_pem", "/home/jason/zemos.net.tls.key" },
		})
	};
}

ircd::m::init::~init()
noexcept
{
	delete listener;
	modules.clear();
}

void
ircd::m::bootstrap()
{
	assert(events::events);
	assert(db::sequence(*events::events) == 0);

	ircd::log::notice
	(
		"This appears to be your first time running IRCd because the events "
		"database is empty. I will be bootstrapping it with initial events now..."
	);

	// ircd.create event
	const m::id::id::room::buf room_id{"ircd", "localhost"};
	const m::id::id::user::buf user_id{"ircd", "localhost"};
	{
		const auto type{"m.room.create"};

		char content[512];
		print(content, sizeof(content), json::members
		{
            { "creator", user_id },
		});

		m::event event
		{
			{ "room_id",           room_id            },
			{ "type",              type               },
			{ "state_key",         ""                 },
			{ "sender",            user_id            },
			{ "content",           content            },
		};

		m::events::insert(event);
	}

	{
		const auto type{"m.room.member"};

		char content[512];
		print(content, sizeof(content), json::members
		{
            { "membership",     "join"     },
		});

		m::event event
		{
			{ "room_id",           room_id            },
			{ "type",              type               },
			{ "state_key",         user_id            },
			{ "sender",            user_id            },
			{ "content",           content            },
		};

		m::events::insert(event);
	}
}

///////////////////////////////////////////////////////////////////////////////
//
// m/db.h
//

namespace ircd::m::dbs
{
	std::map<std::string, ircd::module> modules;
	std::map<std::string, import_shared<database>> databases;

	void init_modules();
	void init_databases();
}

ircd::m::dbs::init::init()
{
	init_modules();
	init_databases();

	ircd::m::events::events = databases.at("events").get();
}

ircd::m::dbs::init::~init()
noexcept
{
	ircd::m::events::events = nullptr;

	databases.clear();
	modules.clear();
}

void
ircd::m::dbs::init_databases()
{
	for(const auto &pair : modules)
	{
		const auto &name(pair.first);
		const auto dbname(mods::unpostfixed(name));
		const std::string shortname(lstrip(dbname, "db_"));
		const std::string symname(shortname + "_database"s);
		databases.emplace(shortname, import_shared<database>
		{
			dbname, symname
		});
	}
}

void
ircd::m::dbs::init_modules()
{
	for(const auto &name : mods::available())
		if(startswith(name, "db_"))
			modules.emplace(name, name);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/events.h
//

namespace ircd::m::events
{
	void write(const event &);
}

ircd::database *ircd::m::events::events;

void
ircd::m::events::insert(json::builder &builder)
{
	const id::event::buf generated_event_id
	{
		builder.has("event_id")? id::event::buf{} : id::event::buf{id::generate, "cdc.z"}
	};

	const json::builder::add event_id
	{
		builder, { "event_id", generated_event_id }
	};

	const json::builder::set event_id2
	{
		builder, { "event_id", generated_event_id }
	};

	const json::builder::add origin_server_ts
	{
		builder, { "origin_server_ts", time<milliseconds>() }
	};

	insert(event{builder});
}

void
ircd::m::events::insert(const event &event)
{
	if(!json::val<name::type>(event))
		throw BAD_JSON("Required event field: '%s'", name::type);

	if(!json::val<name::sender>(event))
		throw BAD_JSON("Required event field: '%s'", name::sender);

	if(!json::val<name::event_id>(event))
		throw BAD_JSON("Required event field: '%s'", name::event_id);

	if(!json::val<name::origin_server_ts>(event))
		throw BAD_JSON("Required event field: '%s'", name::origin_server_ts);

	for(const auto &transition : events::transitions)
		if(!transition->valid(event))
			throw INVALID_TRANSITION("Event insertion refused: '%s'",
			                         transition->name);

	write(event);
}

ircd::m::events::const_iterator
ircd::m::events::find(const id &id)
{
	cursor cur{"!room_id$event_id"};
	return cur.find(id);
}

ircd::m::events::const_iterator
ircd::m::events::begin()
{
	cursor cur{"!room_id$event_id"};
	return cur.begin();
}

ircd::m::events::const_iterator
ircd::m::events::end()
{
	cursor cur{"!room_id$event_id"};
	return cur.end();
}

ircd::m::events::transition_list
ircd::m::events::transitions
{};

ircd::m::events::transition::transition(const char *const &name)
:name{name}
,it
{
	events::transitions, events::transitions.emplace(events::transitions.end(), this)
}
{
}

ircd::m::events::transition::transition(const char *const &name,
                                        struct method method)
:name{name}
,method{std::move(method)}
,it
{
	events::transitions, events::transitions.emplace(events::transitions.end(), this)
}
{
}

ircd::m::events::transition::~transition()
noexcept
{
}

bool
ircd::m::events::transition::valid(const event &event)
const
{
	return method.valid(event);
}

void
ircd::m::events::transition::effects(const event &event)
{
	method.effects(event);
}

void
ircd::m::events::write(const event &event)
{
	const auto &master_index
	{
		at<name::event_id>(event)
	};

	constexpr const size_t num_indexes(1);
	constexpr const size_t num_deltas
	{
		event.size() + num_indexes
	};

	size_t i(0);
	db::delta deltas[num_deltas];
	for_each(event, [&i, &deltas, &master_index]
	(const auto &key, const auto &val)
	{
		deltas[i++] =
		{
			key,                 // col
			master_index,        // key
			byte_view<>{val},    // val
		};
	});

	char buf[id::event::buf::SIZE + id::room::buf::SIZE];
	if(!json::val<name::room_id>(event).empty())
	{
		strlcpy(buf, json::val<name::room_id>(event).data(), sizeof(buf));
		strlcat(buf, json::val<name::event_id>(event).data(), sizeof(buf));
		deltas[i++] = db::delta
		{
			"!room_id$event_id",  // col
			buf,                  // key
		};
	}

	(*events)(begin(deltas), begin(deltas) + i);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/room.h
//

void
ircd::m::room::join(const m::id::user &user_id,
                    json::builder &content)
{
	json::builder::set membership_join
	{
		content, { "membership", "join" }
	};

	membership(user_id, content);
}

void
ircd::m::room::membership(const m::id::user &user_id,
                          json::builder &content)
{
	if(is_member(user_id, content.at("membership")))
		throw m::ALREADY_MEMBER
		{
			"Already a member with this membership."
		};

	char buffer[512];
	const auto printed_content
	{
		stringify(buffer, content)
	};

	json::builder event;
	json::builder::set fields{event,
	{
		{ "room_id",      room_id          },
		{ "type",         "m.room.member"  },
		{ "state_key",    user_id          },
		{ "sender",       user_id          },
		{ "content",      printed_content  }
	}};

	m::events::insert(event);
}

bool
ircd::m::room::is_member(const m::id::user &user_id,
                         const string_view &membership)
{
	const m::events::where::equal query
	{
		{ "type",        "m.room.member" },
		{ "state_key",    user_id        }
	};

	return count(query, [](const auto &event)
	{
		const json::object &content
		{
			json::val<m::name::content>(event)
		};

		const auto &membership
		{
			unquote(content["membership"])
		};

		return membership == membership;
	});
}

bool
ircd::m::room::any(const events::where &where)
const
{
	events::cursor cursor{"!room_id$event_id"};
	cursor.where = &where;
	return bool(cursor.find(room_id));
}

bool
ircd::m::room::any(const events::where &where,
                   const event_closure_bool &closure)
const
{
	events::cursor cursor{"!room_id$event_id"};
	cursor.where = &where;

	for(auto it(cursor.find(room_id)); bool(it); ++it)
		if(closure(*it))
			return true;

	return false;
}

size_t
ircd::m::room::count(const events::where &where)
const
{
	events::cursor cursor{"!room_id$event_id"};
	cursor.where = &where;

	size_t i(0);
	for(auto it(cursor.find(room_id)); bool(it); ++it, i++)
	return i;
}

size_t
ircd::m::room::count(const events::where &where,
                     const event_closure_bool &closure)
const
{
	events::cursor cursor{"!room_id$event_id"};
	cursor.where = &where;

	size_t i(0);
	for(auto it(cursor.find(room_id)); bool(it); ++it)
	{
		const m::event &e(*it);
		i += closure(e);
	}

	return i;
}

void
ircd::m::room::for_each(const events::where &where,
                        const event_closure &closure)
const
{
	events::cursor cursor{"!room_id$event_id"};
	cursor.where = &where;
	auto it(cursor.find(room_id));
	if(!it)
		return;

	for(; bool(it); ++it)
		closure(*it);
}

void
ircd::m::room::for_each(const event_closure &closure)
const
{
	const m::events::where::noop where{};
	for_each(where, closure);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/id.h
//

ircd::m::id::id(const string_view &id)
:string_view{id}
,sigil{m::sigil(id)}
{
}

ircd::m::id::id(const enum sigil &sigil,
                const string_view &id)
:string_view{id}
,sigil{sigil}
{
	if(!valid())
		throw INVALID_MXID("Not a valid '%s' mxid", reflect(sigil));
}

ircd::m::id::id(const enum sigil &sigil,
                char *const &buf,
                const size_t &max,
                const string_view &id)
:string_view{buf}
,sigil{sigil}
{
	strlcpy(buf, id, max);
}

ircd::m::id::id(const enum sigil &sigil,
                char *const &buf,
                const size_t &max,
                const string_view &name,
                const string_view &host)
:string_view{[&]() -> string_view
{
	if(!max)
		return {};

	size_t len(0);
	if(!startswith(name, sigil))
		buf[len++] = char(sigil);

	const auto has_sep
	{
		std::count(std::begin(name), std::end(name), ':')
	};

	if(!has_sep && host.empty())
	{
		len += strlcpy(buf + len, name, max - len);
	}
	else if(!has_sep && !host.empty())
	{
		len += fmt::snprintf(buf + len, max - len, "%s:%s",
		                     name,
		                     host);
	}
	else if(has_sep == 1 && !host.empty() && !split(name, ':').second.empty())
	{
		len += strlcpy(buf + len, name, max - len);
	}
	else if(has_sep && !host.empty())
	{
		throw INVALID_MXID("Not a valid '%s' mxid", reflect(sigil));
	}

	return { buf, len };
}()}
,sigil{sigil}
{
}

ircd::m::id::id(const enum sigil &sigil,
                char *const &buf,
                const size_t &max,
                const generate_t &,
                const string_view &host)
:string_view{[&]
{
	char name[64]; switch(sigil)
	{
		case sigil::USER:
			generate_random_prefixed(sigil::USER, "guest", name, sizeof(name));
			break;

		case sigil::ALIAS:
			generate_random_prefixed(sigil::ALIAS, "", name, sizeof(name));
			break;

		default:
			generate_random_timebased(sigil, name, sizeof(name));
			break;
	};

	const size_t len
	{
		fmt::snprintf(buf, max, "%s:%s",
		              name,
		              host)
	};

	return string_view { buf, len };
}()}
,sigil{sigil}
{
}

bool
ircd::m::id::valid()
const
{
	const auto parts(split(*this, ':'));
	const auto &local(parts.first);
	const auto &host(parts.second);

	// this valid() requires a full canonical mxid with a host
	if(host.empty())
		return false;

	// local requires a sigil plus at least one character
	if(local.size() < 2)
		return false;

	// local requires the correct sigil type
	if(!startswith(local, sigil))
		return false;

	return true;
}

bool
ircd::m::id::valid_local()
const
{
	const auto parts(split(*this, ':'));
	const auto &local(parts.first);

	// local requires a sigil plus at least one character
	if(local.size() < 2)
		return false;

	// local requires the correct sigil type
	if(!startswith(local, sigil))
		return false;

	return true;
}

ircd::string_view
ircd::m::id::generate_random_prefixed(const enum sigil &sigil,
                                      const string_view &prefix,
                                      char *const &buf,
                                      const size_t &max)
{
	const uint32_t num(rand::integer());
	const size_t len(fmt::snprintf(buf, max, "%c%s%u", char(sigil), prefix, num));
	return { buf, len };
}

ircd::string_view
ircd::m::id::generate_random_timebased(const enum sigil &sigil,
                                       char *const &buf,
                                       const size_t &max)
{
	const auto utime(microtime());
	const size_t len(snprintf(buf, max, "%c%zd%06d", char(sigil), utime.first, utime.second));
	return { buf, len };
}

enum ircd::m::id::sigil
ircd::m::sigil(const string_view &s)
try
{
	return sigil(s.at(0));
}
catch(const std::out_of_range &e)
{
	throw BAD_SIGIL("sigil undefined");
}

enum ircd::m::id::sigil
ircd::m::sigil(const char &c)
{
	switch(c)
	{
		case '$':  return id::EVENT;
		case '@':  return id::USER;
		case '#':  return id::ALIAS;
		case '!':  return id::ROOM;
	}

	throw BAD_SIGIL("'%c' is not a valid sigil", c);
}

const char *
ircd::m::reflect(const enum id::sigil &c)
{
	switch(c)
	{
		case id::EVENT:   return "EVENT";
		case id::USER:    return "USER";
		case id::ALIAS:   return "ALIAS";
		case id::ROOM:    return "ROOM";
	}

	return "?????";
}

bool
ircd::m::valid_sigil(const string_view &s)
try
{
	return valid_sigil(s.at(0));
}
catch(const std::out_of_range &e)
{
	return false;
}

bool
ircd::m::valid_sigil(const char &c)
{
	switch(c)
	{
		case id::EVENT:
		case id::USER:
		case id::ALIAS:
		case id::ROOM:
			return true;
	}

	return false;
}
