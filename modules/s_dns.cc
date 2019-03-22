// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "s_dns.h"

ircd::mapi::header
IRCD_MODULE
{
	"Domain Name System Client, Cache & Components",
	[] // init
	{
		ircd::net::dns::resolver_init(ircd::net::dns::handle_resolved);
	},
	[] // fini
	{
		ircd::net::dns::resolver_fini();
	}
};

decltype(ircd::net::dns::cache::error_ttl)
ircd::net::dns::cache::error_ttl
{
	{ "name",     "ircd.net.dns.cache.error_ttl" },
	{ "default",  1200L                          },
};

decltype(ircd::net::dns::cache::nxdomain_ttl)
ircd::net::dns::cache::nxdomain_ttl
{
	{ "name",     "ircd.net.dns.cache.nxdomain_ttl" },
	{ "default",  43200L                            },
};

decltype(ircd::net::dns::cache::min_ttl)
ircd::net::dns::cache::min_ttl
{
	{ "name",     "ircd.net.dns.cache.min_ttl" },
	{ "default",  1200L                        },
};

decltype(ircd::net::dns::cache::room_id)
ircd::net::dns::cache::room_id
{
	"dns", my_host()
};

decltype(ircd::net::dns::cache::hook)
ircd::net::dns::cache::hook
{
    handle_cached,
    {
        { "_site",       "vm.notify"          },
        { "room_id",     string_view{room_id} },
    }
};

decltype(ircd::net::dns::waiting)
ircd::net::dns::waiting;

void
IRCD_MODULE_EXPORT
ircd::net::dns::resolve(const hostport &hp,
                        const opts &opts_,
                        callback_ipport callback)
{
	if(unlikely(!port(hp) && !hp.service))
		throw error
		{
			"Port or service is required for this query"
		};

	dns::opts opts(opts_);
	opts.qtype = 33;
	opts.nxdomain_exceptions = false;
	resolve(hp, opts, dns::callback_one{[opts, callback(std::move(callback))]
	(const hostport &hp, const json::object &rr)
	mutable
	{
		if(rr.has("error"))
		{
			const json::string &error(rr.get("error"));
			const auto eptr(make_exception_ptr<rfc1035::error>("%s", error));
			return callback(eptr, {host(hp), 0}, {});
		}

		const net::hostport target
		{
			rr.has("tgt")?
				rstrip(unquote(rr.at("tgt")), '.'):
				host(hp),

			rr.has("port")?
				rr.get<uint16_t>("port"):
				port(hp)
		};

		opts.qtype = 1;
		opts.nxdomain_exceptions = true;
		resolve(target, opts, dns::callback_one{[callback(std::move(callback)), target]
		(const hostport &hp, const json::object &rr)
		{
			const json::string &error(rr.get("error"));
			const json::string &ip(rr.get("ip", "0.0.0.0"));
			const net::ipport ipport(ip, port(target));
			const auto eptr
			{
				!empty(error)?
					make_exception_ptr<rfc1035::error>("%s", error):
				!ipport?
					make_exception_ptr<net::error>("Host has no A record."):
					std::exception_ptr{}
			};

			return callback(eptr, {host(hp), port(target)}, ipport);
		}});
	}});
}

void
IRCD_MODULE_EXPORT
ircd::net::dns::resolve(const hostport &hp,
                        const opts &opts,
                        callback_one callback)
{
	if(unlikely(!opts.qtype))
		throw error
		{
			"A query type is required; not specified; cannot be deduced here."
		};

	resolve(hp, opts, dns::callback{[callback(std::move(callback))]
	(const hostport &hp, const json::array &rrs)
	{
		const size_t &count(rrs.size());
		const auto choice(count? rand::integer(0, count - 1) : 0UL);
		const json::object &rr(rrs[choice]);
		callback(hp, rr);
	}});
}

void
IRCD_MODULE_EXPORT
ircd::net::dns::resolve(const hostport &hp,
                        const opts &opts,
                        callback cb)
{
	assert(ctx::current);
	if(unlikely(!opts.qtype))
		throw error
		{
			"A query type is required; not specified; cannot be deduced here."
		};

	if(opts.cache_check)
		if(cache::get(hp, opts, cb))
			return;

	auto key
	{
		opts.qtype == 33?
			ircd::string_buffer(rfc1035::NAME_BUF_SIZE*2, make_SRV_key, hp, opts):
			std::string(host(hp))
	};

	waiting.emplace_front([cb(std::move(cb)), opts, key(std::move(key)), port(net::port(hp))]
	(const string_view &type, const string_view &state_key, const json::array &rrs)
	{
		if(type != rfc1035::rqtype.at(opts.qtype))
			return false;

		if(state_key != key)
			return false;

		const hostport &hp
		{
			opts.qtype == 33? unmake_SRV_key(state_key): state_key, port
		};

		cb(hp, rrs);
		return true;
	});

	resolver_call(hp, opts);
}

void
ircd::net::dns::handle_cached(const m::event &event,
                              m::vm::eval &eval)
try
{
	const string_view &full_type
	{
		json::get<"type"_>(event)
	};

	if(!startswith(full_type, "ircd.dns.rrs."))
		return;

	const string_view &type
	{
		lstrip(full_type, "ircd.dns.rrs.")
	};

	const string_view &state_key
	{
		json::get<"state_key"_>(event)
	};

	const json::array &rrs
	{
		json::get<"content"_>(event).get("")
	};

	auto it(begin(waiting));
	while(it != end(waiting)) try
	{
		const auto &proffer(*it);
		if(proffer(type, state_key, rrs))
			it = waiting.erase(it);
		else
			++it;
	}
	catch(const std::exception &e)
	{
		++it;
		log::error
		{
			log, "proffer :%s", e.what()
		};
	}
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "handle_cached() :%s", e.what()
	};
}

/// Called back from the dns::resolver with a vector of answers to the
/// question (we get the whole tag here).
///
/// This is being invoked on the dns::resolver's receiver context stack
/// under lock preventing any other activity with the resolver.
///
/// We process these results and insert them into our cache. The cache
/// insertion involves sending a message to the DNS room. Matrix hooks
/// on that room will catch this message for the user(s) which initiated
/// this query; we don't callback or deal with said users here.
///
void
ircd::net::dns::handle_resolved(std::exception_ptr eptr,
                                const tag &tag,
                                const answers &an)
try
{
	static const size_t recsz(1024);
	thread_local char recbuf[recsz * MAX_COUNT];
	thread_local std::array<const rfc1035::record *, MAX_COUNT> record;

	size_t i(0);
	mutable_buffer buf{recbuf};
	for(; i < an.size(); ++i) switch(an.at(i).qtype)
	{
		case 1:
			record.at(i) = new_record<rfc1035::record::A>(buf, an.at(i));
			continue;

		case 5:
			record.at(i) = new_record<rfc1035::record::CNAME>(buf, an.at(i));
			continue;

		case 28:
			record.at(i) = new_record<rfc1035::record::AAAA>(buf, an.at(i));
			continue;

		case 33:
			record.at(i) = new_record<rfc1035::record::SRV>(buf, an.at(i));
			continue;

		default:
			record.at(i) = new_record<rfc1035::record>(buf, an.at(i));
			continue;
	}

	// Sort the records by type so we can create smaller vectors to send to the
	// cache. nulls from running out of space should be pushed to the back.
	std::sort(begin(record), begin(record) + an.size(), []
	(const auto *const &a, const auto *const &b)
	{
		if(!a)
			return false;

		if(!b)
			return true;

		return a->type < b->type;
	});

	//TODO: don't send cache ephemeral rcodes
	// Bail on error here; send the cache the message
	if(eptr)
	{
		cache::put(tag.hp, tag.opts, tag.rcode, what(eptr));
		return;
	}

	// Branch on no records with no error
	if(!i)
	{
		static const records empty;
		cache::put(tag.hp, tag.opts, empty);
		return;
	}

	// Iterate the record vector which was sorted by type;
	// send the cache an individual view of each type since
	// the cache is organized by record type.
	size_t s(0), e(0);
	auto last(record.at(e)->type);
	for(++e; e <= i; ++e)
	{
		if(e < i && record.at(e)->type == last)
			continue;

		const vector_view<const rfc1035::record *> records
		{
			record.data() + s, record.data() + e
		};

		cache::put(tag.hp, tag.opts, records);

		if(e < i)
		{
			last = record.at(e)->type;
			s = e;
		}
	}

	// We have to send something to the cache with the same type
	// as the query, otherwise our user will never get a response
	// to what they're waiting for.
	bool has_tag_qtype{false};
	for(size_t i(0); i < an.size() && !has_tag_qtype; ++i)
		has_tag_qtype = an.at(i).qtype == tag.opts.qtype;

	if(!has_tag_qtype)
	{
		static const records empty;
		cache::put(tag.hp, tag.opts, empty);
	}
}
catch(const std::exception &e)
{
	log::error
	{
		log, "handle resolved: tag[%u] :%s",
		tag.id,
		e.what()
	};

	throw;
}

template<class type>
ircd::rfc1035::record *
ircd::net::dns::new_record(mutable_buffer &buf,
                           const rfc1035::answer &answer)
{
	if(unlikely(sizeof(type) > size(buf)))
		return nullptr;

	const auto pos(data(buf));
	consume(buf, sizeof(type));
	return new (data(buf)) type(answer);
}

//
// cache
//

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

	char state_key_buf[rfc1035::NAME_BUF_SIZE * 2];
	const string_view &state_key
	{
		opts.qtype == 33?
			make_SRV_key(state_key_buf, host(hp), opts):
			host(hp)
	};

	char content_buf[768];
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
	send(room_id, m::me, type, state_key, json::object(out.completed()));
	return true;
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

	char state_key_buf[rfc1035::NAME_BUF_SIZE * 2];
	const string_view &state_key
	{
		opts.qtype == 33?
			make_SRV_key(state_key_buf, host(hp), opts):
			host(hp)
	};

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
	send(room_id, m::me, type, state_key, json::object{out.completed()});
	return true;
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

	char state_key_buf[rfc1035::NAME_BUF_SIZE * 2];
	const string_view &state_key
	{
		opts.qtype == 33?
			make_SRV_key(state_key_buf, host(hp), opts):
			host(hp)
	};

	const m::room::state state
	{
		room_id
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

		if(ret)
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

	char state_key_buf[rfc1035::NAME_BUF_SIZE * 2];
	const string_view &state_key
	{
		opts.qtype == 33?
			make_SRV_key(state_key_buf, host(hp), opts):
			host(hp)
	};

	const m::room::state state
	{
		room_id
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
		for(const json::object &rr : json::array(content.get("")))
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
		room_id
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
			for(const json::object &rr : json::array(content.get("")))
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

bool
ircd::net::dns::cache::expired(const json::object &rr,
                               const time_t &ts)
{
	const auto ttl(get_ttl(rr));
	return ts + ttl < ircd::time();
}

time_t
ircd::net::dns::cache::get_ttl(const json::object &rr)
{
	const seconds &min_ttl_s(min_ttl);
	const seconds &err_ttl_s(error_ttl);
	const time_t min_ttl_t(min_ttl_s.count());
	const time_t err_ttl_t(err_ttl_s.count());
	const time_t rr_ttl
	{
		rr.get<time_t>("ttl", err_ttl_t)
	};

	return std::max(rr_ttl, min_ttl_t);
}

//
// cache room creation
//

namespace ircd::net::dns::cache
{
	static void create_room();

	extern bool room_exists;
	extern const m::hookfn<m::vm::eval &> create_room_hook;
	extern const ircd::run::changed create_room_hook_alt;
}

decltype(ircd::net::dns::cache::room_exists)
ircd::net::dns::cache::room_exists
{
	m::exists(room_id)
};

decltype(ircd::net::dns::cache::create_room_hook)
ircd::net::dns::cache::create_room_hook
{
	{
		{ "_site",       "vm.effect"      },
		{ "room_id",     "!ircd"          },
		{ "type",        "m.room.create"  },
	},
	[](const m::event &, m::vm::eval &)
	{
		create_room();
	}
};

/// This is for existing installations that won't catch an
/// !ircd room create and must create this room.
decltype(ircd::net::dns::cache::create_room_hook_alt)
ircd::net::dns::cache::create_room_hook_alt{[]
(const auto &level)
{
	if(level != run::level::RUN || room_exists)
		return;

	context{[]
	{
		if(m::exists(m::my_room))  // if false, the other hook will succeed.
			create_room();
	}};
}};

void
ircd::net::dns::cache::create_room()
try
{
	const m::room room
	{
		m::create(room_id, m::me, "internal")
	};

	log::debug
	{
		m::log, "Created '%s' for the DNS cache module.",
		string_view{room.room_id}
	};
}
catch(const std::exception &e)
{
	log::critical
	{
		m::log, "Creating the '%s' room failed :%s",
		string_view{room_id},
		e.what()
	};
}
