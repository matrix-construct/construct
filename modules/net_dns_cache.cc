// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::net::dns::cache
{
	static void handle(const m::event &, m::vm::eval &);

	static bool put(const string_view &type, const string_view &state_key, const records &rrs);
	static bool put(const string_view &type, const string_view &state_key, const uint &code, const string_view &msg);

	extern const m::room::id::buf dns_room_id;
	extern m::hookfn<m::vm::eval &> hook;

	static void init(), fini();
}

ircd::mapi::header
IRCD_MODULE
{
	"DNS cache using Matrix rooms.",
	ircd::net::dns::cache::init,
	ircd::net::dns::cache::fini,
};

decltype(ircd::net::dns::cache::dns_room_id)
ircd::net::dns::cache::dns_room_id
{
	"dns", m::my_host()
};

decltype(ircd::net::dns::cache::hook)
ircd::net::dns::cache::hook
{
	ircd::net::dns::cache::handle,
	{
		{ "_site",    "vm.effect"              },
		{ "room_id",  string_view{dns_room_id} },
	}
};

void
ircd::net::dns::cache::init()
{
	log::debug
	{
		"DNS cache room %s currently set.",
		string_view{dns_room_id}
	};
}

void
ircd::net::dns::cache::fini()
{
	if(!waiting.empty())
		log::warning
		{
			log, "Waiting for %zu unfinished cache operations.",
			waiting.size(),
		};

	dock.wait([]
	{
		return waiting.empty();
	});
}

bool
IRCD_MODULE_EXPORT
ircd::net::dns::cache::put(const hostport &hp,
                           const opts &opts,
                           const uint &code,
                           const string_view &msg)
{
	char type_buf[64];
	const string_view type
	{
		make_type(type_buf, opts.qtype)
	};

	char state_key_buf[m::event::STATE_KEY_MAX_SIZE];
	const string_view &state_key
	{
		opts.qtype == 33?
			make_SRV_key(state_key_buf, hp, opts):
			host(hp)
	};

	return put(type, state_key, code, msg);
}

bool
IRCD_MODULE_EXPORT
ircd::net::dns::cache::put(const hostport &hp,
                           const opts &opts,
                           const records &rrs)
{
	const auto &type_code
	{
		!rrs.empty()? rrs.at(0)->type : opts.qtype
	};

	char type_buf[48];
	const string_view type
	{
		make_type(type_buf, type_code)
	};

	char state_key_buf[m::event::STATE_KEY_MAX_SIZE];
	const string_view &state_key
	{
		opts.qtype == 33?
			make_SRV_key(state_key_buf, hp, opts):
			host(hp)
	};

	return put(type, state_key, rrs);
}

bool
ircd::net::dns::cache::put(const string_view &type,
                           const string_view &state_key,
                           const uint &code,
                           const string_view &msg)
try
{
	char content_buf[1024];
	json::stack out{content_buf};
	json::stack::object content{out};
	json::stack::array array
	{
		content, ""
	};

	json::stack::object rr0
	{
		array
	};

	json::stack::member
	{
		rr0, "errcode", lex_cast(code)
	};

	json::stack::member
	{
		rr0, "error", msg
	};

	json::stack::member
	{
		rr0, "ttl", json::value
		{
			code == 3?
				long(seconds(nxdomain_ttl).count()):
				long(seconds(error_ttl).count())
		}
	};

	rr0.~object();
	array.~array();
	content.~object();
	const m::room room
	{
		dns_room_id
	};

	if(unlikely(!exists(room)))
		create(room, m::me(), "internal");

	send(room, m::me(), type, state_key, json::object(out.completed()));
	return true;
}
catch(const http::error &e)
{
	const ctx::exception_handler eh;
	log::error
	{
		log, "cache put (%s, %s) code:%u (%s) :%s %s",
		type,
		state_key,
		code,
		msg,
		e.what(),
		e.content,
	};

	const json::value error_value
	{
		json::object{e.content}
	};

	const json::value error_records{&error_value, 1};
	const json::strung error{error_records};
	waiter::call(rfc1035::qtype.at(lstrip(type, "ircd.dns.rrs.")), state_key, error);
	return false;
}
catch(const std::exception &e)
{
	const ctx::exception_handler eh;
	log::error
	{
		log, "cache put (%s, %s) code:%u (%s) :%s",
		type,
		state_key,
		code,
		msg,
		e.what()
	};

	const json::members error_object
	{
		{ "error", e.what() },
	};

	const json::value error_value{error_object};
	const json::value error_records{&error_value, 1};
	const json::strung error{error_records};
	waiter::call(rfc1035::qtype.at(lstrip(type, "ircd.dns.rrs.")), state_key, error);
	return false;
}

bool
ircd::net::dns::cache::put(const string_view &type,
                           const string_view &state_key,
                           const records &rrs)
try
{
	const unique_buffer<mutable_buffer> buf
	{
		8_KiB
	};

	json::stack out{buf};
	json::stack::object content{out};
	json::stack::array array
	{
		content, ""
	};

	if(rrs.empty())
	{
		// Add one object to the array with nothing except a ttl indicating no
		// records (and no error) so we can cache that for the ttl. We use the
		// nxdomain ttl for this value.
		json::stack::object rr0{array};
		json::stack::member
		{
			rr0, "ttl", json::value
			{
				long(seconds(nxdomain_ttl).count())
			}
		};
	}
	else for(const auto &record : rrs)
	{
		switch(record->type)
		{
			case 1: // A
			{
				json::stack::object object{array};
				dynamic_cast<const rfc1035::record::A *>(record)->append(object);
				continue;
			}

			case 5: // CNAME
			{
				json::stack::object object{array};
				dynamic_cast<const rfc1035::record::CNAME *>(record)->append(object);
				continue;
			}

			case 28: // AAAA
			{
				json::stack::object object{array};
				dynamic_cast<const rfc1035::record::AAAA *>(record)->append(object);
				continue;
			}

			case 33: // SRV
			{
				json::stack::object object{array};
				dynamic_cast<const rfc1035::record::SRV *>(record)->append(object);
				continue;
			}
		}
	}

	array.~array();
	content.~object();
	const m::room room
	{
		dns_room_id
	};

	if(unlikely(!exists(room)))
		create(room, m::me(), "internal");

	send(room, m::me(), type, state_key, json::object{out.completed()});
	return true;
}
catch(const http::error &e)
{
	const ctx::exception_handler eh;
	log::error
	{
		log, "cache put (%s, %s) rrs:%zu :%s %s",
		type,
		state_key,
		rrs.size(),
		e.what(),
		e.content,
	};

	const json::value error_value
	{
		json::object{e.content}
	};

	const json::value error_records{&error_value, 1};
	const json::strung error{error_records};
	waiter::call(rfc1035::qtype.at(lstrip(type, "ircd.dns.rrs.")), state_key, error);
	return false;
}
catch(const std::exception &e)
{
	const ctx::exception_handler eh;
	log::error
	{
		log, "cache put (%s, %s) rrs:%zu :%s",
		type,
		state_key,
		rrs.size(),
		e.what(),
	};

	const json::members error_object
	{
		{ "error", e.what() },
	};

	const json::value error_value{error_object};
	const json::value error_records{&error_value, 1};
	const json::strung error{error_records};
	waiter::call(rfc1035::qtype.at(lstrip(type, "ircd.dns.rrs.")), state_key, error);
	return false;
}

bool
IRCD_MODULE_EXPORT
ircd::net::dns::cache::get(const hostport &hp,
                           const opts &opts,
                           const callback &closure)
{
	char type_buf[48];
	const string_view type
	{
		make_type(type_buf, opts.qtype)
	};

	char state_key_buf[rfc1035::NAME_BUFSIZE * 2];
	const string_view &state_key
	{
		opts.qtype == 33?
			make_SRV_key(state_key_buf, hp, opts):
			host(hp)
	};

	const m::room::state state
	{
		dns_room_id
	};

	const m::event::idx &event_idx
	{
		state.get(std::nothrow, type, state_key)
	};

	if(!event_idx)
		return false;

	time_t origin_server_ts;
	if(!m::get<time_t>(event_idx, "origin_server_ts", origin_server_ts))
		return false;

	bool ret{false};
	const time_t ts{origin_server_ts / 1000L};
	m::get(std::nothrow, event_idx, "content", [&hp, &closure, &ret, &ts]
	(const json::object &content)
	{
		const json::array &rrs
		{
			content.get("")
		};

		// If all records are expired then skip; otherwise since this closure
		// expects a single array we reveal both expired and valid records.
		ret = !std::all_of(begin(rrs), end(rrs), [&ts]
		(const json::object &rr)
		{
			return expired(rr, ts);
		});

		if(ret && closure)
			closure(hp, rrs);
	});

	return ret;
}

bool
IRCD_MODULE_EXPORT
ircd::net::dns::cache::for_each(const hostport &hp,
                                const opts &opts,
                                const closure &closure)
{
	char type_buf[48];
	const string_view type
	{
		make_type(type_buf, opts.qtype)
	};

	char state_key_buf[rfc1035::NAME_BUFSIZE * 2];
	const string_view &state_key
	{
		opts.qtype == 33?
			make_SRV_key(state_key_buf, hp, opts):
			host(hp)
	};

	const m::room::state state
	{
		dns_room_id
	};

	const m::event::idx &event_idx
	{
		state.get(std::nothrow, type, state_key)
	};

	if(!event_idx)
		return false;

	time_t origin_server_ts;
	if(!m::get<time_t>(event_idx, "origin_server_ts", origin_server_ts))
		return false;

	bool ret{true};
	const time_t ts{origin_server_ts / 1000L};
	m::get(std::nothrow, event_idx, "content", [&state_key, &closure, &ret, &ts]
	(const json::object &content)
	{
		for(const json::object rr : json::array(content.get("")))
		{
			if(expired(rr, ts))
				continue;

			if(!(ret = closure(state_key, rr)))
				break;
		}
	});

	return ret;
}

bool
IRCD_MODULE_EXPORT
ircd::net::dns::cache::for_each(const string_view &type,
                                const closure &closure)
{
	char type_buf[48];
	const string_view full_type
	{
		make_type(type_buf, type)
	};

	const m::room::state state
	{
		dns_room_id
	};

	return state.for_each(full_type, [&closure]
	(const string_view &, const string_view &state_key, const m::event::idx &event_idx)
	{
		time_t origin_server_ts;
		if(!m::get<time_t>(event_idx, "origin_server_ts", origin_server_ts))
			return true;

		bool ret{true};
		const time_t ts{origin_server_ts / 1000L};
		m::get(std::nothrow, event_idx, "content", [&state_key, &closure, &ret, &ts]
		(const json::object &content)
		{
			for(const json::object rr : json::array(content.get("")))
			{
				if(expired(rr, ts))
					continue;

				if(!(ret = closure(state_key, rr)))
					break;
			}
		});

		return ret;
	});
}

void
ircd::net::dns::cache::handle(const m::event &event,
                              m::vm::eval &eval)
try
{
	const string_view &type
	{
		json::get<"type"_>(event)
	};

	if(!startswith(type, "ircd.dns.rrs."))
		return;

	const string_view &state_key
	{
		json::get<"state_key"_>(event)
	};

	const json::array &rrs
	{
		json::get<"content"_>(event).get("")
	};

	waiter::call(rfc1035::qtype.at(lstrip(type, "ircd.dns.rrs.")), state_key, rrs);
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "handle_cached() :%s", e.what()
	};
}

//
// cache room creation
//

namespace ircd::net::dns::cache
{
	static void create_room();

	extern m::hookfn<m::vm::eval &> create_room_hook;
}

decltype(ircd::net::dns::cache::create_room_hook)
ircd::net::dns::cache::create_room_hook
{
	{
		{ "_site",    "vm.effect"      },
		{ "room_id",  "!ircd"          },
		{ "type",     "m.room.create"  },
	},

	[](const m::event &, m::vm::eval &)
	{
		create_room();
	}
};

void
ircd::net::dns::cache::create_room()
try
{
	const m::room room
	{
		m::create(dns_room_id, m::me(), "internal")
	};

	log::debug
	{
		m::log, "Created '%s' for the DNS cache module.",
		string_view{dns_room_id}
	};
}
catch(const std::exception &e)
{
	log::critical
	{
		m::log, "Creating the '%s' room failed :%s",
		string_view{dns_room_id},
		e.what()
	};
}
