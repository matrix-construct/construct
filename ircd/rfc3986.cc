/*
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <ircd/spirit.h>

namespace spirit = boost::spirit;
namespace qi = spirit::qi;
namespace karma = spirit::karma;
namespace ascii = qi::ascii;

using qi::lit;
using qi::string;
using qi::char_;
using qi::short_;
using qi::ushort_;
using qi::int_;
using qi::long_;
using qi::repeat;
using qi::omit;
using qi::raw;
using qi::attr;
using qi::eps;
using qi::attr_cast;

using karma::maxwidth;

namespace ircd::rfc3986
{
	template<class it> struct grammar;
}

template<class it>
struct ircd::rfc3986::grammar
:qi::grammar<it, spirit::unused_type>
{
	template<class R = spirit::unused_type, class... S> using rule = qi::rule<it, R, S...>;

	rule<> NUL                         { lit('\0')                                          ,"nul" };
	rule<> SP                          { lit('\x20')                                      ,"space" };
	rule<> HT                          { lit('\x09')                             ,"horizontal tab" };
	rule<> CR                          { lit('\x0D')                            ,"carriage return" };
	rule<> LF                          { lit('\x0A')                                  ,"line feed" };
	rule<> CRLF                        { CR >> LF                    ,"carriage return, line feed" };
	rule<> ws                          { SP | HT                                     ,"whitespace" };

/*
	rule<string_view> authority
	{
		//TODO: https://tools.ietf.org/html/rfc3986#section-3.2
		-('/' >> '/') >> raw[*(char_ - '/')] >> '/'
	};
*/
	const rule<> port
	{
		ushort_
		,"port"
	};

	const rule<> ip6_address
	{
		//TODO: XXX
		*char_("0-9a-fA-F:")
		,"ip6 address"
	};

	const rule<> ip6_literal
	{
		'[' >> ip6_address >> ']'
		,"ip6 literal"
	};

	const rule<> dns_name
	{
		ip6_literal | *(char_ - ':')
		,"dns name"
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
	template<class R = spirit::unused_type, class... S> using rule = qi::rule<const char *, R, S...>;

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
