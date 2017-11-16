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
#include <ircd/server.h>
#include <ircd/m/m.h>

namespace spirit = boost::spirit;
namespace qi = spirit::qi;
namespace ascii = qi::ascii;
using namespace ircd;

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

template<class it>
struct grammar
:qi::grammar<it, spirit::unused_type>
{
	template<class R = spirit::unused_type, class... S> using rule = qi::rule<it, R, S...>;

	rule<> NUL                         { lit('\0')                                          ,"nul" };
	rule<> SP                          { lit('\x20')                                      ,"space" };
	rule<> HT                          { lit('\x09')                             ,"horizontal tab" };
	rule<> ws                          { SP | HT                                     ,"whitespace" };

	rule<> CR                          { lit('\x0D')                            ,"carriage return" };
	rule<> LF                          { lit('\x0A')                                  ,"line feed" };
	rule<> CRLF                        { CR >> LF                    ,"carriage return, line feed" };

	rule<> illegal                     { NUL | CR | LF                                  ,"illegal" };
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

	const rule<> server_name
	{
		dns_name >> -(':' >> port)
		,"server name"
	};

	const rule<> sigil
	{
		lit(char(m::id::EVENT)) |
		lit(char(m::id::USER))  |
		lit(char(m::id::ROOM))  |
		lit(char(m::id::ALIAS))
		,"sigil"
	};

	const rule<> localpart
	{
		sigil >> *(char_ - ':')
		,"localpart"
	};

	const rule<> mxid
	{
		localpart >> ':' >> server_name
		,"mxid"
	};

	grammar()
	:grammar::base_type{rule<>{}}
	{}
};

struct ircd::m::id::parser
:grammar<const char *>
{
	struct validator;

	const rule<string_view> mxid_view
	{
		raw[eps > mxid]
	};

	string_view operator()(const id::sigil &, const string_view &id) const;
	string_view operator()(const string_view &id) const;
}
const ircd::m::id::parser;

ircd::string_view
ircd::m::id::parser::operator()(const id::sigil &sigil,
                                const string_view &id)
const
{
	string_view out;
	const char *start{id.data()};
	const char *const stop{id.data() + id.size()};
	qi::parse(start, stop, raw[mxid_view], out);
	return out;
}

ircd::string_view
ircd::m::id::parser::operator()(const string_view &id)
const
{
	string_view out;
	const char *start{id.data()};
	const char *const stop{id.data() + id.size()};
	qi::parse(start, stop, raw[mxid_view], out);
	return out;
}

struct ircd::m::id::parser::validator
:grammar<const char *>
{
	const rule<> valid
	{
		mxid
	};

	void operator()(const id::sigil &sigil, const string_view &id) const;
	void operator()(const string_view &id) const;
}
const validator;

void
ircd::m::id::parser::validator::operator()(const string_view &id)
const try
{
	const char *start{id.data()};
	const char *const stop{id.data() + id.size()};
	qi::parse(start, stop, eps > valid);
}
catch(const qi::expectation_failure<const char *> &e)
{
	const auto rule
	{
		ircd::string(e.what_)
	};

	throw INVALID_MXID
	{
		"Not a valid mxid because of an invalid %s.",
		between(rule, '<', '>')
	};
}

void
ircd::m::id::parser::validator::operator()(const id::sigil &sigil,
                                           const string_view &id)
const try
{
	const char *start{id.data()};
	const char *const stop{id.data() + id.size()};
	qi::parse(start, stop, eps > valid);
}
catch(const qi::expectation_failure<const char *> &e)
{
	const auto rule
	{
		ircd::string(e.what_)
	};

	throw INVALID_MXID
	{
		"Not a valid '%s' mxid because of an invalid %s.",
		reflect(sigil),
		between(rule, '<', '>')
	};
}

///////////////////////////////////////////////////////////////////////////////
//
// m/id.h
//

struct ircd::m::id::generator
{
	static string_view random_timebased(const id::sigil &, const mutable_buffer &);
	static string_view random_prefixed(const id::sigil &, const string_view &prefix, const mutable_buffer &);
};

ircd::m::id::id(const string_view &id)
:string_view{id}
{
	validate(m::sigil(id), id);
}

ircd::m::id::id(const id::sigil &sigil,
                const string_view &id)
:string_view{id}
{
	validate(sigil, id);
}

ircd::m::id::id(const id::sigil &sigil,
                const mutable_buffer &buf,
                const string_view &id)
try
:string_view
{[&sigil, &buf, &id]
{
	const string_view src
	{
		buffer::data(buf),
		buffer::data(buf) != id.data()? strlcpy(buffer::data(buf), id, buffer::size(buf)) : id.size()
	};

	return parser(sigil, src);
}()}
{
}
catch(const qi::expectation_failure<const char *> &e)
{
	const auto rule
	{
		ircd::string(e.what_)
	};

	throw INVALID_MXID
	{
		"Not a valid '%s' mxid because of an invalid %s.",
		reflect(sigil),
		between(rule, '<', '>')
	};
}

ircd::m::id::id(const enum sigil &sigil,
                const mutable_buffer &buf,
                const string_view &name,
                const string_view &host)
:string_view{[&]() -> string_view
{
	using buffer::data;
	using buffer::size;

	const size_t &max{size(buf)};
	if(!max)
		return {};

	size_t len(0);
	if(!startswith(name, sigil))
		buf[len++] = char(sigil);

	const auto has_sep
	{
		std::count(std::begin(name), std::end(name), ':')
	};

	if(!has_sep && host.empty())
	{
		len += strlcpy(data(buf) + len, name, max - len);
	}
	else if(!has_sep && !host.empty())
	{
		len += fmt::snprintf(data(buf) + len, max - len, "%s:%s",
		                     name,
		                     host);
	}
	else if(has_sep == 1 && !host.empty() && !split(name, ':').second.empty())
	{
		len += strlcpy(data(buf) + len, name, max - len);
	}
	else if(has_sep >= 1 && !host.empty())
	{
		if(split(name, ':').second != host)
			throw INVALID_MXID("MXID must be on host '%s'", host);

		len += strlcpy(data(buf) + len, name, max - len);
	}
	//else throw INVALID_MXID("Not a valid '%s' mxid", reflect(sigil));

	return { data(buf), len };
}()}
{
	validate(sigil, *this);
}

ircd::m::id::id(const enum sigil &sigil,
                const mutable_buffer &buf,
                const generate_t &,
                const string_view &host)
:string_view{[&]
{
	char name[64]; switch(sigil)
	{
		case sigil::USER:
			generator::random_prefixed(sigil::USER, "guest", name);
			break;

		case sigil::ALIAS:
			generator::random_prefixed(sigil::ALIAS, "", name);
			break;

		default:
			generator::random_timebased(sigil, name);
			break;
	};

	const auto len
	{
		fmt::sprintf(buf, "%s:%s", name, host)
	};

	return string_view { buffer::data(buf), size_t(len) };
}()}
{
}

ircd::string_view
ircd::m::id::generator::random_prefixed(const id::sigil &sigil,
                                        const string_view &prefix,
                                        const mutable_buffer &buf)
{
	using buffer::data;

	const auto len
	{
		fmt::sprintf(buf, "%c%s%u", char(sigil), prefix, rand::integer())
	};

	return { data(buf), size_t(len) };
}

ircd::string_view
ircd::m::id::generator::random_timebased(const id::sigil &sigil,
                                         const mutable_buffer &buf)
{
	using buffer::data;
	using buffer::size;

	const auto utime(microtime());
	const auto len
	{
		snprintf(data(buf), size(buf), "%c%zd%06d", char(sigil), utime.first, utime.second)
	};

	return { data(buf), size_t(len) };
}

void
ircd::m::validate(const id::sigil &sigil,
                  const string_view &id)
try
{
	validator(sigil, id);
}
catch(const std::exception &e)
{
	throw INVALID_MXID
	{
		"Not a valid '%s' mxid: %s",
		reflect(sigil),
		e.what()
	};
}

bool
ircd::m::valid(const id::sigil &sigil,
               const string_view &id)
noexcept try
{
	validator(sigil, id);
	return true;
}
catch(...)
{
	return false;
}

bool
ircd::m::valid_local(const id::sigil &sigil,
                     const string_view &id)
{
	const auto parts(split(id, ':'));
	const auto &local(parts.first);

	// local requires a sigil plus at least one character
	if(local.size() < 2)
		return false;

	// local requires the correct sigil type
	if(!startswith(local, sigil))
		return false;

	return true;
}

bool
ircd::m::valid_sigil(const string_view &s)
try
{
	return valid_sigil(s.at(0));
}
catch(const std::out_of_range &e)
{
	return false;
}

bool
ircd::m::valid_sigil(const char &c)
{
	switch(c)
	{
		case id::EVENT:
		case id::USER:
		case id::ALIAS:
		case id::ROOM:
			return true;
	}

	return false;
}

enum ircd::m::id::sigil
ircd::m::sigil(const string_view &s)
try
{
	return sigil(s.at(0));
}
catch(const std::out_of_range &e)
{
	throw BAD_SIGIL("sigil undefined");
}

enum ircd::m::id::sigil
ircd::m::sigil(const char &c)
{
	switch(c)
	{
		case '$':  return id::EVENT;
		case '@':  return id::USER;
		case '#':  return id::ALIAS;
		case '!':  return id::ROOM;
	}

	throw BAD_SIGIL("'%c' is not a valid sigil", c);
}

const char *
ircd::m::reflect(const enum id::sigil &c)
{
	switch(c)
	{
		case id::EVENT:   return "EVENT";
		case id::USER:    return "USER";
		case id::ALIAS:   return "ALIAS";
		case id::ROOM:    return "ROOM";
	}

	return "?????";
}
