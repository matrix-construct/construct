// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	using namespace ircd::spirit;

	[[noreturn]] void failure(const qi::expectation_failure<const char *> &, const string_view &);
}

struct ircd::m::id::input
:qi::grammar<const char *, unused_type>
{
	using id = m::id;
	using it = const char *;
	template<class R = unused_type, class... S> using rule = qi::rule<it, R, S...>;

	// Sigils
	const rule<id::sigil> event_id_sigil         { lit(char(id::EVENT))          ,"event_id sigil" };
	const rule<id::sigil> user_id_sigil          { lit(char(id::USER))            ,"user_id sigil" };
	const rule<id::sigil> room_id_sigil          { lit(char(id::ROOM))            ,"room_id sigil" };
	const rule<id::sigil> room_alias_sigil       { lit(char(id::ROOM_ALIAS))   ,"room_alias sigil" };
	const rule<id::sigil> group_id_sigil         { lit(char(id::GROUP))          ,"group_id sigil" };
	const rule<id::sigil> device_sigil           { lit(char(id::DEVICE))           ,"device sigil" };
	const rule<id::sigil> sigil
	{
		event_id_sigil    |
		user_id_sigil     |
		room_id_sigil     |
		room_alias_sigil  |
		group_id_sigil    |
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
		rfc3986::parser::remote
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

struct ircd::m::id::output
:karma::grammar<char *, unused_type>
{
	using it = char *;
	template<class T = unused_type> using rule = karma::rule<it, T>;

	output()
	:output::base_type{rule<>{}}
	{}
};

struct ircd::m::id::parser
:input
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
	const char *const stop
	{
		std::min(id.end(), start + MAX_SIZE)
	};

	const bool res{qi::parse(start, stop, view_mxid, out)};
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
	const char *const stop
	{
		std::min(id.end(), start + MAX_SIZE)
	};

	const bool res(qi::parse(start, stop, view_mxid, out));
	assert(res == true);
	return out;
}
catch(const qi::expectation_failure<const char *> &e)
{
	failure(e, "mxid");
}

struct ircd::m::id::validator
:input
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
	const char *const stop
	{
		std::min(id.end(), start + MAX_SIZE)
	};

	const bool ret(qi::parse(start, stop, eps > mxid));
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
	const char *const stop
	{
		std::min(id.end(), start + MAX_SIZE)
	};

	const bool ret(qi::parse(start, stop, valid_mxid));
	assert(ret == true);
}
catch(const qi::expectation_failure<const char *> &e)
{
	failure(e, reflect(sigil));
}

//TODO: abstract this pattern with ircd::json::printer in ircd/spirit.h
struct ircd::m::id::printer
:output
{
	template<class generator,
	         class attribute>
	bool operator()(char *&out, char *const &stop, generator&&, attribute&&) const;

	template<class generator>
	bool operator()(char *&out, char *const &stop, generator&&) const;

	template<class... args>
	bool operator()(mutable_buffer &out, args&&... a) const;
}
const ircd::m::id::printer;

template<class gen,
         class attr>
bool
ircd::m::id::printer::operator()(char *&out,
                                 char *const &stop,
                                 gen&& g,
                                 attr&& a)
const
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
		maxwidth(size_t(stop - out))[std::forward<decltype(g)>(g)] | eps[throws]
	};

	return karma::generate(out, gg, std::forward<decltype(a)>(a));
}

template<class gen>
bool
ircd::m::id::printer::operator()(char *&out,
                                 char *const &stop,
                                 gen&& g)
const
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
		maxwidth(size_t(stop - out))[std::forward<decltype(g)>(g)] | eps[throws]
	};

	return karma::generate(out, gg);
}

template<class... args>
bool
ircd::m::id::printer::operator()(mutable_buffer &out,
                                 args&&... a)
const
{
	return operator()(buffer::begin(out), buffer::end(out), std::forward<args>(a)...);
}

//
// id::id
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
		startswith(local, sigil)?
			fmt::sprintf
			{
				buf, "%s:%s", local, host
			}:
			fmt::sprintf
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

	thread_local char namebuf[MAX_SIZE];
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

ircd::string_view
ircd::m::id::swap(const mutable_buffer &buf)
const
{
	return swap(*this, buf);
}

ircd::string_view
ircd::m::id::swap(const id &id,
                  const mutable_buffer &buf_)
{
	using buffer::consume;
	using buffer::copy;
	using buffer::data;

	mutable_buffer buf(buf_);
	consume(buf, copy(buf, id.host()));
	consume(buf, copy(buf, id.local()));
	return { data(buf_), data(buf) };
}

ircd::m::id
ircd::m::id::unswap(const string_view &str,
                    const mutable_buffer &buf)
{
	size_t i(0);
	for(; i < str.size(); ++i)
		if(is_sigil(str[i]))
			break;

	if(unlikely(!i || i >= str.size()))
		throw m::INVALID_MXID
		{
			"Failed to reconstruct any MXID out of '%s'", str
		};

	return id
	{
		sigil(str[i]), buf, str.substr(i), str.substr(0, i)
	};
}

bool
ircd::m::id::literal()
const
{
	static const parser::rule<string_view> rule
	{
		rfc3986::parser::ip4_literal |
		rfc3986::parser::ip6_literal
	};

	const auto &hostname
	{
		this->hostname()
	};

	const char *start(hostname.begin()), *const stop(hostname.end());
	return qi::parse(start, stop, rule);
}

uint16_t
ircd::m::id::port()
const
{
	static const auto &host{rfc3986::parser::host};
	static const auto &port{rfc3986::parser::port};
	static const parser::rule<uint16_t> rule
	{
		omit[parser.prefix >> ':' >> host >> ':'] >> port
	};

	uint16_t ret{0};
	auto *start{begin()};
	const auto res
	{
		qi::parse(start, end(), rule, ret)
	};

	assert(res || ret == 0);
	return ret;
}

ircd::string_view
ircd::m::id::hostname()
const
{
	static const parser::rule<string_view> host
	{
		rfc3986::parser::host
	};

	static const parser::rule<string_view> rule
	{
		omit[parser.prefix >> ':'] >> raw[host]
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
	if(empty(id))
		return false;

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
	static const auto &test
	{
		m::id::parser.prefix
	};

	const char *start(data(id)), *const &stop
	{
		start + std::min(size(id), id::MAX_SIZE)
	};

	return !empty(id) &&
	       id.at(0) == sigil &&
	       qi::parse(start, stop, test) &&
	       start == stop;
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
	static const auto &test
	{
		m::id::parser.prefix
	};

	const char *start(data(id)), *const &stop
	{
		start + std::min(size(id), id::MAX_SIZE)
	};

	return !empty(id) &&
	       id.at(0) == sigil &&
	       qi::parse(start, stop, test);
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
	const char *start(&c), *const stop(start + 1);
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
	throw BAD_SIGIL
	{
		"no sigil provided"
	};
}

enum ircd::m::id::sigil
ircd::m::sigil(const char &c)
{
	id::sigil ret;
	const char *start(&c), *const stop(&c + 1);
	if(!qi::parse(start, stop, id::parser.sigil, ret))
		throw BAD_SIGIL
		{
			"not a valid sigil"
		};

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
		"Not a valid %s because of an invalid %s.",
		goal,
		between(rule, '<', '>')
	};
}
