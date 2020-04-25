// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::net::dns
{
	template<class T> static rfc1035::record *new_record(mutable_buffer &, const rfc1035::answer &);
	static void handle_resolved(std::exception_ptr, const tag &, const answers &);

	static void handle_resolve_A_ipport(const hostport &, const json::object &rr, opts, callback_ipport);
	static void handle_resolve_SRV_ipport(const hostport &, const json::object &rr, opts, callback_ipport);
	static void handle_resolve_one(const hostport &, const json::array &rr, callback_one);
}

decltype(ircd::net::dns::log)
ircd::net::dns::log
{
	"net.dns"
};

decltype(ircd::net::dns::opts_default)
ircd::net::dns::opts_default;

//
// init
//

ircd::net::dns::init::init()
{
	#ifdef HAVE_NETDB_H
	static const int stay_open {true};
	::setservent(stay_open);
	#endif

	assert(!resolver_instance);
	resolver_instance = new resolver
	{
		handle_resolved
    };
}

ircd::net::dns::init::~init()
noexcept
{
	delete resolver_instance;
	resolver_instance = nullptr;

	#ifdef HAVE_NETDB_H
	::endservent();
	#endif
}

//
// net/dns.h
//

void
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
	opts.qtype = opts_.qtype?: 33; // default to SRV

	if(opts.qtype == 33)
	{
		opts.nxdomain_exceptions = false;
		net::dns::callback_one handler
		{
			std::bind(&handle_resolve_SRV_ipport, ph::_1, ph::_2, opts, std::move(callback))
		};

		resolve(hp, opts, std::move(handler));
	}
	else if(opts.qtype == 1 || opts.qtype == 28)
	{
		net::dns::callback_one handler
		{
			std::bind(&handle_resolve_A_ipport, ph::_1, ph::_2, opts, std::move(callback))
		};

		resolve(hp, opts, std::move(handler));
	}
	else throw error
	{
		"Query type:%u not valid for ipport result callback.", opts.qtype
	};
}

void
ircd::net::dns::resolve(const hostport &hp,
                        const opts &opts,
                        callback_one callback)
{
	if(unlikely(!opts.qtype))
		throw error
		{
			"Query type is required; not specified; cannot be deduced here."
		};

	dns::callback handler
	{
		std::bind(&handle_resolve_one, ph::_1, ph::_2, std::move(callback))
	};

	resolve(hp, opts, std::move(handler));
}

void
ircd::net::dns::resolve(const hostport &hp_,
                        const opts &opts,
                        callback cb)
{
	hostport hp(hp_);
	if(unlikely(!opts.qtype))
		throw error
		{
			"Query type is required; not specified; cannot be deduced here."
		};

	// Make any necessary attempt to translate a service name into a portnum.
	if(likely(opts.service_port))
		if(!port(hp) && service(hp))
			port(hp) = service_port(service(hp), opts.proto);

	// Try to satisfy from the cache first. This requires a ctx.
	if(likely(ctx::current && opts.cache_check))
		if(cache::get(hp, opts, cb))
			return;

	// Remote query will be made; register this callback as waiting for reply
	assert(cb);
	const ctx::critical_assertion ca;
	cache::waiting.emplace_back(hp, opts, std::move(cb));

	// Check if there is already someone else waiting on the same query
	const auto existing
	{
		std::find_if(begin(cache::waiting), end(cache::waiting), []
		(const auto &a)
		{
			return a == cache::waiting.back();
		})
	};

	// When nobody else is already waiting on this query we have to submit it.
	assert(existing != end(cache::waiting));
	if(*existing == cache::waiting.back())
		resolver_call(hp, opts);
}

/// Really assumptional and hacky right now. We're just assuming the SRV
/// key is the first two elements of a dot-delimited string which start
/// with underscores. If that isn't good enough in the future this will rot
/// and become a regression hazard.
ircd::string_view
ircd::net::dns::unmake_SRV_key(const string_view &key)
{
	if(token_count(key, '.') < 3)
		return key;

	if(!startswith(token(key, '.', 0), '_'))
		return key;

	if(!startswith(token(key, '.', 1), '_'))
		return key;

	return tokens_after(key, '.', 1);
}

ircd::string_view
ircd::net::dns::make_SRV_key(const mutable_buffer &out,
                             const hostport &hp,
                             const opts &opts)
{
	thread_local char tlbuf[2][rfc1035::NAME_BUFSIZE];
	if(unlikely(!service(hp) && !opts.srv))
		throw error
		{
			"Service name or query string option is required for SRV lookup."
		};

	assert(host(hp));
	if(!service(hp))
	{
		assert(opts.srv);
		return fmt::sprintf
		{
			out, "%s%s",
			opts.srv,
			tolower(tlbuf[1], host(hp))
		};
	}

	assert(service(hp));
	return fmt::sprintf
	{
		out, "_%s._%s.%s",
		tolower(tlbuf[0], service(hp)),
		opts.proto,
		tolower(tlbuf[1], host(hp)),
	};
}

ircd::json::object
ircd::net::dns::random_choice(const json::array &rrs)
{
	const size_t &count
	{
		rrs.size()
	};

	if(!count)
		return json::object{};

	const auto choice
	{
		rand::integer(0, count - 1)
	};

	assert(choice < count);
	const json::object &rr
	{
		rrs[choice]
	};

	return rr;
}

bool
ircd::net::dns::expired(const json::object &rr,
                        const time_t &rr_ts)
{
	const seconds &min_seconds
	{
		cache::min_ttl
	};

	const seconds &err_seconds
	{
		cache::error_ttl
	};

	const time_t &min
	{
		is_error(rr)?
			err_seconds.count():
			min_seconds.count()
	};

	return expired(rr, rr_ts, min);
}

bool
ircd::net::dns::expired(const json::object &rr,
                        const time_t &rr_ts,
                        const time_t &min_ttl)
{
	const auto &ttl
	{
		get_ttl(rr)
	};

	return rr_ts + std::max(ttl, min_ttl) < ircd::time();
}

time_t
ircd::net::dns::get_ttl(const json::object &rr)
{
	return rr.get<time_t>("ttl", 0L);
}

bool
ircd::net::dns::is_empty(const json::array &rrs)
{
	return std::all_of(begin(rrs), end(rrs), []
	(const json::object &rr)
	{
		return is_empty(rr);
	});
}

bool
ircd::net::dns::is_empty(const json::object &rr)
{
	return empty(rr) || (rr.has("ttl") && size(rr) == 1);
}

bool
ircd::net::dns::is_error(const json::array &rrs)
{
	return !std::none_of(begin(rrs), end(rrs), []
	(const json::object &rr)
	{
		return is_error(rr);
	});
}

bool
ircd::net::dns::is_error(const json::object &rr)
{
	return rr.has("error");
}

//
// internal
//

void
ircd::net::dns::handle_resolve_one(const hostport &hp,
                                   const json::array &rrs,
                                   callback_one callback)
{
	const json::object &rr
	{
		random_choice(rrs)
	};

	callback(hp, rr);
}

void
ircd::net::dns::handle_resolve_SRV_ipport(const hostport &hp,
                                          const json::object &rr,
                                          opts opts,
                                          callback_ipport callback)
{
	const json::string &error
	{
		rr.get("error")
	};

	const hostport &target
	{
		rr.has("tgt")?
			rstrip(unquote(rr.at("tgt")), '.'):
			host(hp),

		rr.has("port")?
			rr.get<uint16_t>("port"):
		!error?
			port(hp):
			uint16_t(0)
	};

	if(error)
	{
		static const ipport empty;
		const auto eptr
		{
			make_exception_ptr<rfc1035::error>(exception::hide_name, "%s", error)
		};

		return callback(eptr, target, empty);
	}

	opts.qtype = 1;
	opts.nxdomain_exceptions = true;
	net::dns::callback_one handler
	{
		std::bind(&handle_resolve_A_ipport, ph::_1, ph::_2, opts, std::move(callback))
	};

	resolve(target, opts, std::move(handler));
}

void
ircd::net::dns::handle_resolve_A_ipport(const hostport &hp,
                                        const json::object &rr,
                                        const opts opts,
                                        const callback_ipport callback)
{
	const json::string &error
	{
		rr.get("error")
	};

	const json::string &ip
	{
		opts.qtype == 28?
			rr.get("ip", ":::0"_sv):
			rr.get("ip", "0.0.0.0"_sv)
	};

	const ipport &ipport
	{
		ip, port(hp)
	};

	const auto eptr
	{
		!empty(error)?
			make_exception_ptr<rfc1035::error>(exception::hide_name, "%s", error):
		!ipport?
			make_exception_ptr<net::error>("Host has no A record."):
			std::exception_ptr{}
	};

	callback(eptr, hp, ipport);
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

		assert(!empty(records));
		cache::put(tag.hp, tag.opts, records);
		s = e;
		if(e < i)
			last = record.at(e)->type;
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

	void *const pos(data(buf));
	consume(buf, sizeof(type));
	return new (pos) type(answer);
}
