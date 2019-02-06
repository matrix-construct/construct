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

	struct grammar;
}

struct ircd::rfc3986::grammar
:qi::grammar<const char *, unused_type>
{
	using it = const char *;
	template<class R = unused_type, class... S> using rule = qi::rule<it, R, S...>;

	const rule<> port
	{
		ushort_
		,"port number"
	};

	const rule<> ip4_octet
	{
		repeat(1,3)[char_("0-9")]
		,"IPv4 octet"
	};

	const rule<> ip4_literal
	{
		repeat(3)[ip4_octet >> '.'] >> ip4_octet
		,"IPv4 literal"
	};

	const rule<> ip6_char
	{
		char_("0-9a-fA-F")
		,"IPv6 character"
	};

	const rule<> ip6_h16
	{
		repeat(1,4)[ip6_char]
		,"IPv6 hexdigit"
	};

	const rule<> ip6_piece
	{
		ip6_h16 >> ':'
		,"IPv6 address piece"
	};

	// This is reversed from the BNF in the RFC otherwise it requires
	// backtracking during the repeat[]; grammars are adjusted accordingly.
	const rule<> ip6_ipiece
	{
		':' >> ip6_h16
		,"IPv6 address piece"
	};

	const rule<> ip6_ls32
	{
		(ip6_h16 >> ':' >> ip6_h16) | ip4_literal
	};

	/// https://tools.ietf.org/html/rfc3986 Appendix A
	const rule<> ip6_addr[9]
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

	const rule<> ip6_address
	{
		ip6_addr[0] | ip6_addr[1] | ip6_addr[2] |
		ip6_addr[3] | ip6_addr[4] | ip6_addr[5] |
		ip6_addr[6] | ip6_addr[7] | ip6_addr[8]
		,"IPv6 address"
	};

	const rule<> ip6_literal
	{
		'[' >> ip6_address >> ']'
		,"ip6 literal"
	};

	const rule<> hostname
	{
		char_("A-Za-z0-9") >> *(char_("A-Za-z0-9\x2D")) // x2D is '-'
		,"hostname"
	};

	const rule<> domain
	{
		hostname % '.'
		,"domain"
	};

	const rule<> host
	{
		ip6_literal | ip4_literal | domain
		,"host"
	};

	const rule<> remote
	{
		host >> -(':' > port)
		,"remote"
	};

	grammar()
	:grammar::base_type{rule<>{}}
	{}
};

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

struct ircd::rfc3986::parser
:grammar
{
	string_view operator()(const string_view &url) const;
}
const ircd::rfc3986::parser;

ircd::string_view
ircd::rfc3986::parser::operator()(const string_view &url)
const try
{
	string_view out;
	const char *start{url.data()};
	const char *const stop{url.data() + url.size()};
	//qi::parse(start, stop, , out);
	return out;
}
catch(const qi::expectation_failure<const char *> &e)
{
	auto rule
	{
		ircd::string(e.what_)
	};

	throw error
	{
		"Not a valid url because of an invalid %s.", between(rule, '<', '>')
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
		parser.remote >> eoi
	};

	const char *start(str.data()), *const stop(start + str.size());
	return qi::parse(start, stop, rule);
}

void
ircd::rfc3986::valid_remote(const string_view &str)
try
{
	static const auto &rule
	{
		parser.remote >> eoi
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
		parser.host >> eoi
	};

	const char *start(str.data()), *const stop(start + str.size());
	return qi::parse(start, stop, rule);
}

void
ircd::rfc3986::valid_host(const string_view &str)
try
{
	static const auto &rule
	{
		parser.host >> eoi
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
		parser.domain >> eoi
	};

	const char *start(str.data()), *const stop(start + str.size());
	return qi::parse(start, stop, rule);
}

void
ircd::rfc3986::valid_domain(const string_view &str)
try
{
	static const auto &rule
	{
		parser.host >> eoi
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
		parser.hostname >> eoi
	};

	const char *start(str.data()), *const stop(start + str.size());
	return qi::parse(start, stop, rule);
}

void
ircd::rfc3986::valid_hostname(const string_view &str)
try
{
	static const auto &rule
	{
		parser.hostname >> eoi
	};

	const char *start(str.data()), *const stop(start + str.size());
	qi::parse(start, stop, eps > rule);
}
catch(const qi::expectation_failure<const char *> &e)
{
	throw expectation_failure<error>{e};
}
