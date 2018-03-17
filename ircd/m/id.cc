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
	using namespace ircd::spirit;

	[[noreturn]] void failure(const qi::expectation_failure<const char *> &, const string_view &);
}

template<class it>
struct ircd::m::id::input
:qi::grammar<it, unused_type>
{
	template<class R = unused_type, class... S> using rule = qi::rule<it, R, S...>;

	// Sigils
	const rule<m::id::sigil> event_id_sigil      { lit(char(m::id::EVENT))           ,"event_id sigil" };
	const rule<m::id::sigil> user_id_sigil       { lit(char(m::id::USER))             ,"user_id sigil" };
	const rule<m::id::sigil> room_id_sigil       { lit(char(m::id::ROOM))             ,"room_id sigil" };
	const rule<m::id::sigil> room_alias_sigil    { lit(char(m::id::ROOM_ALIAS))    ,"room_alias sigil" };
	const rule<m::id::sigil> group_id_sigil      { lit(char(m::id::GROUP))           ,"group_id sigil" };
	const rule<m::id::sigil> node_sigil          { lit(char(m::id::NODE))                ,"node sigil" };
	const rule<m::id::sigil> device_sigil        { lit(char(m::id::DEVICE))            ,"device sigil" };
	const rule<m::id::sigil> sigil
	{
		event_id_sigil    |
		user_id_sigil     |
		room_id_sigil     |
		room_alias_sigil  |
		group_id_sigil    |
		node_sigil        |
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
		*localpart_char
		,"localpart"
	};

	// character of a non-historical user_id localpart
	const rule<> user_id_char
	{
		char_(rfc1459::character::charset(rfc1459::character::NICK) + "./=")
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

	//TODO: ---- share grammar with rfc3986.cc

	const rule<uint16_t> port
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

	const rule<> dns_name
	{
		ip6_literal | ip4_literal | hostname
		,"DNS name"
	};

	//TODO: /---- share grammar with rfc3986.cc

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
		dns_name >> -(':' > port)
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
:karma::grammar<it, unused_type>
{
	template<class T = unused_type> using rule = karma::rule<it, T>;

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
	const rule<> sigil_type
	{
		&lit(char(sigil))
		,"sigil type"
	};

	const rule<string_view> view_mxid
	{
		raw[eps > (sigil_type > mxid)]
	};

	string_view out;
	const char *start{id.begin()};
	const bool res(qi::parse(start, id.end(), view_mxid, out));
	assert(res == true);
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
		raw[eps > mxid]
	};

	string_view out;
	const char *start{id.begin()};
	const bool res(qi::parse(start, id.end(), view_mxid, out));
	assert(res == true);
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
	const char *start{id.begin()};
	const bool ret(qi::parse(start, id.end(), eps > mxid));
	assert(ret == true);
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
	const rule<> sigil_type
	{
		&lit(char(sigil))
		,"sigil type"
	};

	const rule<> valid_mxid
	{
		eps > (sigil_type > mxid)
	};

	const char *start{id.begin()};
	const bool ret(qi::parse(start, id.end(), valid_mxid));
	assert(ret == true);
}
catch(const qi::expectation_failure<const char *> &e)
{
	failure(e, reflect(sigil));
}

//TODO: abstract this pattern with ircd::json::printer in ircd/spirit.h
struct ircd::m::id::printer
:output<const char *>
{
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
		startswith(local, sigil)? fmt::sprintf
		{
			buf, "%s:%s", local, host
		}
		: fmt::sprintf
		{
			buf, "%c%s:%s", char(sigil), local, host
		}
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
			name = fmt::sprintf
			{
				namebuf, "guest%lu", rand::integer()
			};
			break;

		case sigil::ROOM_ALIAS:
			name = fmt::sprintf
			{
				namebuf, "%lu", rand::integer()
			};
			break;

		case sigil::ROOM:
		{
			mutable_buffer buf{namebuf, 16};
			consume(buf, buffer::copy(buf, "AAAA"_sv)); //TODO: cluster euid
			rand::string(rand::dict::alnum, buf);
			name = {namebuf, 16};
			break;
		}

		case sigil::DEVICE:
		{
			static const auto &dict{rand::dict::alpha};
			name = rand::string(dict, mutable_buffer{namebuf, 16});
			break;
		}

		default:
			name = fmt::sprintf
			{
				namebuf, "%c%lu", rand::character(), rand::integer()
			};
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
ircd::m::id::port()
const
{
	static const parser::rule<uint16_t> rule
	{
		omit[parser.prefix >> ':' >> parser.dns_name >> ':'] >> parser.port
	};

	uint16_t ret{8448};
	auto *start{begin()};
	const auto res
	{
		qi::parse(start, end(), rule, ret)
	};

	assert(res || ret == 8448);
	return ret;
}

ircd::string_view
ircd::m::id::hostname()
const
{
	static const parser::rule<string_view> dns_name
	{
		parser.dns_name
	};

	static const parser::rule<string_view> rule
	{
		omit[parser.prefix >> ':'] >> raw[dns_name]
	};

	string_view ret;
	auto *start{begin()};
	const auto res
	{
		qi::parse(start, end(), rule, ret)
	};

	assert(res == true);
	assert(!ret.empty());
	return ret;
}

ircd::string_view
ircd::m::id::localname()
const
{
	auto ret{local()};
	assert(!ret.empty());
	ret.pop_front();
	return ret;
}

ircd::string_view
ircd::m::id::host()
const
{
	static const parser::rule<string_view> server_name
	{
		parser.server_name
	};

	static const parser::rule<string_view> rule
	{
		omit[parser.prefix >> ':'] >> raw[server_name]
	};

	string_view ret;
	auto *start{begin()};
	const auto res
	{
		qi::parse(start, end(), rule, ret)
	};

	assert(res == true);
	assert(!ret.empty());
	return ret;
}

ircd::string_view
ircd::m::id::local()
const
{
	static const parser::rule<string_view> prefix
	{
		parser.prefix
	};

	static const parser::rule<string_view> rule
	{
		eps > raw[prefix]
	};

	string_view ret;
	auto *start{begin()};
	qi::parse(start, end(), rule, ret);
	assert(!ret.empty());
	return ret;
}

bool
ircd::m::my(const id &id)
{
	return my_host(id.host());
}

void
ircd::m::validate(const id::sigil &sigil,
                  const string_view &id)
{
	id::validator(sigil, id);
}

bool
ircd::m::valid(const id::sigil &sigil,
               const string_view &id)
noexcept try
{
	validate(sigil, id);
	return true;
}
catch(...)
{
	return false;
}

bool
ircd::m::valid_local_only(const id::sigil &sigil,
                          const string_view &id)
noexcept try
{
	static const auto test
	{
		m::id::parser.prefix
	};

	const char *start{begin(id)};
	const char *const &stop{end(id)};
	return id.at(0) == sigil && qi::parse(start, stop, test) && start == stop;
}
catch(...)
{
	return false;
}

bool
ircd::m::valid_local(const id::sigil &sigil,
                     const string_view &id)
noexcept try
{
	static const auto test
	{
		m::id::parser.prefix
	};

	const char *start{begin(id)};
	return id.at(0) == sigil && qi::parse(start, end(id), test);
}
catch(...)
{
	return false;
}

bool
ircd::m::has_sigil(const string_view &s)
noexcept try
{
	return is_sigil(s.at(0));
}
catch(...)
{
	return false;
}

bool
ircd::m::is_sigil(const char &c)
noexcept
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
		case id::NODE:         return "NODE"_sv;
		case id::DEVICE:       return "DEVICE"_sv;
	}

	return "?????"_sv;
}

void
ircd::m::failure(const qi::expectation_failure<const char *> &e,
                 const string_view &goal)
{
	const auto rule
	{
		ircd::string(e.what_)
	};

	throw INVALID_MXID
	{
		"Not a valid %s because of an invalid %s.", goal, between(rule, '<', '>')
	};
}
