/**
 * charybdis: libirc: Formal IRC library
 * rfc1459.cc: rfc1459 compliance implementation
 *
 * Copyright (C) 2016 Charybdis Development Team
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

#include <boost/spirit/include/qi.hpp>
#include <ircd/bufs.h>

namespace qi = boost::spirit::qi;

BOOST_FUSION_ADAPT_STRUCT
(
	ircd::rfc1459::pfx,
	( ircd::rfc1459::nick,  nick )
	( ircd::rfc1459::user,  user )
	( ircd::rfc1459::host,  host )
)

BOOST_FUSION_ADAPT_STRUCT
(
	ircd::rfc1459::line,
	( ircd::rfc1459::pfx,   pfx  )
	( ircd::rfc1459::cmd,   cmd  )
	( ircd::rfc1459::parv,  parv )
)

namespace ircd    {
namespace rfc1459 {

using qi::lit;
using qi::char_;
using qi::repeat;
using qi::attr;
using qi::eps;

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
struct grammar
:qi::grammar<it, top>
{
	qi::rule<it> space;
	qi::rule<it> colon;
	qi::rule<it> nulcrlf;
	qi::rule<it> spnulcrlf;
	qi::rule<it> terminator;

	qi::rule<it, std::string()> hostname;
	qi::rule<it, std::string()> user;
	qi::rule<it, std::string()> server;
	qi::rule<it, std::string()> nick;
	qi::rule<it, rfc1459::pfx> prefix;

	qi::rule<it, std::string()> trailing;
	qi::rule<it, std::string()> middle;
	qi::rule<it, rfc1459::parv> params;

	qi::rule<it, std::string()> numeric;
	qi::rule<it, rfc1459::cmd> command;

	qi::rule<it, rfc1459::line> line;
	qi::rule<it, rfc1459::tape> tape;

	grammar(qi::rule<it, top> &top_rule);
};

/* Instantiate the grammar to parse a uint8_t* buffer into an rfc1459::line object.
 * The top rule is inherited and then specified as grammar::line, which is compatible
 * with an rfc1459::line object.
 */
struct head
:grammar<const uint8_t *, rfc1459::line>
{
	head(): grammar{grammar::line} {}
}
static const head;

/* Instantiate the grammar to parse a uint8_t* buffer into an rfc1459::tape object.
 * The top rule is now grammar::tape and the target object is an rfc1459::tape deque.
 */
struct capstan
:grammar<const uint8_t *, rfc1459::tape>
{
	capstan(): grammar{grammar::tape} {}
}
static const capstan;

} // namespace rfc1459
} // namespace ircd

using namespace ircd;

template<class it,
         class top>
rfc1459::grammar<it, top>::grammar(qi::rule<it, top> &top_rule)
:grammar<it, top>::base_type
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

	//char_(gather(character::SPACE))
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
	+char_(gather(character::HOST)) // TODO: https://tools.ietf.org/html/rfc952
	,"hostname"
}
,user // A valid username
{
	+char_(gather(character::USER))
	,"user"
}
,server // A valid servername
{
	hostname
	,"server"
}
,nick // A valid nickname, leading letter followed by any NICK chars
{
	char_(gather(character::ALPHA)) >> *char_(gather(character::NICK))
	,"nick"
}
,prefix // A valid prefix, required name, optional user and host (or empty placeholders)
{
	colon >> (nick | server) >> -(lit('!') >> user) >> -(lit('@') >> hostname)
	,"prefix"
}
,trailing // Trailing string pinch
{
	colon >> +(char_ - nulcrlf)
	,"trailing"
}
,middle // Spaced parameters
{
	!colon >> +(char_ - spnulcrlf)
	,"middle"
}
,params // Parameter vector
{
	*(+space >> middle) >> -(+space >> trailing)
	,"params"
}
,numeric // \d\d\d numeric
{
	repeat(3)[char_(gather(character::DIGIT))]
	,"numeric"
}
,command // A command or numeric
{
	+char_(gather(character::ALPHA)) | numeric
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

// Informal stream (for now)
// NOTE: unterminated
std::ostream &
rfc1459::operator<<(std::ostream &s, const line &line)
{
	const auto &pfx(line.pfx);
	if(!pfx.nick.empty() || !pfx.user.empty() || !pfx.host.empty())
		s << pfx << ' ';

	s << line.cmd;

	const auto &parv(line.parv);
	if(!parv.empty())
		s << ' ' << parv;

	return s;
}

// Informal stream (for now)
std::ostream &
rfc1459::operator<<(std::ostream &s, const parv &parv)
{
	ssize_t i(0);
	for(; i < ssize_t(parv.size()) - 1; ++i)
		s << parv[i] << ' ';

	if(!parv.empty())
		s << ':' << parv[parv.size() - 1];

	return s;
}

// Informal stream (for now)
std::ostream &
rfc1459::operator<<(std::ostream &s, const pfx &pfx)
{
	s << ':';
	if(!pfx.nick.empty())
		s << pfx.nick;
	else
		s << '*';

	s << '!';
	if(!pfx.user.empty())
		s << pfx.user;
	else
		s << '*';

	s << '@';
	if(!pfx.host.empty())
		s << pfx.host;
	else
		s << '*';

	return s;
}

rfc1459::tape::tape(const std::string &str)
:tape
{
	reinterpret_cast<const uint8_t *>(str.data()),
	str.size()
}
{
}

rfc1459::tape::tape(const char *const &str)
:tape
{
	reinterpret_cast<const uint8_t *>(str),
	strlen(str)
}
{
}

rfc1459::tape::tape(const uint8_t *const &buf,
                    const size_t &size)
try
{
	const uint8_t *start(buf);
	const uint8_t *stop(buf + size);
	qi::parse(start, stop, capstan, *this);
}
catch(const boost::spirit::qi::expectation_failure<const uint8_t *> &e)
{
	throw syntax_error("@%d expecting :%s",
	                   int(e.last - e.first),
	                   string(e.what_).c_str());
}

bool
rfc1459::tape::append(const char *const &buf,
                      const size_t &len)
{
	const auto &start(reinterpret_cast<const uint8_t *>(buf));
	const auto &stop(start + len);
	return qi::parse(start, stop, capstan, *this);
}

rfc1459::line::line(const std::string &str)
:line
{
	reinterpret_cast<const uint8_t *>(str.data()),
	str.size()
}
{
}

rfc1459::line::line(const char *const &str)
:line
{
	reinterpret_cast<const uint8_t *>(str),
	strlen(str)
}
{
}

rfc1459::line::line(const uint8_t *const &buf,
                    const size_t &size)
try
{
	const uint8_t *start(buf);
	const uint8_t *stop(buf + size);
	qi::parse(start, stop, head, *this);
}
catch(const boost::spirit::qi::expectation_failure<const uint8_t *> &e)
{
	throw syntax_error("@%d expecting :%s",
	                   int(e.last - e.first),
	                   string(e.what_).c_str());
}

bool
rfc1459::line::empty()
const
{
	return pfx.empty() &&
	       cmd.empty() &&
	       parv.empty();
}

bool
rfc1459::pfx::empty()
const
{
	return nick.empty() &&
	       user.empty() &&
	       host.empty();
}

std::string
rfc1459::character::gather(const attr &attr)
{
	uint8_t buf[256];
	const size_t len(gather(attr, buf, sizeof(buf)));
	return { reinterpret_cast<const char *>(buf), len };
}

size_t
rfc1459::character::gather(const attr &attr,
                           uint8_t *const &buf,
                           const size_t &max)
{
	size_t ret(0);
	for(ssize_t i(attrs.size() - 1); i >= 0 && ret < max; --i)
		if(is(i, attr))
			buf[ret++] = i;

	return ret;
}

decltype(rfc1459::character::tolower_tab)
rfc1459::character::tolower_tab
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

decltype(rfc1459::character::toupper_tab)
rfc1459::character::toupper_tab
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
const decltype(rfc1459::character::attrs)
rfc1459::character::attrs
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
