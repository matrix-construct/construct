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

/// DNS resolution suite.
///
/// Note that in returned rfc1035::records that TTL values are modified from their
/// original value to an absolute epoch time in seconds which we use for caching.
/// This modification only occurs if the dns query allows result caching (default).
///
namespace ircd::net::dns
{
	struct opts extern const opts_default;

	using records = vector_view<const rfc1035::record *>;
	using callback = std::function<void (std::exception_ptr, const hostport &, const records  &)>;
	using callback_A_one = std::function<void (std::exception_ptr, const hostport &, const rfc1035::record::A &)>;
	using callback_SRV_one = std::function<void (std::exception_ptr, const hostport &, const rfc1035::record::SRV &)>;
	using callback_ipport_one = std::function<void (std::exception_ptr, const hostport &, const ipport &)>;

	// Cache warming drop-in callbacks.
	extern const callback_A_one prefetch_A;
	extern const callback_SRV_one prefetch_SRV;
	extern const callback_ipport_one prefetch_ipport;

	// Callback-based interface
	void resolve(const hostport &, const opts &, callback);
	void resolve(const hostport &, const opts &, callback_A_one);
	void resolve(const hostport &, const opts &, callback_SRV_one);
	void resolve(const hostport &, const opts &, callback_ipport_one);

	// Callback-based interface (default options)
	template<class Callback> void resolve(const hostport &, Callback&&);

	// (internal) generate strings for rfc1035 questions or dns::cache keys.
	string_view make_SRV_key(const mutable_buffer &out, const hostport &, const opts &);
	string_view unmake_SRV_key(const string_view &);

	extern log::log log;
};

/// DNS resolution options
struct ircd::net::dns::opts
{
	/// Specifies the rfc1035 query type. If this is non-zero a query of this
	/// type will be made or an exception will be thrown if it is not possible
	/// to make the query with this type. A zero value means automatic and the
	/// type will deduced and set internally. To translate a string type like
	/// "SRV" to the type integer for this value see ircd/rfc1035.h.
	uint16_t qtype {0};

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
namespace ircd::net::dns::cache
{
	using closure = std::function<bool (const string_view &, const rfc1035::record &)>;

	bool for_each(const uint16_t &type, const closure &);
	bool for_each(const string_view &type, const closure &);
	bool get(const hostport &, const opts &, const callback &);
	rfc1035::record *put(const rfc1035::question &, const rfc1035::answer &);
	rfc1035::record *put_error(const rfc1035::question &, const uint &code);
};

template<class Callback>
void
ircd::net::dns::resolve(const hostport &hostport,
                        Callback&& callback)
{
	resolve(hostport, opts_default, std::forward<Callback>(callback));
}
