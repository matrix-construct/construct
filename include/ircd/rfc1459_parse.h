/**
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

#pragma once
#define HAVE_IRCD_RFC1459_PARSE_H

#include <boost/spirit/include/qi.hpp>

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

namespace ircd    {
namespace rfc1459 {
namespace parse   {

namespace qi = boost::spirit::qi;

using qi::lit;
using qi::char_;
using qi::repeat;
using qi::attr;
using qi::eps;
using qi::raw;
using qi::omit;

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
grammar<it, top>::grammar(qi::rule<it, top> &top_rule)
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

// Instantiate the input grammar to parse a uint8_t* buffer into an rfc1459::line object.
// The top rule is inherited and then specified as grammar::line, which is compatible
// with an rfc1459::line object.
//
struct head
:parse::grammar<const char *, rfc1459::line>
{
    head(): grammar{grammar::line} {}
}
extern const head;

// Instantiate the input grammar to parse a uint8_t* buffer into an rfc1459::tape object.
// The top rule is now grammar::tape and the target object is an rfc1459::tape deque.
//
struct capstan
:parse::grammar<const char *, std::deque<rfc1459::line>>
{
    capstan(): grammar{grammar::tape} {}
}
extern const capstan;

} // namespace parse
} // namespace rfc1459
} // namespace ircd
