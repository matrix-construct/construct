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
/// Note that at this time these functions must be called on an ircd::ctx to
/// conduct any queries of the cache.
///
namespace ircd::net::dns
{
	struct opts extern const opts_default;

	using records = vector_view<const rfc1035::record *>;
	using callback = std::function<void (const hostport &, const json::array &)>;
	using callback_one = std::function<void (const hostport &, const json::object &)>;
	using callback_ipport = std::function<void (std::exception_ptr, const hostport &, const ipport &)>;

	// Callback-based interface
	void resolve(const hostport &, const opts &, callback);
	void resolve(const hostport &, const opts &, callback_one); // convenience
	void resolve(const hostport &, const opts &, callback_ipport); // convenience

	// Utilities
	bool is_error(const json::object &rr);
	bool is_error(const json::array &rr);
	bool is_empty(const json::object &rr);
	bool is_empty(const json::array &rr);
	time_t get_ttl(const json::object &rr);
	bool expired(const json::object &rr, const time_t &rr_ts, const time_t &min_ttl);
	bool expired(const json::object &rr, const time_t &rr_ts);
	json::object random_choice(const json::array &);
	string_view make_SRV_key(const mutable_buffer &out, const hostport &, const opts &);
	string_view unmake_SRV_key(const string_view &);

	extern log::log log;
}

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
	/// added to the cache.
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
	using closure = std::function<bool (const string_view &, const json::object &)>;

	string_view make_type(const mutable_buffer &out, const string_view &);
	string_view make_type(const mutable_buffer &out, const uint16_t &);

	bool for_each(const string_view &type, const closure &); // do not make_type() here
	bool for_each(const hostport &, const opts &, const closure &);
	bool get(const hostport &, const opts &, const callback &);
	bool put(const hostport &, const opts &, const records &);
	bool put(const hostport &, const opts &, const uint &code, const string_view &msg = {});
}
