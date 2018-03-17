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

	template<class it> struct grammar;
}

template<class it>
struct ircd::rfc3986::grammar
:qi::grammar<it, unused_type>
{
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
	const rule<> ip6_addr[10]
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
		ip6_addr[0] | ip6_addr[1] | ip6_addr[2] | ip6_addr[3] | ip6_addr[4] | ip6_addr[5] |
		ip6_addr[6] | ip6_addr[7] | ip6_addr[8] | ip6_addr[9]
		,"IPv6 address"
	};

	const rule<> ip6_literal
	{
		'[' >> ip6_address >> ']'
		,"ip6 literal"
	};

	const rule<> hostlabel
	{
		char_("A-Za-z0-9") >> *(char_("A-Za-z0-9\x2D")) // x2D is '-'
	};

	const rule<> hostname
	{
		hostlabel % '.',
		"hostname"
	};

	const rule<> host
	{
		ip6_literal | ip4_literal | hostname
		,"DNS name"
	};

	const rule<> remote
	{
		host >> -(':' > port)
		,"server name"
	};

	grammar()
	:grammar::base_type{rule<>{}}
	{}
};

struct ircd::rfc3986::parser
:grammar<const char *>
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

ircd::string_view
ircd::rfc3986::encode(const string_view &url,
                      const mutable_buffer &buf)
{
	char *out{data(buf)};
	karma::generate(out, maxwidth(size(buf))[encoder], url);
	return string_view{data(buf), size_t(std::distance(data(buf), out))};
}

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

ircd::string_view
ircd::rfc3986::decode(const string_view &url,
                      const mutable_buffer &buf)
try
{
	const char *start{url.data()}, *const stop
	{
		start + std::min(size(url), size(buf))
	};

	mutable_buffer mb{data(buf), size_t(0)};
	qi::parse(start, stop, eps > decoder, mb);
	return string_view{data(mb), size(mb)};
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
