// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

extern ircd::mapi::header
IRCD_MODULE;

namespace ircd::net::dns
{
	// Maximum number of records we present in result vector to any closure
	constexpr const size_t MAX_COUNT {64};

	static void handle__A(callback_A_one, std::exception_ptr, const hostport &, const records &);
	static void handle__SRV(callback_SRV_one, std::exception_ptr, const hostport &, const records &);
	static void handle_ipport__A(callback_ipport_one, std::exception_ptr, const hostport &, const rfc1035::record::A &);

	extern "C" void _resolve__(const hostport &, const opts &, callback);
	extern "C" void _resolve__A(const hostport &, opts, callback_A_one);
	extern "C" void _resolve__SRV(const hostport &, opts, callback_SRV_one);
	extern "C" void _resolve_ipport(const hostport &, opts, callback_ipport_one);
}

//
// s_dns_cache.cc
//

namespace ircd::net::dns::cache
{
	extern conf::item<seconds> min_ttl;
	extern conf::item<seconds> clear_nxdomain;

	extern std::multimap<std::string, rfc1035::record::A, std::less<>> cache_A;
	extern std::multimap<std::string, rfc1035::record::SRV, std::less<>> cache_SRV;

	extern "C" rfc1035::record *_put(const rfc1035::question &, const rfc1035::answer &);
	extern "C" rfc1035::record *_put_error(const rfc1035::question &, const uint &code);
	extern "C" bool _get(const hostport &, const opts &, const callback &);
	extern "C" bool _for_each(const uint16_t &type, const closure &);
}

//
// s_dns_resolver.cc
//

namespace ircd::net::dns
{
	// Resolver instance
	struct resolver extern *resolver;

	// Interface to resolver because it is not included here to avoid requiring
	// boost headers (ircd/asio.h) for units other than s_dns_resolver.cc
	void resolver_call(const hostport &, const opts &, callback &&);
	void resolver_init();
	void resolver_fini();
}
