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

#include <boost/spirit/include/qi.hpp>

namespace qi = boost::spirit::qi;
using namespace qi;

using str = std::string;
using strvec = std::vector<str>;
using strvecvec = std::vector<strvec>;
using strvecvecvec = std::vector<strvecvec>;

template<class iter>
struct ignores
:grammar<iter>
{
	using rule = qi::rule<iter>;

	rule c_comment;
	rule cc_comment;
	rule sh_comment;
	rule skip;

	ignores();
};

template<class iter,
         class ignores>
struct newconf_parser
:grammar<iter, strvecvecvec(), ignores>
{
	template<class ret> using rule = qi::rule<iter, ret, ignores>;

	rule<str()> unquoted;
	rule<str()> quoted;
	rule<str()> key;
	rule<strvec()> item;
	rule<strvec()> topitem;
	rule<strvecvec()> block;
	rule<strvecvecvec()> conf;

	newconf_parser();
};

template<class iter,
         class ignores>
newconf_parser<iter, ignores>::newconf_parser()
:newconf_parser::base_type // pass reference the topmost level to begin parsing on
{
	conf
}
,unquoted // config values without double-quotes, cannot have ';' because that's the ending
{
	lexeme[+(char_ - ';')]
}
,quoted // config values surrounded by double quotes, which cannot have double quotes in them
{
	lexeme['"' >> +(char_ - '"') >> '"']
}
,key // configuration key, must be simple alnum's with underscores
{
	+char_("[a-zA-Z0-9_]")
}
,item // a key-value pair
{
	+(key) >> '=' >> +(quoted | unquoted) >> ';'
}
,topitem // a key-value pair, but at the topconf no '=' is used
{
	+(key) >> +(quoted) >> ';'
}
,block // a bracketed conf block, the type-label is like a key, and it can have a unique quoted label
{
	*(key >> *(quoted)) >> '{' >> *(item) >> '}' >> ';'
}
,conf // newconf is comprised of either conf blocks or key-value's at the top level
{
	*(block | topitem)
}
{
}

template<class iter>
ignores<iter>::ignores()
:ignores::base_type // pass reference to the topmost skip-parser
{
	skip
}
,c_comment // a multiline comment
{
	lit('/') >> lit('*') >> *(char_ - (lit('*') >> '/')) >> lit("*/")
}
,cc_comment // a single line comment, like this one :)
{
	lit('/') >> lit('/') >> *(char_ - eol) >> eol | blank
}
,sh_comment // a single line shell comment
{
	'#' >> *(char_ - eol) >> eol | blank
}
,skip // the skip parser is one and any one of the comment types, or unmatched whitespace
{
	+(ascii::space | c_comment | cc_comment | sh_comment)
}
{
}

ircd::conf::newconf::topconf
ircd::conf::newconf::parse_file(const std::string &path)
{
	std::ifstream file(path);
	return parse(file);
}

ircd::conf::newconf::topconf
ircd::conf::newconf::parse(std::ifstream &file)
{
	const std::istreambuf_iterator<char> bit(file), eit;
	return parse(std::string(bit, eit));
}

ircd::conf::newconf::topconf
ircd::conf::newconf::parse(const std::string &str)
{
	using iter = std::string::const_iterator;

	strvecvecvec vec;
	const ignores<iter> ign;
	const newconf_parser<iter, ignores<iter>> p;
	if(!phrase_parse(begin(str), end(str), p, ign, vec))
		throw ircd::error("Failed to parse");

	// The parser doesn't feed exactly into the ideal topconf format returned to the user.
	// It's a simple position-semantic vector of strings at the same level.
	// Translation for the user is here:
	topconf top;
	for(const auto &v : vec)
	{
		key k; block b;
		for(const auto &vv : v)
		{
			if(vv.empty())
			{
				// For unnamed blocks (without double-quoted names) the "*" serves
				// as a placeholder indicating no-name rather than ""
				b.first = "*";
				continue;
			}

			item i;
			for(const auto &vvv : vv)
			{
				if(vvv.empty())
					continue;
				else if(k.empty())
					k = vvv;
				else if(b.first.empty())
					b.first = vvv;
				else if(i.first.empty())
					i.first = vvv;
				else
					i.second.emplace_back(vvv);
			}

			if(!i.first.empty())
				b.second.emplace_back(i);
		}

		top.emplace(k, b);
	}

	return top;
}
