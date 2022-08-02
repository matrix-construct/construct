// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::rfc3986::parser
{
	using namespace ircd::spirit;
}

decltype(ircd::rfc3986::parser::sub_delims)
ircd::rfc3986::parser::sub_delims
{
	lit('!') | lit('$') | lit('&') | lit('\'') |
	lit('(') | lit(')') | lit('*') | lit('+') |
	lit(',') | lit(';') | lit('=')
	,"sub-delims"
};

decltype(ircd::rfc3986::parser::gen_delims)
ircd::rfc3986::parser::gen_delims
{
	lit(':') | lit('/') | lit('?') | lit('#') |
	lit('[') | lit(']') | lit('@')
	,"gen-delims"
};

decltype(ircd::rfc3986::parser::unreserved)
ircd::rfc3986::parser::unreserved
{
	ascii::alpha | ascii::digit |
	lit('-') | lit('.') | lit('_') | lit('~')
	,"unreserved"
};

decltype(ircd::rfc3986::parser::reserved)
ircd::rfc3986::parser::reserved
{
	gen_delims | sub_delims
	,"reserved"
};

decltype(ircd::rfc3986::parser::pct_encoded)
ircd::rfc3986::parser::pct_encoded
{
	lit('%') >> repeat(2)[ascii::xdigit]
	,"pct-encoded"
};

decltype(ircd::rfc3986::parser::pchar)
ircd::rfc3986::parser::pchar
{
	unreserved | pct_encoded | sub_delims | lit(':') | lit('@')
	,"pchar"
};

decltype(ircd::rfc3986::parser::query)
ircd::rfc3986::parser::query
{
	*(pchar | lit('/') | lit('?'))
	,"query"
};

decltype(ircd::rfc3986::parser::fragment)
ircd::rfc3986::parser::fragment
{
	*(pchar | lit('/') | lit('?'))
	,"fragment"
};

decltype(ircd::rfc3986::parser::segment_nz_nc)
ircd::rfc3986::parser::segment_nz_nc
{
	+(unreserved | pct_encoded | sub_delims | lit('@'))
	,"segment-nz-nc"
};

decltype(ircd::rfc3986::parser::segment_nz)
ircd::rfc3986::parser::segment_nz
{
	+pchar
	,"segment-nz"
};

decltype(ircd::rfc3986::parser::segment)
ircd::rfc3986::parser::segment
{
	*pchar
	,"segment"
};

decltype(ircd::rfc3986::parser::path_abempty)
ircd::rfc3986::parser::path_abempty
{
	*(lit('/') >> segment)
	,"path-abempty"
};

decltype(ircd::rfc3986::parser::path_noscheme)
ircd::rfc3986::parser::path_noscheme
{
	segment_nz_nc >> *(lit('/') >> segment)
	,"path-noscheme"
};

decltype(ircd::rfc3986::parser::path_rootless)
ircd::rfc3986::parser::path_rootless
{
	segment_nz >> *(lit('/') >> segment)
	,"path-rootless"
};

decltype(ircd::rfc3986::parser::path_absolute)
ircd::rfc3986::parser::path_absolute
{
	lit('/') >> -(path_rootless)
	,"path-absolute"
};

decltype(ircd::rfc3986::parser::path)
ircd::rfc3986::parser::path
{
	-(path_abempty | path_absolute | path_noscheme | path_rootless)
	,"path"
};

decltype(ircd::rfc3986::parser::reg_name)
ircd::rfc3986::parser::reg_name
{
	*(unreserved | pct_encoded | sub_delims)
	,"reg-name"
};

decltype(ircd::rfc3986::parser::userinfo)
ircd::rfc3986::parser::userinfo
{
	*(unreserved | pct_encoded | sub_delims | lit(':'))
	,"userinfo"
};

decltype(ircd::rfc3986::parser::port)
ircd::rfc3986::parser::port
{
	ushort_
	,"port number"
};

decltype(ircd::rfc3986::parser::ip4_octet)
ircd::rfc3986::parser::ip4_octet
{
	repeat(1,3)[char_("0-9")]
	,"IPv4 octet"
};

decltype(ircd::rfc3986::parser::ip4_address)
ircd::rfc3986::parser::ip4_address
{
	repeat(3)[ip4_octet >> '.'] >> ip4_octet
	,"IPv4 address"
};

decltype(ircd::rfc3986::parser::ip4_literal)
ircd::rfc3986::parser::ip4_literal
{
	ip4_address
	,"IPv4 literal"
};

decltype(ircd::rfc3986::parser::ip4_remote)
ircd::rfc3986::parser::ip4_remote
{
	ip4_literal >> -(':' >> port)
	,"IPv4 remote"
};

decltype(ircd::rfc3986::parser::ip6_char)
ircd::rfc3986::parser::ip6_char
{
	char_("0-9a-fA-F")
	,"IPv6 character"
};

decltype(ircd::rfc3986::parser::ip6_h16)
ircd::rfc3986::parser::ip6_h16
{
	repeat(1,4)[ip6_char]
	,"IPv6 hexdigit"
};

decltype(ircd::rfc3986::parser::ip6_piece)
ircd::rfc3986::parser::ip6_piece
{
	ip6_h16 >> ':'
	,"IPv6 address piece"
};

// This is reversed from the BNF in the RFC otherwise it requires
// backtracking during the repeat[]; parsers are adjusted accordingly.
decltype(ircd::rfc3986::parser::ip6_ipiece)
ircd::rfc3986::parser::ip6_ipiece
{
	':' >> ip6_h16
	,"IPv6 address piece"
};

decltype(ircd::rfc3986::parser::ip6_ls32)
ircd::rfc3986::parser::ip6_ls32
{
	(ip6_h16 >> ':' >> ip6_h16) | ip4_address
};

/// https://tools.ietf.org/html/rfc3986 Appendix A
decltype(ircd::rfc3986::parser::ip6_addr)
ircd::rfc3986::parser::ip6_addr
{
	{                                                     repeat(6)[ip6_piece] >> ip6_ls32     },
	{                                        lit("::") >> repeat(5)[ip6_piece] >> ip6_ls32     },
	{                             ip6_h16 >> lit("::") >> repeat(4)[ip6_piece] >> ip6_ls32     },
	{  ip6_h16 >> repeat(0,1)[ip6_ipiece] >> lit("::") >> repeat(3)[ip6_piece] >> ip6_ls32     },
	{  ip6_h16 >> repeat(0,2)[ip6_ipiece] >> lit("::") >> repeat(2)[ip6_piece] >> ip6_ls32     },
	{  ip6_h16 >> repeat(0,3)[ip6_ipiece] >> lit("::") >> ip6_piece >> ip6_ls32                },
	{  ip6_h16 >> repeat(0,4)[ip6_ipiece] >> lit("::") >> ip6_ls32                             },
	{  ip6_h16 >> repeat(0,5)[ip6_ipiece] >> lit("::") >> -ip6_h16                             },
	{                                        lit("::") >> -ip6_h16                             },
};

decltype(ircd::rfc3986::parser::ip6_address)
ircd::rfc3986::parser::ip6_address
{
	ip6_addr[0] | ip6_addr[1] | ip6_addr[2] |
	ip6_addr[3] | ip6_addr[4] | ip6_addr[5] |
	ip6_addr[6] | ip6_addr[7] | ip6_addr[8]
	,"IPv6 address"
};

decltype(ircd::rfc3986::parser::ip6_literal)
ircd::rfc3986::parser::ip6_literal
{
	'[' >> ip6_address >> ']'
	,"IPv6 literal"
};

decltype(ircd::rfc3986::parser::ip6_remote)
ircd::rfc3986::parser::ip6_remote
{
	ip6_literal >> -(':' >> port)
	,"IPv6 literal"
};

decltype(ircd::rfc3986::parser::ip_address)
ircd::rfc3986::parser::ip_address
{
	ip4_address | ip6_address
	,"IP address"
};

decltype(ircd::rfc3986::parser::ip_literal)
ircd::rfc3986::parser::ip_literal
{
	ip6_literal | ip4_literal
	,"IP literal"
};

decltype(ircd::rfc3986::parser::ip_remote)
ircd::rfc3986::parser::ip_remote
{
	ip_literal >> -(':' >> port)
	,"IP literal"
};

decltype(ircd::rfc3986::parser::hostname)
ircd::rfc3986::parser::hostname
{
	char_("A-Za-z0-9") >> *(char_("A-Za-z0-9\x2D")) // x2D is '-'
	,"hostname"
};

decltype(ircd::rfc3986::parser::domain)
ircd::rfc3986::parser::domain
{
	hostname % '.'
	,"domain"
};

decltype(ircd::rfc3986::parser::hostport)
ircd::rfc3986::parser::hostport
{
	domain >> -(':' >> port)
	,"hostport"
};

decltype(ircd::rfc3986::parser::host)
ircd::rfc3986::parser::host
{
	ip_address | domain
	,"host"
};

decltype(ircd::rfc3986::parser::host_literal)
ircd::rfc3986::parser::host_literal
{
	ip_literal | domain
	,"host literal"
};

decltype(ircd::rfc3986::parser::remote)
ircd::rfc3986::parser::remote
{
	ip_remote | hostport
	,"remote"
};

decltype(ircd::rfc3986::parser::authority)
ircd::rfc3986::parser::authority
{
	-(userinfo >> lit('@')) >> remote
	,"authority"
};

decltype(ircd::rfc3986::parser::scheme)
ircd::rfc3986::parser::scheme
{
	ascii::alpha >> *(ascii::alnum | lit('+') | lit('-') | lit('.'))
	,"scheme"
};

decltype(ircd::rfc3986::parser::hier_part)
ircd::rfc3986::parser::hier_part
{
	-((lit("//") >> authority >> path_abempty) | path_absolute | path_rootless)
	,"hier_part"
};

decltype(ircd::rfc3986::parser::relative_part)
ircd::rfc3986::parser::relative_part
{
	-((lit("//") >> authority >> path_abempty) | path_absolute | path_noscheme)
	,"relative-part"
};

decltype(ircd::rfc3986::parser::relative_ref)
ircd::rfc3986::parser::relative_ref
{
	relative_part >> -(lit('?') >> query) >> -(lit('#') >> fragment)
	,"relative-ref"
};

decltype(ircd::rfc3986::parser::absolute_uri)
ircd::rfc3986::parser::absolute_uri
{
	scheme >> lit(':') >> hier_part >> -(lit('?') >> query)
	,"absolute-URI"
};

decltype(ircd::rfc3986::parser::uri)
ircd::rfc3986::parser::uri
{
	scheme >> lit(':') >> hier_part >> -(lit('?') >> query) >> -(lit('#') >> fragment)
	,"URI"
};

decltype(ircd::rfc3986::parser::uri_ref)
ircd::rfc3986::parser::uri_ref
{
	uri | relative_ref
	,"URI-reference"
};

//
// uri decompose
//

#pragma GCC visibility push(internal)
BOOST_FUSION_ADAPT_STRUCT
(
    ircd::rfc3986::uri,
    ( decltype(ircd::rfc3986::uri::scheme),    scheme    )
    ( decltype(ircd::rfc3986::uri::user),      user      )
    ( decltype(ircd::rfc3986::uri::remote),    remote    )
    ( decltype(ircd::rfc3986::uri::path),      path      )
    ( decltype(ircd::rfc3986::uri::query),     query     )
    ( decltype(ircd::rfc3986::uri::fragment),  fragment  )
)
#pragma GCC visibility pop

namespace ircd::rfc3986::parser
{
	static const expr uri_parse
	{
		raw[parser::scheme] >> lit("://")
		>> -raw[parser::userinfo >> lit('@')]
		>> raw[parser::remote]
		>> raw[parser::path_abempty]
		>> -raw[lit('?') >> parser::query]
		>> -raw[lit('#') >> parser::fragment]
		,"uri"
	};

	static const rule<rfc3986::uri> parse_uri
	{
		expect[uri_parse]
		,"uri"
	};
}

ircd::rfc3986::uri::uri(const string_view &input)
{
	const char *start(begin(input)), *const stop(end(input));
	ircd::parse<error>(start, stop, parser::parse_uri, *this);

	//TODO: XXX Can this go?
	this->user = rstrip(this->user, '@');
	this->query = lstrip(this->query, '?');
	this->fragment = lstrip(this->fragment, '#');
}

//
// uri decoding
//

namespace ircd::rfc3986::parser::decoder
{
	template<class R = unused_type,
	         class... S>
	struct [[gnu::visibility("internal")]] rule
	:qi::rule<const char *, R, S...>
	{
		using qi::rule<const char *, R, S...>::rule;
	};

	static const auto is_safe
	{
		[](const int8_t val, auto &c, bool &pass)
		{
			pass = (val > 0x1F) | (val < 0x00);
			attr_at<0>(c) = val;
		}
	};

	const rule<char()> decode_char_safe
	{
		lit('%') > qi::int_parser<uint8_t, 16, 2, 2>{}[is_safe]
		,"url decodable character"
	};

	const expr decode_char
	{
		lit('%') >> qi::int_parser<uint8_t, 16, 2, 2>{}
		,"url decodable character"
	};

	const expr unreserved_char
	{
		//char_("A-Za-z0-9._~!$+*'(),-")
		//NOTE: allow any non-control character here. No reason for trouble with
		//NOTE: already-decoded inputs unless some other grammar expects it.
		(~ascii::cntrl) - '%'
		,"url unreserved characters"
	};

	const rule<mutable_buffer> decode_safe
	{
		*(unreserved_char | decode_char_safe)
		,"url safe decode"
	};

	const rule<mutable_buffer> decode_unsafe
	{
		*((char_ - '%') | decode_char)
		,"url unsafe decode"
	};
}

ircd::const_buffer
ircd::rfc3986::decode_unsafe(const mutable_buffer &buf,
                             const string_view &url)
noexcept
{
	const char *start(url.data()), *const stop
	{
		start + std::min(size(url), size(buf))
	};

	mutable_buffer mb
	{
		data(buf), size_t(0)
	};

	const bool ok
	{
		ircd::parse(std::nothrow, start, stop, parser::decoder::decode_unsafe, mb)
	};

	assert(size(mb) <= size(url));
	return string_view
	{
		data(mb), size(mb)
	};
}

ircd::string_view
ircd::rfc3986::decode(const mutable_buffer &buf,
                      const string_view &url)
{
	const char *start(url.data()), *const stop
	{
		start + std::min(size(url), size(buf))
	};

	mutable_buffer mb
	{
		data(buf), size_t(0)
	};

	const bool ok
	{
		ircd::parse<decoding_error>(start, stop, parser::decoder::decode_safe, mb)
	};

	assert(size(mb) <= size(url));
	return string_view
	{
		data(mb), size(mb)
	};
}

//
// uri encoding
//

namespace ircd::rfc3986::parser::encoder
{
	template<class R = unused_type,
	         class... S>
	struct [[gnu::visibility("internal")]] rule
	:karma::rule<char *, R, S...>
	{
		using karma::rule<char *, R, S...>::rule;
	};

	const rule<char()> unreserved
	{
		char_("A-Za-z0-9._~-")
		,"url unencoded"
	};

	const rule<string_view> encode
	{
		*(unreserved | (lit('%') << karma::right_align(2, '0')[karma::upper[karma::hex]]))
		,"url encode"
	};
}

ircd::string_view
ircd::rfc3986::encode(const mutable_buffer &out,
                      const json::members &members)
{
	window_buffer buf{out};
	const auto append{[&buf](const json::member &member)
	{
		assert(type(member.first) == json::STRING);
		if(unlikely(!member.second.serial && type(member.second) != json::STRING))
			throw panic
			{
				"Cannot encode non-serial json::member type '%s'",
				reflect(type(member.second))
			};

		consume(buf, size(encode(buf, member.first)));
		consume(buf, copy(buf, '='));
		consume(buf, size(encode(buf, member.second)));
	}};

	auto it(begin(members));
	if(it != end(members))
	{
		append(*it);
		for(++it; it != end(members); ++it)
		{
			consume(buf, copy(buf, '&'));
			append(*it);
		}
	}

	return buf.completed();
}

ircd::string_view
ircd::rfc3986::encode(const mutable_buffer &buf_,
                      const string_view &url)
{
	mutable_buffer buf
	{
		buf_
	};

	const bool ok
	{
		ircd::generate(buf, parser::encoder::encode, url)
	};

	return string_view
	{
		data(buf_), data(buf)
	};
}

//
// general interface
//

namespace ircd::rfc3986::parser
{
	[[gnu::visibility("internal")]]
	extern const rule<string_view>
	host_literal_parse,
	host_non_literal_parse,
	host_alternative_parse,
	host_parse;

	[[gnu::visibility("internal")]]
	extern const rule<uint16_t>
	port_parse;
}

decltype(ircd::rfc3986::parser::host_literal_parse)
ircd::rfc3986::parser::host_literal_parse
{
	parser::ip6_address
};

decltype(ircd::rfc3986::parser::host_non_literal_parse)
ircd::rfc3986::parser::host_non_literal_parse
{
	parser::ip6_address | parser::ip4_address | parser::domain
};

decltype(ircd::rfc3986::parser::host_alternative_parse)
ircd::rfc3986::parser::host_alternative_parse
{
	(&lit('[') > raw[host_literal_parse]) | raw[host_non_literal_parse]
	,"host"
};

decltype(ircd::rfc3986::parser::host_parse)
ircd::rfc3986::parser::host_parse
{
	expect[host_alternative_parse >> omit[&lit(':') | eoi]]
	,"host"
};

decltype(ircd::rfc3986::parser::port_parse)
ircd::rfc3986::parser::port_parse
{
	omit[host_alternative_parse] >> (omit[lit(':')] >> port) >> eoi
	,"port"
};

ircd::string_view
ircd::rfc3986::host(const string_view &str)
{
	string_view ret;
	const char *start(str.data()), *const stop(start + str.size());
	ircd::parse<error>(start, stop, parser::host_parse, ret);
	assert(!ret.empty());
	return ret;
}

uint16_t
ircd::rfc3986::port(const string_view &str)
{
	uint16_t ret(0);
	const char *start(str.data()), *const stop(start + str.size());
	ircd::parse<error>(start, stop, parser::port_parse, ret);
	return ret;
}

//
// validators
//

bool
ircd::rfc3986::valid_remote(std::nothrow_t,
                            const string_view &str)
{
	if(likely(str.size() <= DOMAIN_MAX + 6))
		return valid(std::nothrow, parser::remote, str);

	return false;
}

void
ircd::rfc3986::valid_remote(const string_view &str)
{
	if(unlikely(str.size() > DOMAIN_MAX + 6))
		throw error
		{
			"String length %zu exceeds maximum of %zu characters",
			size(str),
			DOMAIN_MAX + 6
		};

	valid(parser::remote, str);
}

bool
ircd::rfc3986::valid_domain(std::nothrow_t,
                           const string_view &str)
{
	if(likely(str.size() <= DOMAIN_MAX))
		return valid(std::nothrow, parser::domain, str);

	return false;
}

void
ircd::rfc3986::valid_domain(const string_view &str)
{
	if(unlikely(str.size() > DOMAIN_MAX))
		throw error
		{
			"String length %zu exceeds maximum of %zu characters",
			size(str),
			DOMAIN_MAX
		};

	valid(parser::domain, str);
}

bool
ircd::rfc3986::valid_host(std::nothrow_t,
                          const string_view &str)
{
	if(likely(str.size() <= DOMAIN_MAX))
		return valid(std::nothrow, parser::host, str);

	return false;
}

void
ircd::rfc3986::valid_host(const string_view &str)
{
	if(unlikely(str.size() > DOMAIN_MAX))
		throw error
		{
			"String length %zu exceeds maximum of %zu characters",
			size(str),
			DOMAIN_MAX
		};

	valid(parser::host, str);
}

bool
ircd::rfc3986::valid_hostname(std::nothrow_t,
                              const string_view &str)
{
	if(likely(str.size() <= HOSTNAME_MAX))
		return valid(std::nothrow, parser::hostname, str);

	return false;
}

void
ircd::rfc3986::valid_hostname(const string_view &str)
{
	if(unlikely(str.size() > HOSTNAME_MAX))
		throw error
		{
			"String length %zu exceeds maximum of %zu characters",
			size(str),
			HOSTNAME_MAX
		};

	valid(parser::hostname, str);
}

bool
ircd::rfc3986::valid(std::nothrow_t,
                     const parser::rule<> &rule,
                     const string_view &str)
{
	const parser::rule<> only_rule
	{
		rule >> parser::eoi
	};

	const char *start(str.data()), *const stop(start + str.size());
	return ircd::parse(std::nothrow, start, stop, only_rule);
}

void
ircd::rfc3986::valid(const parser::rule<> &rule,
                     const string_view &str)
{
	const parser::rule<> only_rule
	{
		parser::eps > (rule >> parser::eoi)
	};

	const char *start(str.data()), *const stop(start + str.size());
	ircd::parse<error>(start, stop, only_rule);
}
