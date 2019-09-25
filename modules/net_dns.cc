// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "net_dns.h"

namespace ircd::net::dns
{
	template<class T> static rfc1035::record *new_record(mutable_buffer &, const rfc1035::answer &);
	static void handle_resolved(std::exception_ptr, const tag &, const answers &);

	static void handle_resolve_A_ipport(const hostport &, const json::object &rr, opts, uint16_t, callback_ipport);
	static void handle_resolve_SRV_ipport(const hostport &, const json::object &rr, opts, callback_ipport);
	static void handle_resolve_one(const hostport &, const json::array &rr, callback_one);

	static void fini();
	static void init();
}

ircd::mapi::header
IRCD_MODULE
{
	"Domain Name System Client, Cache & Components",
	ircd::net::dns::init,
	ircd::net::dns::fini,
};

void
ircd::net::dns::init()
{
	cache::init();
	resolver_init(handle_resolved);
}

void
ircd::net::dns::fini()
{
	cache::fini();
	resolver_fini();
}

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
			std::bind(&handle_resolve_A_ipport, ph::_1, ph::_2, opts, port(hp), std::move(callback))
		};

		resolve(hp, opts, std::move(handler));
	}
	else throw error
	{
		"Query type:%u not valid for ipport result callback.", opts.qtype
	};
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

	dns::callback handler
	{
		std::bind(&handle_resolve_one, ph::_1, ph::_2, std::move(callback))
	};

	resolve(hp, opts, std::move(handler));
}

void
IRCD_MODULE_EXPORT
ircd::net::dns::resolve(const hostport &hp,
                        const opts &opts,
                        callback cb)
{
	if(unlikely(!opts.qtype))
		throw error
		{
			"A query type is required; not specified; cannot be deduced here."
		};

	// Try to satisfy from the cache first. This requires a ctx.
	if(likely(ctx::current && opts.cache_check))
		if(cache::get(hp, opts, cb))
			return;

	// Remote query will be made; register this callback as waiting for reply
	assert(cb);
	cache::waiting.emplace_back(hp, opts, std::move(cb));

	// Check if there is already someone else waiting on the same query
	const auto count
	{
		std::count_if(begin(cache::waiting), end(cache::waiting), []
		(const auto &a)
		{
			return a == cache::waiting.back();
		})
	};

	// When nobody else is already waiting on this query we have to submit it.
	assert(count >= 1);
	if(count == 1)
		resolver_call(hp, opts);
}

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
		std::bind(&handle_resolve_A_ipport, ph::_1, ph::_2, opts, port(target), std::move(callback))
	};

	resolve(target, opts, std::move(handler));
}

void
ircd::net::dns::handle_resolve_A_ipport(const hostport &hp,
                                        const json::object &rr,
                                        const opts opts,
                                        const uint16_t port,
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
		ip, port
	};

	const hostport &target
	{
		host(hp), port
	};

	const auto eptr
	{
		!empty(error)?
			make_exception_ptr<rfc1035::error>(exception::hide_name, "%s", error):
		!ipport?
			make_exception_ptr<net::error>("Host has no A record."):
			std::exception_ptr{}
	};

	callback(eptr, target, ipport);
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
