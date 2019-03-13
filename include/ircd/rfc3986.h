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
#define HAVE_IRCD_RFC3986_H

/// Universal Resource Indicator (URI) grammars & tools
namespace ircd::rfc3986
{
	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, coding_error)
	IRCD_EXCEPTION(coding_error, encoding_error)
	IRCD_EXCEPTION(coding_error, decoding_error)

	struct parser;
	struct encoder extern const encoder;
	struct decoder extern const decoder;

	constexpr size_t HOSTNAME_MAX {rfc1035::LABEL_MAX};
	constexpr size_t DOMAIN_MAX {rfc1035::NAME_MAX};

	void valid_hostname(const string_view &); // name part
	bool valid_hostname(std::nothrow_t, const string_view &);
	void valid_domain(const string_view &); // dot delimited hostnames
	bool valid_domain(std::nothrow_t, const string_view &);
	void valid_host(const string_view &); // domain | ip4 | ip6
	bool valid_host(std::nothrow_t, const string_view &);
	void valid_remote(const string_view &); // host + optional :port
	bool valid_remote(std::nothrow_t, const string_view &);

	uint16_t port(const string_view &remote); // get portnum from valid remote
	string_view host(const string_view &remote); // get host without portnum

	string_view encode(const mutable_buffer &, const string_view &url);
	string_view encode(const mutable_buffer &, const json::members &);
	string_view decode(const mutable_buffer &, const string_view &url);
}

namespace ircd
{
	namespace url = rfc3986;
}

struct ircd::rfc3986::parser
{
	using it = const char *;
	using unused = boost::spirit::unused_type;

	template<class R = unused>
	using rule = boost::spirit::qi::rule<it, R, unused, unused, unused>;

	static const rule<uint16_t> port;
	static const rule<> ip4_octet;
	static const rule<> ip4_literal;
	static const rule<> ip6_char;
	static const rule<> ip6_h16;
	static const rule<> ip6_piece;
	static const rule<> ip6_ipiece;
	static const rule<> ip6_ls32;
	static const rule<> ip6_addr[9];
	static const rule<> ip6_address;
	static const rule<> ip6_literal;
	static const rule<> hostname;
	static const rule<> domain;
	static const rule<> host;
	static const rule<> remote;
};
