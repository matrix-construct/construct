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

namespace ircd    {
namespace conf    {
namespace newconf {

	letters registry;

} // namespace newconf
} // namespace conf
} // namespace ircd

using namespace ircd;

/*
	registry['A'] = "admin";
	registry['B'] = "blacklist";
	registry['C'] = "connect";
	registry['I'] = "auth";
	registry['O'] = "operator";
	registry['P'] = "listen";
	registry['U'] = "service";
	registry['Y'] = "class";
	registry['a'] = "alias";
	registry['d'] = "exempt";
	registry['g'] = "general";
	registry['l'] = "log";
*/

namespace qi = boost::spirit::qi;
namespace ascii = qi::ascii;
using qi::lexeme;
using qi::char_;
using qi::lit;
using qi::eol;
using qi::blank;

using str = std::string;
using strvec = std::vector<str>;
using strvecvec = std::vector<strvec>;
using strvecvecvec = std::vector<strvecvec>;

template<class iter>
struct ignores
:qi::grammar<iter>
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
:qi::grammar<iter, strvecvecvec(), ignores>
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

		top.emplace_back(k, b);
	}

	return top;
}

std::list<std::string>
conf::newconf::translate(const topconf &top)
{
	std::list<std::string> ret;
	translate(top, [&ret](std::string line)
	{
		ret.emplace_back(std::move(line));
	});

	return ret;
}

void
conf::newconf::translate(const topconf &top,
                         const std::function<void (std::string)> &closure)
{
	for(const auto &pair : top) try
	{
		const auto &type(pair.first);
		const auto &block(pair.second);
		const auto &label(block.first);
		const auto &items(block.second);
		const auto &letter(find_letter(type));
		for(const auto &item : items)
		{
			const auto &key(item.first);
			const auto &vals(item.second);
			std::stringstream buf;
			buf << letter << " "
			    << label << " "
			    << key << " :";

			for(auto i(0); i < int(vals.size()) - 1; ++i)
				buf << vals.at(i) << " ";

			if(!vals.empty())
				buf << vals.back();

			closure(buf.str());
		}
	}
	catch(const error &e)
	{
		log.warning("%s", e.what());
	}
}

uint8_t
conf::newconf::find_letter(const std::string &name)
{
	const auto &registry(newconf::registry);
	const auto it(std::find(begin(registry), end(registry), name));
	if(it == end(registry))
		throw unknown_block("%s is not registered to a letter", name.c_str());

	return std::distance(begin(registry), it);
}
