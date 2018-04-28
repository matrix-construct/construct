// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_NET_DNS_H

namespace ircd::net
{
	struct dns extern dns;
}

/// DNS resolution suite.
///
/// This is a singleton class; public usage is to make calls on the singleton
/// object like `ircd::net::dns()` etc.
///
/// Note that in returned rfc1035::records that TTL values are modified from their
/// original value to an absolute epoch time in seconds which we use for caching.
/// This modification only occurs if the dns query allows result caching (default).
///
struct ircd::net::dns
{
	struct opts;
	struct resolver;
	struct cache;

	struct cache static cache;
	struct resolver static *resolver;
	struct opts static const opts_default;

	using callback = std::function<void (std::exception_ptr, vector_view<const rfc1035::record *>)>;
	using callback_A_one = std::function<void (std::exception_ptr, const rfc1035::record::A &)>;
	using callback_SRV_one = std::function<void (std::exception_ptr, const rfc1035::record::SRV &)>;
	using callback_ipport_one = std::function<void (std::exception_ptr, const ipport &)>;

	// (internal) generate strings for rfc1035 questions or dns::cache keys.
	static string_view make_SRV_key(const mutable_buffer &out, const hostport &, const opts &);

  public:
	// Cache warming
	static const callback_A_one prefetch_A;
	static const callback_SRV_one prefetch_SRV;
	static const callback_ipport_one prefetch_ipport;

	// Callback-based interface
	void operator()(const hostport &, const opts &, callback);
	void operator()(const hostport &, const opts &, callback_A_one);
	void operator()(const hostport &, const opts &, callback_SRV_one);
	void operator()(const hostport &, const opts &, callback_ipport_one);

	// Callback-based interface (default options)
	template<class Callback> void operator()(const hostport &, Callback&&);
};

/// DNS resolution options
struct ircd::net::dns::opts
{
	/// Overrides the SRV query to make for this resolution. If empty an
	/// SRV query may still be made from other deductions. This string is
	/// copied at the start of the resolution. It must be a fully qualified
	/// SRV query string: Example: "_matrix._tcp."
	string_view srv;

	/// Specifies the SRV protocol part when deducing the SRV query to be
	/// made. This is used when the net::hostport.service string is just
	/// the name of the service like "matrix" so we then use this value
	/// to build the protocol part of the SRV query string. It is ignored
	/// if `srv` is set or if no service is specified (thus no SRV query is
	/// made in the first place).
	string_view proto{"tcp"};

	/// Whether the dns::cache is checked and may respond to the query.
	bool cache_check {true};

	/// Whether the result of this lookup from the nameserver should be
	/// added to the cache. If true, the TTL value in result records will be
	/// modified to an absolute expiration time. If false, no modification
	/// occurs from the original value.
	bool cache_result {true};

	/// When false, nxdomain errors are not treated as exceptions and the
	/// eptr of a callback will not be set. Instead, the returned record
	/// will contain some nulled / empty data and the user is obliged to
	/// know that this is not a real DNS answer but the error's result.
	/// Note: Requires that cache_result is set to true. If not, this value
	/// is ignored and always considered to be set to true; this is because
	/// the returned record is a reference to the cached error.
	bool nxdomain_exceptions {true};
};

/// (internal) DNS cache
struct ircd::net::dns::cache
{
	static conf::item<seconds> min_ttl;
	static conf::item<seconds> clear_nxdomain;

	std::multimap<std::string, rfc1035::record::A, std::less<>> A;
	std::multimap<std::string, rfc1035::record::SRV, std::less<>> SRV;

  public:
	bool get(const hostport &, const opts &, const callback &);
	rfc1035::record *put(const rfc1035::question &, const rfc1035::answer &);
	rfc1035::record *put_error(const rfc1035::question &, const uint &code);
};

template<class Callback>
void
ircd::net::dns::operator()(const hostport &hostport,
                           Callback&& callback)
{
	operator()(hostport, opts_default, std::forward<Callback>(callback));
}
