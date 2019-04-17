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

namespace ircd::rfc1459
{
	using namespace ircd::spirit;
}

namespace ircd { namespace rfc1459 { namespace parse
__attribute__((visibility("hidden")))
{
	template<class it, class top> struct grammar;
	struct capstan extern const capstan;
	struct head extern const head;
}}}

namespace ircd { namespace rfc1459 { namespace gen
__attribute__((visibility("hidden")))
{
	using ircd::spirit::buffer;

	template<class it, class top> struct grammar;
}}}

BOOST_FUSION_ADAPT_STRUCT
(
	ircd::rfc1459::pfx
	,( decltype(ircd::rfc1459::pfx::nick),  nick )
	,( decltype(ircd::rfc1459::pfx::user),  user )
	,( decltype(ircd::rfc1459::pfx::host),  host )
)

BOOST_FUSION_ADAPT_STRUCT
(
	ircd::rfc1459::line
	,( decltype(ircd::rfc1459::line::pfx),   pfx  )
	,( decltype(ircd::rfc1459::line::cmd),   cmd  )
	,( decltype(ircd::rfc1459::line::parv),  parv )
)

/* The grammar template class.
 * This aggregates all the rules under one template to make composing them easier.
 *
 * The grammar template is instantiated by individual parsers depending on the goal
 * for the specific parse, or the "top level." The first top-level was an IRC line, so
 * a class was created `struct head` specifying grammar::line as the top rule, and
 * rfc1459::line as the top output target to parse into.
 */
template<class it,
         class top>
struct ircd::rfc1459::parse::grammar
:qi::grammar<it, top>
{
	qi::rule<it> space;
	qi::rule<it> colon;
	qi::rule<it> nulcrlf;
	qi::rule<it> spnulcrlf;
	qi::rule<it> terminator;

	qi::rule<it, rfc1459::host> hostname;
	qi::rule<it, rfc1459::host> server;
	qi::rule<it, rfc1459::user> user;
	qi::rule<it, rfc1459::nick> nick;
	qi::rule<it, rfc1459::pfx> prefix;

	qi::rule<it, string_view> trailing;
	qi::rule<it, string_view> middle;
	qi::rule<it, rfc1459::parv> params;

	qi::rule<it, string_view> numeric;
	qi::rule<it, rfc1459::cmd> command;

	qi::rule<it, rfc1459::line> line;
	qi::rule<it, std::deque<rfc1459::line>> tape;

	grammar(qi::rule<it, top> &top_rule);
};

template<class it,
         class top>
ircd::rfc1459::parse::grammar<it, top>::grammar(qi::rule<it, top> &top_rule)
:qi::grammar<it, top>::base_type
{
	top_rule
}
,space // A single space character
{
	/* TODO: RFC says:
	 *   1) <SPACE> is consists only of SPACE character(s) (0x20).
	 *      Specially notice that TABULATION, and all other control
	 *      characters are considered NON-WHITE-SPACE.
	 * But our table in this namespace has control characters labeled as SPACE.
	 * This needs to be fixed.
	 */

	//char_(charset(character::SPACE))
	lit(' ')
	,"space"
}
,colon // A single colon character
{
	lit(':')
	,"colon"
}
,nulcrlf // Match on NUL or CR or LF
{
	lit('\0') | lit('\r') | lit('\n')
	,"nulcrlf"
}
,spnulcrlf // Match on space or nulcrlf
{
	space | nulcrlf
	,"spnulcrlf"
}
,terminator // The message terminator
{
	lit('\r') >> lit('\n')
	,"terminator"
}
,hostname // A valid hostname
{
	raw[+char_(charset(character::HOST))] // TODO: https://tools.ietf.org/html/rfc952
	,"hostname"
}
,server // A valid servername
{
	hostname
	,"server"
}
,user // A valid username
{
	raw[+char_(charset(character::USER))]
	,"user"
}
,nick // A valid nickname, leading letter followed by any NICK chars
{
	raw[char_(charset(character::ALPHA)) >> *char_(charset(character::NICK))]
	,"nick"
}
,prefix // A valid prefix, required name, optional user and host (or empty placeholders)
{
	colon >> (nick | server) >> -(lit('!') >> user) >> -(lit('@') >> hostname)
	,"prefix"
}
,trailing // Trailing string pinch
{
	colon >> raw[+(char_ - nulcrlf)]
	,"trailing"
}
,middle // Spaced parameters
{
	!colon >> raw[+(char_ - spnulcrlf)]
	,"middle"
}
,params // Parameter vector
{
	*(+space >> middle) >> -(+space >> trailing)
	,"params"
}
,numeric // \d\d\d numeric
{
	repeat(3)[char_(charset(character::DIGIT))]
	,"numeric"
}
,command // A command or numeric
{
	raw[+char_(charset(character::ALPHA)) | numeric]
	,"command"
}
,line
{
	-(prefix >> +space) >> command >> params
	,"line"
}
,tape
{
	+(-line >> +terminator)
	,"tape"
}
{
}

// Instantiate the input grammar to parse a const char* buffer into an rfc1459::line object.
// The top rule is inherited and then specified as grammar::line, which is compatible
// with an rfc1459::line object.
//
struct ircd::rfc1459::parse::head
:parse::grammar<const char *, rfc1459::line>
{
    head(): grammar{line} {}
}
const ircd::rfc1459::parse::head;

// Instantiate the input grammar to parse a const char* buffer into an rfc1459::tape object.
// The top rule is now grammar::tape and the target object is an rfc1459::tape deque.
//
struct ircd::rfc1459::parse::capstan
:parse::grammar<const char *, std::deque<rfc1459::line>>
{
    capstan(): grammar{tape} {}
}
const ircd::rfc1459::parse::capstan;

template<class it,
         class top>
struct ircd::rfc1459::gen::grammar
:karma::grammar<it, top>
{
	std::string trail_save;

	karma::rule<it> space;
	karma::rule<it> colon;
	karma::rule<it> terminator;

	karma::rule<it, rfc1459::host> hostname;
	karma::rule<it, rfc1459::user> user;
	karma::rule<it, rfc1459::nick> nick;
	karma::rule<it, rfc1459::pfx> prefix;
	karma::rule<it, rfc1459::pfx> prefix_optionals;

	karma::rule<it, string_view> trailing;
	karma::rule<it, string_view> middle;
	karma::rule<it, rfc1459::parv> params;

	karma::rule<it, rfc1459::cmd> command_numeric;
	karma::rule<it, rfc1459::cmd> command_alpha;
	karma::rule<it, rfc1459::cmd> command;
	karma::rule<it, rfc1459::line> line;

	grammar(karma::rule<it, top> &top_rule);
};

template<class it,
         class top>
ircd::rfc1459::gen::grammar<it, top>::grammar(karma::rule<it, top> &top_rule)
:karma::grammar<it, top>::base_type
{
	top_rule
}
,space // A single space character
{
	lit(' ')
	,"space"
}
,colon // A single colon character
{
	lit(':')
	,"colon"
}
,terminator // The message terminator
{
	lit('\r') << lit('\n')
	,"terminator"
}
,hostname // A valid hostname
{
	+char_(charset(character::HOST)) // TODO: https://tools.ietf.org/html/rfc952
	,"hostname"
}
,user // A valid username
{
	+char_(charset(character::USER))
	,"user"
}
,nick // A valid nickname, leading letter followed by any NICK chars
{
	buffer[char_(charset(character::ALPHA)) << *char_(charset(character::NICK))]
	,"nick"
}
,prefix
{
	colon << nick << lit('!') << user << lit('@') << hostname
	,"prefix"
}
,prefix_optionals
{
	colon        << (nick      | lit('*'))
	<< lit('!')  << (user      | lit('*'))
	<< lit('@')  << (hostname  | lit('*'))
	,"prefix_optionals"
}
,trailing
{
	colon << +(~char_("\r\n"))
	,"trailing"
}
,middle // Spaced parameters
{
	~char_(":\x20\r\n") << *(~char_("\x20\r\n"))
	,"middle"
}
,params //TODO: this doesn't work yet, don't use
{
	*(middle % space) << buffer[-trailing]
	,"params"
}
,command_numeric // \d\d\d numeric
{
	repeat(3)[char_(charset(character::DIGIT))]
	,"command_numeric"
}
,command_alpha
{
	+char_(charset(character::ALPHA))
	,"command_alpha"
}
,command
{
	command_alpha | command_numeric
	,"command"
}
,line
{
	prefix << command << space << params << terminator
	,"line"
}
{
}

//NOTE: unterminated
//TODO: Fix carriage
std::ostream &
ircd::rfc1459::operator<<(std::ostream &s, const line &line)
{
	struct carriage
	:gen::grammar<karma::ostream_iterator<char>, rfc1459::line>
	{
		carriage(): grammar{grammar::line} {}
	}
	static const carriage;

	if(!line.pfx.empty())
		s << line.pfx << ' ';

	s << line.cmd;

	if(!line.parv.empty())
		s << ' ' << line.parv;

	return s;
}

std::ostream &
ircd::rfc1459::operator<<(std::ostream &s, const parv &parv)
{
	using karma::delimit;

	struct generate_middle
	:gen::grammar<karma::ostream_iterator<char>, string_view>
	{
		generate_middle(): grammar{grammar::middle} {}
	}
	static const generate_middle;

	struct generate_trailing
	:gen::grammar<karma::ostream_iterator<char>, string_view>
	{
		generate_trailing(): grammar{grammar::trailing} {}
	}
	static const generate_trailing;

	ssize_t i(0);
	karma::ostream_iterator<char> osi(s);
	for(; i < ssize_t(parv.size()) - 1; ++i)
		if(!karma::generate(osi, delimit[generate_middle], parv.at(i)))
			throw syntax_error("Invalid middle parameter");

	if(!parv.empty())
		if(!karma::generate(osi, generate_trailing, parv.at(parv.size() - 1)))
			throw syntax_error("Invalid trailing parameter");

	return s;
}

std::ostream &
ircd::rfc1459::operator<<(std::ostream &s, const cmd &cmd)
{
	struct generate_command
	:gen::grammar<karma::ostream_iterator<char>, rfc1459::cmd>
	{
		generate_command(): grammar{grammar::command} {}
	}
	static const generate_command;

	karma::ostream_iterator<char> osi(s);
	if(!karma::generate(osi, generate_command, cmd))
		throw syntax_error("Bad command or numeric name");

	return s;
}

std::ostream &
ircd::rfc1459::operator<<(std::ostream &s, const pfx &pfx)
{
	struct generate_prefix
	:gen::grammar<karma::ostream_iterator<char>, rfc1459::pfx>
	{
		generate_prefix(): grammar{grammar::prefix} {}
	}
	static const generate_prefix;

	karma::ostream_iterator<char> osi(s);
	if(!karma::generate(osi, generate_prefix, pfx))
		throw syntax_error("Invalid prefix");

	return s;
}

ircd::rfc1459::line::line(const char *&start,
                          const char *const &stop)
try
{
	static const auto &grammar
	{
		parse::head
	};

	qi::parse(start, stop, grammar, *this);
}
catch(const qi::expectation_failure<const char *> &e)
{
	throw syntax_error
	{
		"@%d expecting :%s",
		int(e.last - e.first),
		ircd::string(e.what_).c_str()
	};
}

bool
ircd::rfc1459::line::empty()
const
{
	return pfx.empty() &&
	       cmd.empty() &&
	       parv.empty();
}

bool
ircd::rfc1459::pfx::empty()
const
{
	return nick.empty() &&
	       user.empty() &&
	       host.empty();
}

std::string
ircd::rfc1459::character::charset(const attr &attr)
{
	uint8_t buf[256];
	const size_t len(charset(attr, buf, sizeof(buf)));
	return { reinterpret_cast<const char *>(buf), len };
}

size_t
ircd::rfc1459::character::charset(const attr &attr,
                                  uint8_t *const &buf,
                                  const size_t &max)
{
	const auto len(gather(attr, buf, max));
	std::sort(buf, buf + len, []
	(const uint8_t &a, const uint8_t &b)
	{
		// Ensure special char '-' is always at the front. Also preserve
		// the reverse ordering from gather() so NUL is always at the end.
		return a == '-'?  true:
		       b == '-'?  false:
		                  a > b;
	});

	return len;
}

std::string
ircd::rfc1459::character::gather(const attr &attr)
{
	uint8_t buf[256];
	const size_t len(gather(attr, buf, sizeof(buf)));
	return { reinterpret_cast<const char *>(buf), len };
}

size_t
ircd::rfc1459::character::gather(const attr &attr,
                                 uint8_t *const &buf,
                                 const size_t &max)
{
	size_t ret(0);
	for(ssize_t i(attrs.size() - 1); i >= 0 && ret < max; --i)
		if(is(i, attr))
			buf[ret++] = i;

	return ret;
}

decltype(ircd::rfc1459::character::tolower_tab)
ircd::rfc1459::character::tolower_tab
{
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
	0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14,
	0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,
	0x1e, 0x1f,
	' ', '!', '"', '#', '$', '%', '&', 0x27, '(', ')',
	'*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	':', ';', '<', '=', '>', '?',
	'@', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
	'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
	't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~',
	'_',
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
	'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
	't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~',
	0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
	0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
	0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9,
	0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9,
	0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9,
	0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9,
	0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
	0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9,
	0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

decltype(ircd::rfc1459::character::toupper_tab)
ircd::rfc1459::character::toupper_tab
{
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
	0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14,
	0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,
	0x1e, 0x1f,
	' ', '!', '"', '#', '$', '%', '&', 0x27, '(', ')',
	'*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	':', ';', '<', '=', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I',
	'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S',
	'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^',
	0x5f,
	'`', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I',
	'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S',
	'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^',
	0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
	0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
	0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9,
	0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9,
	0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9,
	0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9,
	0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
	0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9,
	0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

/*
 * CharAttrs table
 *
 * NOTE: RFC 1459 sez: anything but a ^G, comma, or space is allowed
 * for channel names
 */
const decltype(ircd::rfc1459::character::attrs)
ircd::rfc1459::character::attrs
{{
/* 0     */  CNTRL,
/* 1     */  CNTRL | CHAN | NONEOS,
/* 2     */  CNTRL | CHAN | FCHAN | NONEOS,
/* 3     */  CNTRL | CHAN | FCHAN | NONEOS,
/* 4     */  CNTRL | CHAN | NONEOS,
/* 5     */  CNTRL | CHAN | NONEOS,
/* 6     */  CNTRL | CHAN | NONEOS,
/* 7 BEL */  CNTRL | NONEOS,
/* 8  \b */  CNTRL | CHAN | NONEOS,
/* 9  \t */  CNTRL | SPACE | CHAN | NONEOS,
/* 10 \n */  CNTRL | SPACE | CHAN | NONEOS | EOL,
/* 11 \v */  CNTRL | SPACE | CHAN | NONEOS,
/* 12 \f */  CNTRL | SPACE | CHAN | NONEOS,
/* 13 \r */  CNTRL | SPACE | CHAN | NONEOS | EOL,
/* 14    */  CNTRL | CHAN | NONEOS,
/* 15    */  CNTRL | CHAN | NONEOS,
/* 16    */  CNTRL | CHAN | NONEOS,
/* 17    */  CNTRL | CHAN | NONEOS,
/* 18    */  CNTRL | CHAN | NONEOS,
/* 19    */  CNTRL | CHAN | NONEOS,
/* 20    */  CNTRL | CHAN | NONEOS,
/* 21    */  CNTRL | CHAN | NONEOS,
/* 22    */  CNTRL | CHAN | FCHAN | NONEOS,
/* 23    */  CNTRL | CHAN | NONEOS,
/* 24    */  CNTRL | CHAN | NONEOS,
/* 25    */  CNTRL | CHAN | NONEOS,
/* 26    */  CNTRL | CHAN | NONEOS,
/* 27    */  CNTRL | CHAN | NONEOS,
/* 28    */  CNTRL | CHAN | NONEOS,
/* 29    */  CNTRL | CHAN | FCHAN | NONEOS,
/* 30    */  CNTRL | CHAN | NONEOS,
/* 31    */  CNTRL | CHAN | FCHAN | NONEOS,
/* SP    */  PRINT | SPACE,
/* !     */  PRINT | KWILD | CHAN | NONEOS,
/* "     */  PRINT | CHAN | NONEOS,
/* #     */  PRINT | MWILD | CHANPFX | CHAN | NONEOS,
/* $     */  PRINT | CHAN | NONEOS,
/* %     */  PRINT | CHAN | NONEOS,
/* &     */  PRINT | CHANPFX | CHAN | NONEOS,
/* '     */  PRINT | CHAN | NONEOS,
/* (     */  PRINT | CHAN | NONEOS,
/* )     */  PRINT | CHAN | NONEOS,
/* *     */  PRINT | KWILD | MWILD | CHAN | NONEOS,
/* +     */  PRINT | CHAN | NONEOS,
/* ,     */  PRINT | NONEOS,
/* -     */  PRINT | NICK | CHAN | NONEOS | USER | HOST,
/* .     */  PRINT | KWILD | CHAN | NONEOS | USER | HOST | SERV,
/* /     */  PRINT | CHAN | NONEOS | HOST,
/* 0     */  PRINT | DIGIT | NICK | CHAN | NONEOS | USER | HOST,
/* 1     */  PRINT | DIGIT | NICK | CHAN | NONEOS | USER | HOST,
/* 2     */  PRINT | DIGIT | NICK | CHAN | NONEOS | USER | HOST,
/* 3     */  PRINT | DIGIT | NICK | CHAN | NONEOS | USER | HOST,
/* 4     */  PRINT | DIGIT | NICK | CHAN | NONEOS | USER | HOST,
/* 5     */  PRINT | DIGIT | NICK | CHAN | NONEOS | USER | HOST,
/* 6     */  PRINT | DIGIT | NICK | CHAN | NONEOS | USER | HOST,
/* 7     */  PRINT | DIGIT | NICK | CHAN | NONEOS | USER | HOST,
/* 8     */  PRINT | DIGIT | NICK | CHAN | NONEOS | USER | HOST,
/* 9     */  PRINT | DIGIT | NICK | CHAN | NONEOS | USER | HOST,
/* :     */  PRINT | CHAN | NONEOS | HOST,
/* ;     */  PRINT | CHAN | NONEOS,
/* <     */  PRINT | CHAN | NONEOS,
/* =     */  PRINT | CHAN | NONEOS,
/* >     */  PRINT | CHAN | NONEOS,
/* ?     */  PRINT | KWILD | MWILD | CHAN | NONEOS,
/* @     */  PRINT | KWILD | MWILD | CHAN | NONEOS,
/* A     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* B     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* C     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* D     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* E     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* F     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* G     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* H     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* I     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* J     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* K     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* L     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* M     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* N     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* O     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* P     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* Q     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* R     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* S     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* T     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* U     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* V     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* W     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* X     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* Y     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* Z     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* [     */  PRINT | ALPHA | NICK | CHAN | NONEOS | USER,
/* \     */  PRINT | ALPHA | NICK | CHAN | NONEOS | USER,
/* ]     */  PRINT | ALPHA | NICK | CHAN | NONEOS | USER,
/* ^     */  PRINT | ALPHA | NICK | CHAN | NONEOS | USER,
/* _     */  PRINT | NICK | CHAN | NONEOS | USER,
/* `     */  PRINT | NICK | CHAN | NONEOS | USER,
/* a     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* b     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* c     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* d     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* e     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* f     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* g     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* h     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* i     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* j     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* k     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* l     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* m     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* n     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* o     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* p     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* q     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* r     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* s     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* t     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* u     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* v     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* w     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* x     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* y     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* z     */  PRINT | ALPHA | LET | NICK | CHAN | NONEOS | USER | HOST,
/* {     */  PRINT | ALPHA | NICK | CHAN | NONEOS | USER,
/* |     */  PRINT | ALPHA | NICK | CHAN | NONEOS | USER,
/* }     */  PRINT | ALPHA | NICK | CHAN | NONEOS | USER,
/* ~     */  PRINT | ALPHA | CHAN | NONEOS | USER,
/* del   */  CHAN | NONEOS,
/* 0x80  */  CHAN | NONEOS,
/* 0x81  */  CHAN | NONEOS,
/* 0x82  */  CHAN | NONEOS,
/* 0x83  */  CHAN | NONEOS,
/* 0x84  */  CHAN | NONEOS,
/* 0x85  */  CHAN | NONEOS,
/* 0x86  */  CHAN | NONEOS,
/* 0x87  */  CHAN | NONEOS,
/* 0x88  */  CHAN | NONEOS,
/* 0x89  */  CHAN | NONEOS,
/* 0x8A  */  CHAN | NONEOS,
/* 0x8B  */  CHAN | NONEOS,
/* 0x8C  */  CHAN | NONEOS,
/* 0x8D  */  CHAN | NONEOS,
/* 0x8E  */  CHAN | NONEOS,
/* 0x8F  */  CHAN | NONEOS,
/* 0x90  */  CHAN | NONEOS,
/* 0x91  */  CHAN | NONEOS,
/* 0x92  */  CHAN | NONEOS,
/* 0x93  */  CHAN | NONEOS,
/* 0x94  */  CHAN | NONEOS,
/* 0x95  */  CHAN | NONEOS,
/* 0x96  */  CHAN | NONEOS,
/* 0x97  */  CHAN | NONEOS,
/* 0x98  */  CHAN | NONEOS,
/* 0x99  */  CHAN | NONEOS,
/* 0x9A  */  CHAN | NONEOS,
/* 0x9B  */  CHAN | NONEOS,
/* 0x9C  */  CHAN | NONEOS,
/* 0x9D  */  CHAN | NONEOS,
/* 0x9E  */  CHAN | NONEOS,
/* 0x9F  */  CHAN | NONEOS,
/* 0xA0  */  CHAN | FCHAN | NONEOS,
/* 0xA1  */  CHAN | NONEOS,
/* 0xA2  */  CHAN | NONEOS,
/* 0xA3  */  CHAN | NONEOS,
/* 0xA4  */  CHAN | NONEOS,
/* 0xA5  */  CHAN | NONEOS,
/* 0xA6  */  CHAN | NONEOS,
/* 0xA7  */  CHAN | NONEOS,
/* 0xA8  */  CHAN | NONEOS,
/* 0xA9  */  CHAN | NONEOS,
/* 0xAA  */  CHAN | NONEOS,
/* 0xAB  */  CHAN | NONEOS,
/* 0xAC  */  CHAN | NONEOS,
/* 0xAD  */  CHAN | NONEOS,
/* 0xAE  */  CHAN | NONEOS,
/* 0xAF  */  CHAN | NONEOS,
/* 0xB0  */  CHAN | NONEOS,
/* 0xB1  */  CHAN | NONEOS,
/* 0xB2  */  CHAN | NONEOS,
/* 0xB3  */  CHAN | NONEOS,
/* 0xB4  */  CHAN | NONEOS,
/* 0xB5  */  CHAN | NONEOS,
/* 0xB6  */  CHAN | NONEOS,
/* 0xB7  */  CHAN | NONEOS,
/* 0xB8  */  CHAN | NONEOS,
/* 0xB9  */  CHAN | NONEOS,
/* 0xBA  */  CHAN | NONEOS,
/* 0xBB  */  CHAN | NONEOS,
/* 0xBC  */  CHAN | NONEOS,
/* 0xBD  */  CHAN | NONEOS,
/* 0xBE  */  CHAN | NONEOS,
/* 0xBF  */  CHAN | NONEOS,
/* 0xC0  */  CHAN | NONEOS,
/* 0xC1  */  CHAN | NONEOS,
/* 0xC2  */  CHAN | NONEOS,
/* 0xC3  */  CHAN | NONEOS,
/* 0xC4  */  CHAN | NONEOS,
/* 0xC5  */  CHAN | NONEOS,
/* 0xC6  */  CHAN | NONEOS,
/* 0xC7  */  CHAN | NONEOS,
/* 0xC8  */  CHAN | NONEOS,
/* 0xC9  */  CHAN | NONEOS,
/* 0xCA  */  CHAN | NONEOS,
/* 0xCB  */  CHAN | NONEOS,
/* 0xCC  */  CHAN | NONEOS,
/* 0xCD  */  CHAN | NONEOS,
/* 0xCE  */  CHAN | NONEOS,
/* 0xCF  */  CHAN | NONEOS,
/* 0xD0  */  CHAN | NONEOS,
/* 0xD1  */  CHAN | NONEOS,
/* 0xD2  */  CHAN | NONEOS,
/* 0xD3  */  CHAN | NONEOS,
/* 0xD4  */  CHAN | NONEOS,
/* 0xD5  */  CHAN | NONEOS,
/* 0xD6  */  CHAN | NONEOS,
/* 0xD7  */  CHAN | NONEOS,
/* 0xD8  */  CHAN | NONEOS,
/* 0xD9  */  CHAN | NONEOS,
/* 0xDA  */  CHAN | NONEOS,
/* 0xDB  */  CHAN | NONEOS,
/* 0xDC  */  CHAN | NONEOS,
/* 0xDD  */  CHAN | NONEOS,
/* 0xDE  */  CHAN | NONEOS,
/* 0xDF  */  CHAN | NONEOS,
/* 0xE0  */  CHAN | NONEOS,
/* 0xE1  */  CHAN | NONEOS,
/* 0xE2  */  CHAN | NONEOS,
/* 0xE3  */  CHAN | NONEOS,
/* 0xE4  */  CHAN | NONEOS,
/* 0xE5  */  CHAN | NONEOS,
/* 0xE6  */  CHAN | NONEOS,
/* 0xE7  */  CHAN | NONEOS,
/* 0xE8  */  CHAN | NONEOS,
/* 0xE9  */  CHAN | NONEOS,
/* 0xEA  */  CHAN | NONEOS,
/* 0xEB  */  CHAN | NONEOS,
/* 0xEC  */  CHAN | NONEOS,
/* 0xED  */  CHAN | NONEOS,
/* 0xEE  */  CHAN | NONEOS,
/* 0xEF  */  CHAN | NONEOS,
/* 0xF0  */  CHAN | NONEOS,
/* 0xF1  */  CHAN | NONEOS,
/* 0xF2  */  CHAN | NONEOS,
/* 0xF3  */  CHAN | NONEOS,
/* 0xF4  */  CHAN | NONEOS,
/* 0xF5  */  CHAN | NONEOS,
/* 0xF6  */  CHAN | NONEOS,
/* 0xF7  */  CHAN | NONEOS,
/* 0xF8  */  CHAN | NONEOS,
/* 0xF9  */  CHAN | NONEOS,
/* 0xFA  */  CHAN | NONEOS,
/* 0xFB  */  CHAN | NONEOS,
/* 0xFC  */  CHAN | NONEOS,
/* 0xFD  */  CHAN | NONEOS,
/* 0xFE  */  CHAN | NONEOS,
/* 0xFF  */  CHAN | NONEOS
}};
