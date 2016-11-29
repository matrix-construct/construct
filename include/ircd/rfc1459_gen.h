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
#define HAVE_IRCD_RFC1459_GEN_H

#ifdef __cplusplus
#include <boost/spirit/include/karma.hpp>
#include "rfc1459_parse.h"

namespace ircd    {
namespace rfc1459 {
namespace gen     {

namespace karma = boost::spirit::karma;
using karma::lit;
using karma::int_;
using karma::char_;
using karma::buffer;
using karma::repeat;

template<class it,
         class top>
struct grammar
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
rfc1459::gen::grammar<it, top>::grammar(karma::rule<it, top> &top_rule)
:grammar<it, top>::base_type
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

}      // namespace gen
}      // namespace rfc1459
}      // namespace ircd
#endif // __cplusplus
