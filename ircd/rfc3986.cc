// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/spirit.h>

namespace ircd::rfc3986
{
	using namespace ircd::spirit;
}

struct ircd::rfc3986::encoder
:karma::grammar<char *, const string_view &>
{
	void throw_illegal()
	{
		throw encoding_error("Generator Protection: urlencode");
	}

	karma::rule<char *, const string_view &> url_encoding
	{
		*(karma::char_("A-Za-z0-9") | (karma::lit('%') << karma::hex))
		,"url encoding"
	};

	encoder(): encoder::base_type{url_encoding} {}
}
const ircd::rfc3986::encoder;

struct ircd::rfc3986::decoder
:qi::grammar<const char *, mutable_buffer>
{
	template<class R = unused_type, class... S> using rule = qi::rule<const char *, R, S...>;

	rule<> url_illegal
	{
		char_(0x00, 0x1f)
		,"url illegal"
	};

	rule<char()> url_encodable
	{
		char_("A-Za-z0-9")
		,"url encodable character"
	};

	rule<char()> urlencoded_character
	{
		'%' > qi::uint_parser<char, 16, 2, 2>{}
		,"urlencoded character"
	};

	rule<mutable_buffer> url_decode
	{
		*((char_ - '%') | urlencoded_character)
		,"urldecode"
	};

	decoder(): decoder::base_type { url_decode } {}
}
const ircd::rfc3986::decoder;

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

decltype(ircd::rfc3986::parser::ip4_literal)
ircd::rfc3986::parser::ip4_literal
{
	repeat(3)[ip4_octet >> '.'] >> ip4_octet
	,"IPv4 literal"
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
	(ip6_h16 >> ':' >> ip6_h16) | ip4_literal
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
	,"ip6 literal"
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

decltype(ircd::rfc3986::parser::host)
ircd::rfc3986::parser::host
{
	ip6_literal | ip4_literal | domain
	,"host"
};

decltype(ircd::rfc3986::parser::remote)
ircd::rfc3986::parser::remote
{
	host >> -(':' > port)
	,"remote"
};

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
		consume(buf, copy(buf, "="_sv));
		consume(buf, size(encode(buf, member.second)));
	}};

	auto it(begin(members));
	if(it != end(members))
	{
		append(*it);
		for(++it; it != end(members); ++it)
		{
			consume(buf, copy(buf, "&"_sv));
			append(*it);
		}
	}

	return buf.completed();
}

ircd::string_view
ircd::rfc3986::encode(const mutable_buffer &buf,
                      const string_view &url)
{
	char *out(data(buf));
	karma::generate(out, maxwidth(size(buf))[encoder], url);
	return string_view
	{
		data(buf), size_t(std::distance(data(buf), out))
	};
}

ircd::string_view
ircd::rfc3986::decode(const mutable_buffer &buf,
                      const string_view &url)
try
{
	const char *start(url.data()), *const stop
	{
		start + std::min(size(url), size(buf))
	};

	mutable_buffer mb
	{
		data(buf), size_t(0)
	};

	qi::parse(start, stop, eps > decoder, mb);
	return string_view
	{
		data(mb), size(mb)
	};
}
catch(const qi::expectation_failure<const char *> &e)
{
	const auto rule
	{
		ircd::string(e.what_)
	};

	throw decoding_error
	{
		"I require a valid urlencoded %s. You sent %zu invalid chars starting with `%s'.",
		between(rule, "<", ">"),
		size_t(e.last - e.first),
		string_view{e.first, e.last}
	};
}

bool
ircd::rfc3986::valid_remote(std::nothrow_t,
                            const string_view &str)
{
	static const auto &rule
	{
		parser::remote >> eoi
	};

	if(str.size() > DOMAIN_MAX + 6)
		return false;

	const char *start(str.data()), *const stop(start + str.size());
	return qi::parse(start, stop, rule);
}

void
ircd::rfc3986::valid_remote(const string_view &str)
try
{
	static const auto &rule
	{
		parser::remote >> eoi
	};

	if(str.size() > DOMAIN_MAX + 6)
		throw error
		{
			"String length %zu exceeds maximum of %zu characters",
			size(str),
			DOMAIN_MAX + 6
		};

	const char *start(str.data()), *const stop(start + str.size());
	qi::parse(start, stop, eps > rule);
}
catch(const qi::expectation_failure<const char *> &e)
{
	throw expectation_failure<error>{e};
}

bool
ircd::rfc3986::valid_host(std::nothrow_t,
                          const string_view &str)
{
	static const auto &rule
	{
		parser::host >> eoi
	};

	if(str.size() > DOMAIN_MAX)
		return false;

	const char *start(str.data()), *const stop(start + str.size());
	return qi::parse(start, stop, rule);
}

void
ircd::rfc3986::valid_host(const string_view &str)
try
{
	static const auto &rule
	{
		parser::host >> eoi
	};

	if(str.size() > DOMAIN_MAX)
		throw error
		{
			"String length %zu exceeds maximum of %zu characters",
			size(str),
			DOMAIN_MAX
		};

	const char *start(str.data()), *const stop(start + str.size());
	qi::parse(start, stop, eps > rule);
}
catch(const qi::expectation_failure<const char *> &e)
{
	throw expectation_failure<error>{e};
}

bool
ircd::rfc3986::valid_domain(std::nothrow_t,
                            const string_view &str)
{
	static const auto &rule
	{
		parser::domain >> eoi
	};

	if(str.size() > DOMAIN_MAX)
		return false;

	const char *start(str.data()), *const stop(start + str.size());
	return qi::parse(start, stop, rule);
}

void
ircd::rfc3986::valid_domain(const string_view &str)
try
{
	static const auto &rule
	{
		parser::host >> eoi
	};

	if(str.size() > DOMAIN_MAX)
		throw error
		{
			"String length %zu exceeds maximum of %zu characters",
			size(str),
			DOMAIN_MAX
		};

	const char *start(str.data()), *const stop(start + str.size());
	qi::parse(start, stop, eps > rule);
}
catch(const qi::expectation_failure<const char *> &e)
{
	throw expectation_failure<error>{e};
}

bool
ircd::rfc3986::valid_hostname(std::nothrow_t,
                              const string_view &str)
{
	static const auto &rule
	{
		parser::hostname >> eoi
	};

	if(str.size() > HOSTNAME_MAX)
		return false;

	const char *start(str.data()), *const stop(start + str.size());
	return qi::parse(start, stop, rule);
}

void
ircd::rfc3986::valid_hostname(const string_view &str)
try
{
	static const auto &rule
	{
		parser::hostname >> eoi
	};

	if(str.size() > HOSTNAME_MAX)
		throw error
		{
			"String length %zu exceeds maximum of %zu characters",
			size(str),
			HOSTNAME_MAX
		};

	const char *start(str.data()), *const stop(start + str.size());
	qi::parse(start, stop, eps > rule);
}
catch(const qi::expectation_failure<const char *> &e)
{
	throw expectation_failure<error>{e};
}
