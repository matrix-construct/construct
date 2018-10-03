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
		ircd::net::dns::resolver_init();
	},
	[] // fini
	{
		ircd::net::dns::resolver_fini();
	}
};

/// Convenience composition with a single ipport callback. This is the result of
/// an automatic chain of queries such as SRV and A/AAAA based on the input and
/// intermediate results.
void
ircd::net::dns::_resolve_ipport(const hostport &hp,
                                const opts &opts,
                                callback_ipport_one callback)
{
	//TODO: ip6
	auto calluser{[callback(std::move(callback))]
	(std::exception_ptr eptr, const hostport &hp, const uint32_t &ip)
	{
		if(eptr)
			return callback(std::move(eptr), hp, {});

		if(!ip)
		{
			static const net::not_found no_record
			{
				"Host has no A record"
			};

			return callback(std::make_exception_ptr(no_record), hp, {});
		}

		const ipport ipport{ip, port(hp)};
		callback(std::move(eptr), hp, ipport);
	}};

	if(!hp.service)
		return _resolve__A(hp, opts, [calluser(std::move(calluser))]
		(std::exception_ptr eptr, const hostport &hp, const rfc1035::record::A &record)
		{
			calluser(std::move(eptr), hp, record.ip4);
		});

	auto srv_opts{opts};
	srv_opts.nxdomain_exceptions = false;
	_resolve__SRV(hp, srv_opts, [opts(opts), calluser(std::move(calluser))]
	(std::exception_ptr eptr, hostport hp, const rfc1035::record::SRV &record)
	mutable
	{
		if(eptr)
			return calluser(std::move(eptr), hp, 0);

		if(record.port != 0)
			hp.port = record.port;

		hp.host = record.tgt?: unmake_SRV_key(hp.host);
		opts.qtype = 0;

		_resolve__A(hp, opts, [calluser(std::move(calluser))]
		(std::exception_ptr eptr, const hostport &hp, const rfc1035::record::A &record)
		{
			calluser(std::move(eptr), hp, record.ip4);
		});
	});
}

/// Convenience callback with a single SRV record which was selected from
/// the vector with stochastic respect for weighting and priority.
void
ircd::net::dns::_resolve__SRV(const hostport &hp,
                              opts opts,
                              callback_SRV_one callback)
{
	static const auto &qtype
	{
		rfc1035::qtype.at("SRV")
	};

	if(unlikely(opts.qtype && opts.qtype != qtype))
		throw error
		{
			"Specified query type '%s' (%u) but user's callback is for SRV records only.",
			rfc1035::rqtype.at(opts.qtype),
			opts.qtype
		};

	if(!opts.qtype)
		opts.qtype = qtype;

	_resolve__(hp, opts, [callback(std::move(callback))]
	(std::exception_ptr eptr, const hostport &hp, const vector_view<const rfc1035::record *> rrs)
	{
		static const rfc1035::record::SRV empty;

		if(eptr)
			return callback(std::move(eptr), hp, empty);

		//TODO: prng on weight / prio plz
		for(size_t i(0); i < rrs.size(); ++i)
		{
			const auto &rr{*rrs.at(i)};
			if(rr.type != 33)
				continue;

			const auto &record(rr.as<const rfc1035::record::SRV>());
			return callback(std::move(eptr), hp, record);
		}

		return callback(std::move(eptr), hp, empty);
	});
}

/// Convenience callback with a single A record which was selected from
/// the vector randomly.
void
ircd::net::dns::_resolve__A(const hostport &hp,
                            opts opts,
                            callback_A_one callback)
{
	static const auto &qtype
	{
		rfc1035::qtype.at("A")
	};

	if(unlikely(opts.qtype && opts.qtype != qtype))
		throw error
		{
			"Specified query type '%s' (%u) but user's callback is for A records only.",
			rfc1035::rqtype.at(opts.qtype),
			opts.qtype
		};

	if(!opts.qtype)
		opts.qtype = qtype;

	_resolve__(hp, opts, [callback(std::move(callback))]
	(std::exception_ptr eptr, const hostport &hp, const vector_view<const rfc1035::record *> &rrs)
	{
		static const rfc1035::record::A empty;

		if(eptr)
			return callback(std::move(eptr), hp, empty);

		//TODO: prng plz
		for(size_t i(0); i < rrs.size(); ++i)
		{
			const auto &rr{*rrs.at(i)};
			if(rr.type != 1)
				continue;

			const auto &record(rr.as<const rfc1035::record::A>());
			return callback(std::move(eptr), hp, record);
		}

		return callback(std::move(eptr), hp, empty);
	});
}

/// Fundamental callback with a vector of abstract resource records.
void
ircd::net::dns::_resolve__(const hostport &hp,
                           const opts &opts,
                           callback cb)
{
	if(unlikely(!opts.qtype))
		throw error
		{
			"A query type is required; not specified; cannot be deduced here."
		};

	if(opts.cache_check)
		if(cache::get(hp, opts, cb))
			return;

	resolver_call(hp, opts, std::move(cb));
}
