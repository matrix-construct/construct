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
#include <ircd/m/m.h>

namespace ircd::m
{
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

	using karma::lit;
	using karma::char_;
	using karma::long_;
	using karma::double_;
	using karma::bool_;
	using karma::maxwidth;
	using karma::eps;
	using karma::attr_cast;

	[[noreturn]] void failure(const qi::expectation_failure<const char *> &, const string_view &);
}

template<class it>
struct ircd::m::id::input
:qi::grammar<it, spirit::unused_type>
{
	template<class R = spirit::unused_type, class... S> using rule = qi::rule<it, R, S...>;

	// Sigils
	const rule<m::id::sigil> event_id_sigil      { lit(char(m::id::EVENT))           ,"event_id sigil" };
	const rule<m::id::sigil> user_id_sigil       { lit(char(m::id::USER))             ,"user_id sigil" };
	const rule<m::id::sigil> room_id_sigil       { lit(char(m::id::ROOM))             ,"room_id sigil" };
	const rule<m::id::sigil> room_alias_sigil    { lit(char(m::id::ROOM_ALIAS))    ,"room_alias sigil" };
	const rule<m::id::sigil> group_id_sigil      { lit(char(m::id::GROUP))           ,"group_id sigil" };
	const rule<m::id::sigil> origin_sigil        { lit(char(m::id::ORIGIN))            ,"origin sigil" };
	const rule<m::id::sigil> device_sigil        { lit(char(m::id::DEVICE))            ,"device sigil" };
	const rule<m::id::sigil> sigil
	{
		event_id_sigil    |
		user_id_sigil     |
		room_id_sigil     |
		room_alias_sigil  |
		group_id_sigil    |
		origin_sigil      |
		device_sigil
		,"sigil"
	};

	// character of a localpart; must not contain ':' because that's the terminator
	const rule<> localpart_char
	{
		char_ - ':'
		,"localpart character"
	};

	// a localpart is zero or more localpart characters
	const rule<> localpart
	{
		*(localpart_char)
		,"localpart"
	};

	// character of a non-historical user_id localpart
	const rule<> user_id_char
	{
		char_("a-z/_=.\x2D") // x2d is '-'
		,"user_id character"
	};

	// a user_id localpart is 1 or more user_id localpart characters
	const rule<> user_id_localpart
	{
		+user_id_char
		,"user_id localpart"
	};

	// a prefix is a sigil and a localpart; user_id prefix
	const rule<> user_id_prefix
	{
		user_id_sigil >> user_id_localpart
		,"user_id prefix"
	};

	// a prefix is a sigil and a localpart; proper invert of user_id prefix
	const rule<> non_user_id_prefix
	{
		((!user_id_sigil) > sigil) >> localpart
		,"non user_id prefix"
	};

	// a prefix is a sigil and a localpart
	const rule<> prefix
	{
		user_id_prefix | non_user_id_prefix
		,"prefix"
	};

	//TODO: XXX-----------------
	//TODO: XXX start an ircd::net grammar; move to net::
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

	//TODO: /XXX-----------------

	/// (Appendix 4.1) Server Name
	/// A homeserver is uniquely identified by its server name. This value
	/// is used in a number of identifiers, as described below. The server
	/// name represents the address at which the homeserver in question can
	/// be reached by other homeservers. The complete grammar is:
	/// `server_name = dns_name [ ":" port]`
	/// `dns_name = host`
	/// `port = *DIGIT`
	/// where host is as defined by RFC3986, section 3.2.2. Examples of valid
	/// server names are:
	/// `matrix.org`
	/// `matrix.org:8888`
	/// `1.2.3.4` (IPv4 literal)
	/// `1.2.3.4:1234` (IPv4 literal with explicit port)
	/// `[1234:5678::abcd]` (IPv6 literal)
	/// `[1234:5678::abcd]:5678` (IPv6 literal with explicit port)
	const rule<> server_name
	{
		dns_name >> -(':' >> port)
		,"server name"
	};

	const rule<> mxid
	{
		prefix >> ':' >> server_name
		,"mxid"
	};

	input()
	:input::base_type{rule<>{}}
	{}
};

template<class it>
struct ircd::m::id::output
:karma::grammar<it, spirit::unused_type>
{
	template<class T = spirit::unused_type> using rule = karma::rule<it, T>;

	output()
	:output::base_type{rule<>{}}
	{}
};

struct ircd::m::id::parser
:input<const char *>
{
	string_view operator()(const id::sigil &, const string_view &id) const;
	string_view operator()(const string_view &id) const;
}
const ircd::m::id::parser;

ircd::string_view
ircd::m::id::parser::operator()(const id::sigil &sigil,
                                const string_view &id)
const try
{
	const rule<string_view> view_mxid
	{
		raw[&lit(char(sigil)) > mxid]
		,"mxid"
	};

	string_view out;
	const char *start{id.data()};
	const char *const stop{id.data() + id.size()};
	qi::parse(start, stop, eps > view_mxid, out);
	return out;
}
catch(const qi::expectation_failure<const char *> &e)
{
	failure(e, reflect(sigil));
}

ircd::string_view
ircd::m::id::parser::operator()(const string_view &id)
const try
{
	static const rule<string_view> view_mxid
	{
		raw[mxid]
	};

	string_view out;
	const char *start{id.data()};
	const char *const stop{id.data() + id.size()};
	qi::parse(start, stop, eps > view_mxid, out);
	return out;
}
catch(const qi::expectation_failure<const char *> &e)
{
	failure(e, "mxid");
}

struct ircd::m::id::validator
:input<const char *>
{
	void operator()(const id::sigil &sigil, const string_view &id) const;
	void operator()(const string_view &id) const;
}
const ircd::m::id::validator;

void
ircd::m::id::validator::operator()(const string_view &id)
const try
{
	const char *start{id.data()};
	const char *const stop{id.data() + id.size()};
	qi::parse(start, stop, eps > mxid);
}
catch(const qi::expectation_failure<const char *> &e)
{
	failure(e, "mxid");
}

void
ircd::m::id::validator::operator()(const id::sigil &sigil,
                                   const string_view &id)
const try
{
	const rule<string_view> valid_mxid
	{
		&lit(char(sigil)) > mxid
		,"mxid"
	};

	const char *start{id.data()};
	const char *const stop{id.data() + id.size()};
	qi::parse(start, stop, eps > valid_mxid);
}
catch(const qi::expectation_failure<const char *> &e)
{
	failure(e, reflect(sigil));
}

//TODO: abstract this pattern with ircd::json::printer in ircd/spirit.h
struct ircd::m::id::printer
:output<const char *>
{
	static string_view random_alpha(const mutable_buffer &buf, const size_t &len);
	static string_view random_timebased(const mutable_buffer &);
	static string_view random_prefixed(const string_view &prefix, const mutable_buffer &);

	template<class generator,
	         class attribute>
	bool operator()(char *&out, char *const &stop, generator&& g, attribute&& a) const
	{
		const auto throws{[&out, &stop]
		{
			throw INVALID_MXID
			{
				"Failed to print attribute '%s' generator '%s' (%zd bytes in buffer)",
				demangle<decltype(a)>(),
				demangle<decltype(g)>(),
				size_t(stop - out)
			};
		}};

		const auto gg
		{
			maxwidth(size_t(stop - out))[std::forward<generator>(g)] | eps[throws]
		};

		return karma::generate(out, gg, std::forward<attribute>(a));
	}

	template<class generator>
	bool operator()(char *&out, char *const &stop, generator&& g) const
	{
		const auto throws{[&out, &stop]
		{
			throw INVALID_MXID
			{
				"Failed to print generator '%s' (%zd bytes in buffer)",
				demangle<decltype(g)>(),
				size_t(stop - out)
			};
		}};

		const auto gg
		{
			maxwidth(size_t(stop - out))[std::forward<generator>(g)] | eps[throws]
		};

		return karma::generate(out, gg);
	}

	template<class... args>
	bool operator()(mutable_buffer &out, args&&... a) const
	{
		return operator()(buffer::begin(out), buffer::end(out), std::forward<args>(a)...);
	}
}
const ircd::m::id::printer;

ircd::string_view
ircd::m::id::printer::random_prefixed(const string_view &prefix,
                                      const mutable_buffer &buf)
{
	using buffer::data;

	const auto len
	{
		fmt::sprintf(buf, "%s%u", prefix, rand::integer())
	};

	return { data(buf), size_t(len) };
}

ircd::string_view
ircd::m::id::printer::random_timebased(const mutable_buffer &buf)
{
	using buffer::data;
	using buffer::size;

	const auto utime(microtime());
	const auto len
	{
		snprintf(data(buf), size(buf), "%zd%06d", utime.first, utime.second)
	};

	return { data(buf), size_t(len) };
}

ircd::string_view
ircd::m::id::printer::random_alpha(const mutable_buffer &buf,
                                   const size_t &len)
{
	using buffer::data;
	using buffer::size;

	const mutable_buffer out
	{
		data(buf), std::min(size(buf), len)
	};

	return rand::string(rand::dict::alpha, out);
}

//
// id
//

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

ircd::m::id::id(const enum sigil &sigil,
                const mutable_buffer &buf,
                const string_view &local,
                const string_view &host)
:string_view{[&sigil, &buf, &local, &host]
{
	const string_view src
	{
		buffer::data(buf), size_t(fmt::sprintf(buf, "%c%s:%s", char(sigil), local, host))
	};

	return parser(sigil, src);
}()}
{
}

ircd::m::id::id(const id::sigil &sigil,
                const mutable_buffer &buf,
                const string_view &id)
:string_view{[&sigil, &buf, &id]
{
	const auto len
	{
		buffer::data(buf) != id.data()?
			strlcpy(buffer::data(buf), id, buffer::size(buf)):
			id.size()
	};

	const string_view src
	{
		buffer::data(buf), len
	};

	return parser(sigil, src);
}()}
{
}

ircd::m::id::id(const enum sigil &sigil,
                const mutable_buffer &buf,
                const generate_t &,
                const string_view &host)
:string_view{[&]
{
	//TODO: output grammar

	char namebuf[64];
	string_view name; switch(sigil)
	{
		case sigil::USER:
			name = printer::random_prefixed("guest", namebuf);
			break;

		case sigil::ROOM_ALIAS:
			name = printer::random_prefixed("", namebuf);
			break;

		case sigil::DEVICE:
			name = printer::random_alpha(namebuf, 16);
			break;

		default:
			name = printer::random_timebased(namebuf);
			break;
	};

	return fmt::sprintf
	{
		buf, "%c%s:%s", char(sigil), name, host
	};
}()}
{
}

uint16_t
ircd::m::id::hostport()
const try
{
	//TODO: grammar
	const auto port
	{
		split(host(), ':').second
	};

	return port? lex_cast<uint16_t>(port) : 8448;
}
catch(const std::exception &e)
{
	return 8448;
}

ircd::string_view
ircd::m::id::hostname()
const
{
	//TODO: grammar
	return rsplit(host(), ':').first;
}

ircd::string_view
ircd::m::id::name()
const
{
	//TODO: grammar
	return lstrip(local(), at(0));
}

ircd::string_view
ircd::m::id::host()
const
{
	//TODO: grammar
	return split(*this, ':').second;
}

ircd::string_view
ircd::m::id::local()
const
{
	//TODO: grammar
	return split(*this, ':').first;
}

bool
ircd::m::my(const id &id)
{
	return my_host(id.host());
}

void
ircd::m::validate(const id::sigil &sigil,
                  const string_view &id)
try
{
	id::validator(sigil, id);
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
	id::validator(sigil, id);
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
	static const auto test
	{
		&lit(char(sigil)) > m::id::parser.prefix
	};

	const char *start{id.data()};
	const char *const stop{id.data() + id.size()};
	return qi::parse(start, stop, test);
}

bool
ircd::m::has_sigil(const string_view &s)
try
{
	return is_sigil(s.at(0));
}
catch(const std::out_of_range &e)
{
	return false;
}

bool
ircd::m::is_sigil(const char &c)
{
	const char *start{&c};
	const char *const stop{start + 1};
	return qi::parse(start, stop, id::parser.sigil);
}

enum ircd::m::id::sigil
ircd::m::sigil(const string_view &s)
try
{
	return sigil(s.at(0));
}
catch(const std::out_of_range &e)
{
	throw BAD_SIGIL("no sigil provided");
}

enum ircd::m::id::sigil
ircd::m::sigil(const char &c)
{
	id::sigil ret;
	const char *start{&c};
	const char *const stop{&c + 1};
	if(!qi::parse(start, stop, id::parser.sigil, ret))
		throw BAD_SIGIL("'%c' is not a valid sigil", c);

	assert(start == stop);
	return ret;
}

ircd::string_view
ircd::m::reflect(const id::sigil &c)
{
	switch(c)
	{
		case id::EVENT:        return "EVENT"_sv;
		case id::USER:         return "USER"_sv;
		case id::ROOM:         return "ROOM"_sv;
		case id::ROOM_ALIAS:   return "ROOM_ALIAS"_sv;
		case id::GROUP:        return "GROUP"_sv;
		case id::ORIGIN:       return "ORIGIN"_sv;
		case id::DEVICE:       return "DEVICE"_sv;
	}

	return "?????"_sv;
}

void
ircd::m::failure(const qi::expectation_failure<const char *> &e,
                 const string_view &goal)
{
	auto rule
	{
		ircd::string(e.what_)
	};

	throw INVALID_MXID
	{
		"Not a valid %s because of an invalid %s.", goal, between(rule, '<', '>')
	};
}
