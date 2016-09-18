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
#include <ircd/lex_cast.h>
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

const char SPECIFIER_TERMINATOR
{
	'$'
};

std::map<std::string, specifier *> _specifiers;

template<class generator> bool generate_string(char *&out, const generator &, const arg &);
template<class integer> bool generate_integer(char *&out, const size_t &max, const spec &, const integer &i);

struct param_specifier
:specifier
{
	bool operator()(char *&out, const size_t &max, const spec &, const arg &) const override;
	using specifier::specifier;
}
const param_specifier
{
	"param"s
};

struct parv_specifier
:specifier
{
	bool operator()(char *&out, const size_t &max, const spec &, const arg &) const override;
	using specifier::specifier;
}
const parv_specifier
{
	"parv"s
};

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

struct prefix_specifier
:specifier
{
	bool operator()(char *&out, const size_t &max, const spec &, const arg &) const override;
	using specifier::specifier;
}
const prefix_specifier
{
	"prefix"s
};

struct string_specifier
:specifier
{
	bool operator()(char *&out, const size_t &max, const spec &, const arg &val) const override;
	using specifier::specifier;
}
const string_specifier
{
	"s"s
};

struct int_specifier
:specifier
{
	bool operator()(char *&out, const size_t &max, const spec &, const arg &val) const override;
	using specifier::specifier;
}
const int_specifier
{
	{ "d"s, "ld"s, "u"s, "lu"s, "zd"s, "zu"s }
};

struct char_specifier
:specifier
{
	bool operator()(char *&out, const size_t &max, const spec &, const arg &val) const override;
	using specifier::specifier;
}
const char_specifier
{
	"c"s
};

bool is_specifier(const std::string &name);
void handle_specifier(char *&out, const size_t &max, const uint &idx, const spec &, const arg &);

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
		qi::rule<it> specterm;
		qi::rule<it, std::string()> name;
		qi::rule<it, fmt::spec> spec;

		grammar(qi::rule<it, top> &top_rule);
	};
}

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
,specterm
{
	char_(SPECIFIER_TERMINATOR)
}
,name
{
	repeat(1,14)[char_("A-Za-z")] >> omit[-specterm]
}
{
	spec %= specsym >> -char_("+-") >> -int_ >> name[([]
	(auto &str, auto &ctx, auto &valid)
	{
		valid = is_specifier(str);
	})];
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
	for(; start; stop = start++, start = start < end? strchr(start, SPECIFIER) : nullptr)
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
		handle_specifier(out, remaining(), index, spec, val);
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
	throw invalid_format("Format string requires more than %zu arguments.", p.size());
}

const decltype(fmt::_specifiers) &
fmt::specifiers()
{
	return _specifiers;
}

fmt::specifier::specifier(const std::string &name)
:specifier{{name}}
{
}

fmt::specifier::specifier(const std::initializer_list<std::string> &names)
:names{names}
{
	for(const auto &name : this->names)
		if(is_specifier(name))
			throw error("Specifier '%c%s' already registered\n",
			            SPECIFIER,
			            name.c_str());

	for(const auto &name : this->names)
		_specifiers.emplace(name, this);
}

fmt::specifier::~specifier()
noexcept
{
	for(const auto &name : names)
		_specifiers.erase(name);
}

fmt::spec::spec()
:sign('+')
,width(0)
{
	name.reserve(14);
}

bool
fmt::is_specifier(const std::string &name)
{
	return specifiers().count(name);
}

void
fmt::handle_specifier(char *&out,
                      const size_t &max,
                      const uint &idx,
                      const spec &spec,
                      const arg &val)
try
{
	const auto &type(get<1>(val));
	const auto &handler(*specifiers().at(spec.name));
	if(!handler(out, max, spec, val))
		throw invalid_type("`%s' for format specifier '%c%s' for argument #%u",
		                   type.name(),
		                   SPECIFIER,
		                   spec.name.c_str(),
		                   idx);
}
catch(const std::out_of_range &e)
{
	throw invalid_format("Unhandled specifier `%c%s' for argument #%u in format string",
	                     SPECIFIER,
	                     spec.name.c_str(),
	                     idx);
}
catch(const illegal &e)
{
	throw illegal("Specifier `%c%s' for argument #%u: %s",
	              SPECIFIER,
	              spec.name.c_str(),
	              idx,
	              e.what());
}

bool
fmt::char_specifier::operator()(char *&out,
                                const size_t &max,
                                const spec &,
                                const arg &val)
const
{
	using karma::char_;
	using karma::eps;
	using karma::maxwidth;

	static const auto throw_illegal([]
	{
		throw illegal("Not a printable character");
	});

	struct generator
	:rfc1459::gen::grammar<char *, char()>
	{
		karma::rule<char *, char()> printable
		{
			char_(rfc1459::character::gather(rfc1459::character::PRINT))
		};

		generator(): grammar{printable} {}
	}
	static const generator;

	const auto &ptr(get<0>(val));
	const auto &type(get<1>(val));
	if(type == typeid(const char))
	{
		const auto &c(*reinterpret_cast<const char *>(ptr));
		karma::generate(out, maxwidth(max)[generator] | eps[throw_illegal], c);
		return true;
	}
	else return false;
}

bool
fmt::int_specifier::operator()(char *&out,
                               const size_t &max,
                               const spec &s,
                               const arg &val)
const
{
	const auto &ptr(get<0>(val));
	const auto &type(get<1>(val));

	if(type == typeid(const char))
		return generate_integer(out, max, s, *reinterpret_cast<const char *>(ptr));

	if(type == typeid(const unsigned char))
			return generate_integer(out, max, s, *reinterpret_cast<const unsigned char *>(ptr));

	if(type == typeid(const short))
			return generate_integer(out, max, s, *reinterpret_cast<const short *>(ptr));

	if(type == typeid(const unsigned short))
			return generate_integer(out, max, s, *reinterpret_cast<const unsigned short *>(ptr));

	if(type == typeid(const int))
			return generate_integer(out, max, s, *reinterpret_cast<const int *>(ptr));

	if(type == typeid(const unsigned int))
			return generate_integer(out, max, s, *reinterpret_cast<const unsigned int *>(ptr));

	if(type == typeid(const long))
			return generate_integer(out, max, s, *reinterpret_cast<const long *>(ptr));

	if(type == typeid(const unsigned long))
			return generate_integer(out, max, s, *reinterpret_cast<const unsigned long *>(ptr));

	if(type == typeid(const long long))
			return generate_integer(out, max, s, *reinterpret_cast<const long long *>(ptr));

	if(type == typeid(const unsigned long long))
			return generate_integer(out, max, s, *reinterpret_cast<const unsigned long long *>(ptr));

	if(type == typeid(const char[]))
	{
		size_t test;
		const auto &i(reinterpret_cast<const char *>(ptr));
		const auto len(std::min(max, strlen(i)));
		if(!boost::conversion::try_lexical_convert(i, test))
			throw illegal("The string literal value for integer specifier is not a valid integer");

		memcpy(out, i, len);
		out += len;
		return true;
	}

	if(type == typeid(const char *))
	{
		size_t test;
		const auto &i(*reinterpret_cast<const char *const *>(ptr));
		const auto len(std::min(max, strlen(i)));
		if(!boost::conversion::try_lexical_convert(i, test))
			throw illegal("The character buffer for integer specifier is not a valid integer");

		memcpy(out, i, len);
		out += len;
		return true;
	}

	if(type == typeid(const std::string))
	{
		size_t test;
		const auto &i(*reinterpret_cast<const std::string *>(ptr));
		const auto len(std::min(max, i.size()));
		if(!boost::conversion::try_lexical_convert(i, test))
			throw illegal("The string argument for integer specifier is not a valid integer");

		memcpy(out, i.data(), len);
		out += len;
		return true;
	}

	return false;
}

bool
fmt::string_specifier::operator()(char *&out,
                                  const size_t &max,
                                  const spec &,
                                  const arg &val)
const
{
	using karma::char_;
	using karma::eps;
	using karma::maxwidth;

	static const auto throw_illegal([]
	{
		throw illegal("Not a printable string");
	});

	struct generator
	:rfc1459::gen::grammar<char *, std::string()>
	{
		karma::rule<char *, std::string()> printable
		{
			*char_(rfc1459::character::gather(rfc1459::character::PRINT))
		};

		generator(): grammar{printable} {}
	}
	static const generator;
	return generate_string(out, maxwidth(max)[generator] | eps[throw_illegal], val);
}

bool
fmt::prefix_specifier::operator()(char *&out,
                                  const size_t &max,
                                  const spec &spec,
                                  const arg &val)
const
{
	using karma::eps;
	using karma::maxwidth;

	static const auto throw_illegal([]
	{
		throw illegal("Argument is not a valid IRC prefix");
	});

	const auto &ptr(get<0>(val));
	const auto &type(get<1>(val));
	if(type == typeid(rfc1459::pfx))
	{
		struct generator
		:rfc1459::gen::grammar<char *, rfc1459::pfx>
		{
			generator(): grammar{grammar::prefix} {}
		}
		static const generator;
		const auto &pfx(*reinterpret_cast<const rfc1459::pfx *>(ptr));
		return karma::generate(out, maxwidth(max)[generator] | eps[throw_illegal], pfx);
	}
	else return false;
}

bool
fmt::host_specifier::operator()(char *&out,
                                const size_t &max,
                                const spec &spec,
                                const arg &val)
const
{
	using karma::eps;
	using karma::maxwidth;

	static const auto throw_illegal([]
	{
		throw illegal("Argument is not a valid host string");
	});

	struct generator
	:rfc1459::gen::grammar<char *, std::string()>
	{
		generator(): grammar{grammar::hostname} {}
	}
	static const generator;
	return generate_string(out, maxwidth(max)[generator] | eps[throw_illegal], val);
}

bool
fmt::user_specifier::operator()(char *&out,
                                const size_t &max,
                                const spec &spec,
                                const arg &val)
const
{
	using karma::eps;
	using karma::maxwidth;

	static const auto throw_illegal([]
	{
		throw illegal("Argument is not a valid user string");
	});

	struct generator
	:rfc1459::gen::grammar<char *, std::string()>
	{
		generator(): grammar{grammar::user} {}
	}
	static const generator;
	return generate_string(out, maxwidth(max)[generator] | eps[throw_illegal], val);
}

bool
fmt::nick_specifier::operator()(char *&out,
                                const size_t &max,
                                const spec &spec,
                                const arg &val)
const
{
	using karma::eps;
	using karma::maxwidth;

	static const auto throw_illegal([]
	{
		throw illegal("Argument is not a valid nick string");
	});

	struct generator
	:rfc1459::gen::grammar<char *, std::string()>
	{
		generator(): grammar{grammar::nick} {}
	}
	static const generator;
	return generate_string(out, maxwidth(max)[generator] | eps[throw_illegal], val);
}

bool
fmt::parv_specifier::operator()(char *&out,
                                const size_t &max,
                                const spec &spec,
                                const arg &val)
const
{
	using karma::eps;
	using karma::delimit;
	using karma::maxwidth;

	static const auto throw_illegal([]
	{
		throw illegal("Argument is not a valid IRC parameter vector");
	});

	const auto &ptr(get<0>(val));
	const auto &type(get<1>(val));
	if(type == typeid(const rfc1459::parv))
	{
		/*
		struct generator
		:rfc1459::gen::grammar<char *, rfc1459::parv>
		{
			generator(): grammar{grammar::params} {}
		}
		static const generator;
		const auto &parv(*reinterpret_cast<const rfc1459::parv *>(ptr));
		return karma::generate(out, maxwidth(max)[generator] | eps[throw_illegal], parv);
		*/

		struct generate_middle
		:rfc1459::gen::grammar<char *, std::string()>
		{
			generate_middle(): grammar{grammar::middle} {}
		}
		static const generate_middle;

		struct generate_trailing
		:rfc1459::gen::grammar<char *, std::string()>
		{
			generate_trailing(): grammar{grammar::trailing} {}
		}
		static const generate_trailing;
		const auto &parv(*reinterpret_cast<const rfc1459::parv *>(ptr));

		const char *const start(out);
		const auto remain([&start, &out, &max]
		{
			return max - (out - start);
		});

		ssize_t i(0);
		for(; i < ssize_t(parv.size()) - 1; ++i)
			if(!karma::generate(out, maxwidth(remain())[delimit[generate_middle]], parv.at(i)))
				throw illegal("Invalid middle parameter");

		if(!parv.empty())
			if(!karma::generate(out, maxwidth(remain())[generate_trailing], parv.at(parv.size() - 1)))
				throw illegal("Invalid trailing parameter");

		return true;
	}
	else return false;
}

bool
fmt::param_specifier::operator()(char *&out,
                                 const size_t &max,
                                 const spec &spec,
                                 const arg &val)
const
{
	using karma::eps;
	using karma::maxwidth;

	static const auto throw_illegal([]
	{
		throw illegal("Argument is not a valid IRC 'middle' parameter");
	});

	struct generator
	:rfc1459::gen::grammar<char *, std::string()>
	{
		generator(): grammar{grammar::middle} {}
	}
	static const generator;
	return generate_string(out, maxwidth(max)[generator] | eps[throw_illegal], val);
}

template<class integer>
bool
fmt::generate_integer(char *&out,
                      const size_t &max,
                      const spec &s,
                      const integer &i)
{
	using karma::int_;
	using karma::maxwidth;

	struct generator
	:rfc1459::gen::grammar<char *, integer()>
	{
		karma::rule<char *, integer()> rule
		{
			int_
		};

		generator(): rfc1459::gen::grammar<char *, integer()>{rule} {}
	}
	static const generator;
	return karma::generate(out, maxwidth(max)[generator], i);
}

template<class generator>
bool
fmt::generate_string(char *&out,
                     const generator &gen,
                     const arg &val)
{
	using karma::eps;

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
		const auto &str(*reinterpret_cast<const char *const *>(ptr));
		karma::generate(out, gen, str);
		return true;
	}

	// This for string literals which have unique array types depending on their size.
	// There is no reasonable way to match them. The best that can be hoped for is the
	// grammar will fail gracefully (most of the time) or not print something bogus when
	// it happens to be legal.
	const auto &str(reinterpret_cast<const char *>(ptr));
	return karma::generate(out, gen, str);
}
