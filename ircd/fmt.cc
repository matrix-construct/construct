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

#include <ircd/rfc1459_parse.h>
#include <ircd/rfc1459_gen.h>
#include <ircd/fmt.h>

BOOST_FUSION_ADAPT_STRUCT
(
	ircd::fmt::spec,
	( char,         sign  )
	( int,          width )
	( std::string,  name  )
)

namespace ircd {
namespace fmt  {

namespace qi = boost::spirit::qi;
namespace karma = boost::spirit::karma;

const char SPECIFIER
{
	'%'
};

namespace parse
{
	using qi::lit;
	using qi::char_;
	using qi::int_;
	using qi::eps;
	using qi::repeat;
	using qi::omit;

	template<class it,
	         class top>
	struct grammar
	:qi::grammar<it, top>
	{
		qi::rule<it> specsym;
		qi::rule<it, std::string()> name;
		qi::rule<it, fmt::spec> spec;

		grammar(qi::rule<it, top> &top_rule);
	};
}

template<class generator> bool generate_string(char *&out, const generator &, const arg &);
void handle_specifier(char *&out, const size_t &max, const spec &, const arg &);
bool is_specifier(const std::string &name);

std::map<std::string, specifier *> _specifiers;

struct nick_specifier
:specifier
{
	bool operator()(char *&out, const size_t &max, const spec &, const arg &) const override;
	using specifier::specifier;
}
const nick_specifier
{
	"nick"s
};

struct user_specifier
:specifier
{
	bool operator()(char *&out, const size_t &max, const spec &, const arg &) const override;
	using specifier::specifier;
}
const user_specifier
{
	"user"s
};

struct host_specifier
:specifier
{
	bool operator()(char *&out, const size_t &max, const spec &, const arg &) const override;
	using specifier::specifier;
}
const host_specifier
{
	"host"s
};

} // namespace fmt
} // namespace ircd

using namespace ircd;

template<class it,
         class top>
fmt::parse::grammar<it, top>::grammar(qi::rule<it, top> &top_rule)
:grammar<it, top>::base_type
{
	top_rule
}
,specsym
{
	lit(SPECIFIER)
}
,name
{
	repeat(1,14)[char_("A-Za-z")]
}
{
	spec %= specsym >> -char_("+-") >> -int_ >> name[([]
	(auto &str, auto &ctx, auto &valid)
	{
		valid = is_specifier(str);
	})];
}

fmt::specifier::specifier(const std::string &name)
:name{name}
{
	const auto iit(_specifiers.emplace(name, this));
	if(!iit.second)
		throw error("Specifier '%c%s' already registered\n",
		            SPECIFIER,
		            name.c_str());
}

fmt::specifier::~specifier()
noexcept
{
	_specifiers.erase(name);
}

fmt::spec::spec()
:sign('+')
,width(0)
{
	name.reserve(14);
}

const decltype(fmt::_specifiers) &
fmt::specifiers()
{
	return _specifiers;
}

bool
fmt::is_specifier(const std::string &name)
{
	return specifiers().count(name);
}

void
fmt::handle_specifier(char *&out,
                      const size_t &max,
                      const spec &spec,
                      const arg &val)
try
{
	const auto &type(get<1>(val));
	const auto &handler(*specifiers().at(spec.name));
	if(!handler(out, max, spec, val))
		throw fmtstr_mismatch("Invalid type `%s' for format specifier '%c%s'",
		                      type.name(),
		                      SPECIFIER,
		                      spec.name.c_str());
}
catch(const std::out_of_range &e)
{
	throw fmtstr_invalid("Unhandled specifier `%c%s' in format string",
	                     SPECIFIER,
	                     spec.name.c_str());
}

bool
fmt::host_specifier::operator()(char *&out,
                                const size_t &max,
                                const spec &spec,
                                const arg &val)
const
{
	using karma::maxwidth;

	struct generator
	:rfc1459::gen::grammar<char *, std::string()>
	{
		generator(): grammar{grammar::hostname} {}
	}
	static const generator;
	return generate_string(out, maxwidth(max)[generator], val);
}

bool
fmt::user_specifier::operator()(char *&out,
                                const size_t &max,
                                const spec &spec,
                                const arg &val)
const
{
	using karma::maxwidth;

	struct generator
	:rfc1459::gen::grammar<char *, std::string()>
	{
		generator(): grammar{grammar::user} {}
	}
	static const generator;
	return generate_string(out, maxwidth(max)[generator], val);
}

bool
fmt::nick_specifier::operator()(char *&out,
                                const size_t &max,
                                const spec &spec,
                                const arg &val)
const
{
	using karma::maxwidth;

	struct generator
	:rfc1459::gen::grammar<char *, std::string()>
	{
		generator(): grammar{grammar::nick} {}
	}
	static const generator;
	return generate_string(out, maxwidth(max)[generator], val);
}

template<class generator>
bool
fmt::generate_string(char *&out,
                     const generator &gen,
                     const arg &val)
{
	const auto &ptr(get<0>(val));
	const auto &type(get<1>(val));
	if(type == typeid(std::string))
	{
		const auto &str(*reinterpret_cast<const std::string *>(ptr));
		karma::generate(out, gen, str);
		return true;
	}
	else if(type == typeid(const char *))
	{
		const auto &str(reinterpret_cast<const char *>(ptr));
		karma::generate(out, gen, str);
		return true;
	}
	else return false;
}

ssize_t
fmt::_snprintf(char *const &buf,
               const size_t &max,
               const char *const &fmt,
               const ptrs &p,
               const types &t)
try
{
	if(unlikely(!max))
		return 0;

	char *out(buf);                             // Always points at next place to write
	const char *stop(fmt);                      // Saves the 'last' place to copy a literal from
	const char *start(strchr(fmt, SPECIFIER));  // The running position of the format string parse
	const char *const end(fmt + strlen(fmt));   // The end of the format string

	// Calculates remaining room in output buffer
	const auto remaining([&max, &buf, &out]() -> size_t
	{
		return max - std::distance(buf, out) - 1;
	});

	// Copies string data between format specifiers.
	const auto copy_literal([&stop, &out, &remaining]
	(const char *const &end)
	{
		const size_t len(std::distance(stop, end));
		const size_t &cpsz(std::min(len, remaining()));
		memcpy(out, stop, cpsz);
		out += cpsz;
	});

	size_t index(0); // The current position for vectors p and t (specifier count)
	for(; start && start != end; stop = start++, start = strchr(start, SPECIFIER))
	{
		// Copy literal data from where the last parse stopped up to the found specifier
		copy_literal(start);

		// Instantiate the format specifier grammar
		struct parser
		:parse::grammar<const char *, fmt::spec>
		{
			parser(): grammar{grammar::spec} {}
		}
		static const parser;

		// Parse the specifier with the grammar
		fmt::spec spec;
		if(!qi::parse(start, end, parser, spec))
			continue;

		// Throws if the format string has more specifiers than arguments.
		const arg val{p.at(index), t.at(index)};
		handle_specifier(out, remaining(), spec, val);
		index++;
	}

	// If the end of the string is not a format specifier itself, it needs to be copied
	if(!start)
		copy_literal(end);

	*out = '\0';
	return std::distance(buf, out);
}
catch(const std::out_of_range &e)
{
	throw fmtstr_invalid("Format string requires more than %zu arguments.", p.size());
}
