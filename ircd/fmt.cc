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

namespace ircd::fmt
{
	using namespace ircd::spirit;

	struct spec;
	struct specifier;

	constexpr char SPECIFIER
	{
		'%'
    };

	constexpr char SPECIFIER_TERMINATOR
	{
		'$'
	};

	struct parser extern const parser;
	extern std::map<string_view, specifier *, std::less<>> specifiers;

	bool is_specifier(const string_view &name);
	void handle_specifier(char *&out, const size_t &max, const uint &idx, const spec &, const arg &);
	template<class generator> bool generate_string(char *&out, const generator &gen, const arg &val);
	template<class T, class lambda> bool visit_type(const arg &val, lambda&& closure);
}

// Structural representation of a format specifier
struct ircd::fmt::spec
{
	char sign {'+'};
	ushort width {0};
	string_view name;

	spec() = default;
};

BOOST_FUSION_ADAPT_STRUCT
(
	ircd::fmt::spec,
	( decltype(ircd::fmt::spec::sign),   sign   )
	( decltype(ircd::fmt::spec::width),  width  )
	( decltype(ircd::fmt::spec::name),   name   )
)

// A format specifier handler module.
// This allows a new "%foo" to be defined with custom handling.
class ircd::fmt::specifier
{
	std::set<std::string> names;

  public:
	virtual bool operator()(char *&out, const size_t &max, const spec &, const arg &) const = 0;

	specifier(const std::initializer_list<std::string> &names);
	specifier(const std::string &name);
	virtual ~specifier() noexcept;
};

decltype(ircd::fmt::specifiers)
ircd::fmt::specifiers;

struct ircd::fmt::parser
:qi::grammar<const char *, fmt::spec>
{
	template<class R = unused_type> using rule = qi::rule<const char *, R>;

	const rule<> specsym
	{
		lit(SPECIFIER)
		,"format specifier"
	};

	const rule<> specterm
	{
		lit(SPECIFIER_TERMINATOR)
		,"specifier termination"
	};

	const rule<string_view> name
	{
		raw[repeat(1,14)[char_("A-Za-z")]]
		,"specifier name"
	};

	rule<fmt::spec> spec;

	parser()
	:parser::base_type{spec}
	{
		static const auto is_valid([]
		(const auto &str, auto &, auto &valid)
		{
			valid = is_specifier(str);
		});

		spec %= specsym >> -(char_('+') | char_('-')) >> -ushort_ >> name[is_valid] >> -specterm;
	}
}
const ircd::fmt::parser;

namespace ircd {
namespace fmt  {

struct string_specifier
:specifier
{
	static const std::tuple
	<
		const char *,
		std::string,
		std::string_view,
		ircd::string_view,
		ircd::json::string,
		ircd::json::object,
		ircd::json::array
	>
	types;

	bool operator()(char *&out, const size_t &max, const spec &, const arg &val) const override;
	using specifier::specifier;
}
const string_specifier
{
	"s"s
};

decltype(string_specifier::types)
string_specifier::types
{};

struct bool_specifier
:specifier
{
	static const std::tuple
	<
		bool,
		char,       unsigned char,
		short,      unsigned short,
		int,        unsigned int,
		long,       unsigned long,
		long long,  unsigned long long
	>
	types;

	bool operator()(char *&out, const size_t &max, const spec &, const arg &val) const override;
	using specifier::specifier;
}
const bool_specifier
{
	{ "b"s }
};

decltype(bool_specifier::types)
bool_specifier::types
{};

struct signed_specifier
:specifier
{
	static const std::tuple
	<
		bool,
		char,       unsigned char,
		short,      unsigned short,
		int,        unsigned int,
		long,       unsigned long,
		long long,  unsigned long long
	>
	types;

	bool operator()(char *&out, const size_t &max, const spec &, const arg &val) const override;
	using specifier::specifier;
}
const signed_specifier
{
	{ "d"s, "ld"s, "zd"s }
};

decltype(signed_specifier::types)
signed_specifier::types
{};

struct unsigned_specifier
:specifier
{
	static const std::tuple
	<
		bool,
		char,       unsigned char,
		short,      unsigned short,
		int,        unsigned int,
		long,       unsigned long,
		long long,  unsigned long long
	>
	types;

	bool operator()(char *&out, const size_t &max, const spec &, const arg &val) const override;
	using specifier::specifier;
}
const unsigned_specifier
{
	{ "u"s, "lu"s, "zu"s }
};

struct hex_lowercase_specifier
:specifier
{
	static const std::tuple
	<
		bool,
		char,       unsigned char,
		short,      unsigned short,
		int,        unsigned int,
		long,       unsigned long,
		long long,  unsigned long long
	>
	types;

	bool operator()(char *&out, const size_t &max, const spec &, const arg &val) const override;
	using specifier::specifier;
}
const hex_lowercase_specifier
{
	{ "x"s, "lx"s }
};

decltype(hex_lowercase_specifier::types)
hex_lowercase_specifier::types
{};

decltype(unsigned_specifier::types)
unsigned_specifier::types
{};

struct float_specifier
:specifier
{
	static const std::tuple
	<
		char,   unsigned char,
		short,  unsigned short,
		int,    unsigned int,
		long,   unsigned long,
		float,  double
	>
	types;

	bool operator()(char *&out, const size_t &max, const spec &, const arg &val) const override;
	using specifier::specifier;
}
const float_specifier
{
	{ "f"s, "lf"s }
};

decltype(float_specifier::types)
float_specifier::types
{};

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

struct pointer_specifier
:specifier
{
	bool operator()(char *&out, const size_t &max, const spec &, const arg &val) const override;
	using specifier::specifier;
}
const pointer_specifier
{
	"p"s
};

} // namespace fmt
} // namespace ircd

using namespace ircd;

fmt::snprintf::snprintf(internal_t,
                        char *const &out,
                        const size_t &max,
                        const char *const &fstr,
                        const va_rtti &v)
try
:fstart{strchr(fstr, SPECIFIER)}
,fstop{fstr}
,fend{fstr + strnlen(fstr, max)}
,obeg{out}
,oend{out + max}
,out{out}
,idx{0}
{
	if(unlikely(!max))
	{
		fstart = nullptr;
		return;
	}

	if(!fstart)
	{
		const mutable_buffer dst{out, max};
		const const_buffer src{fstr, fend};
		this->out += size_t(strlcpy(dst, src));
		return;
	}

	append(fstr, fstart);
	auto it(begin(v));
	for(size_t i(0); i < v.size() && !finished(); ++it, i++)
	{
		const auto &ptr(get<0>(*it));
		const auto &type(get<1>(*it));
		argument(std::make_tuple(ptr, std::type_index(*type)));
	}

	assert(this->out >= obeg);
	assert(this->out < oend);
	*this->out = '\0';
}
catch(const std::out_of_range &e)
{
	throw invalid_format
	{
		"Format string requires more than %zu arguments.", v.size()
	};
}

void
fmt::snprintf::argument(const arg &val)
{
	fmt::spec spec;
	if(qi::parse(fstart, fend, parser, spec))
		handle_specifier(out, remaining(), idx++, spec, val);

	fstop = fstart;
	if(fstart >= fend)
		return;

	fstart = strchr(fstart, SPECIFIER);
	append(fstop, fstart?: fend);
}

void
fmt::snprintf::append(const char *const &begin,
                      const char *const &end)
{
	assert(begin <= end);
	const size_t rem(remaining());
	const size_t len(end - begin);
	const size_t &cpsz
	{
		std::min(len, rem)
	};

	memcpy(out, begin, cpsz);
	out += cpsz;
	assert(out < oend);
}

size_t
fmt::snprintf::remaining()
const
{
	assert(out < oend);
	assert(obeg <= oend);
	const ssize_t r(oend - out);
	return std::max(r - 1, ssize_t(0));
}

bool
fmt::snprintf::finished()
const
{
	return !fstart || fstop >= fend || remaining() == 0;
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
			throw error
			{
				"Specifier '%s' already registered\n", name
			};

	for(const auto &name : this->names)
		specifiers.emplace(name, this);
}

fmt::specifier::~specifier()
noexcept
{
	for(const auto &name : names)
		specifiers.erase(name);
}

bool
fmt::is_specifier(const string_view &name)
{
	return specifiers.count(name);
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
	const auto &handler(*specifiers.at(spec.name));
	if(unlikely(!handler(out, max, spec, val)))
		throw invalid_type
		{
			"`%s' (%s) for format specifier '%s' for argument #%u",
			demangle(type.name()),
			type.name(),
			spec.name,
			idx
		};
}
catch(const std::out_of_range &e)
{
	throw invalid_format
	{
		"Unhandled specifier `%s' for argument #%u in format string",
		spec.name,
		idx
	};
}
catch(const illegal &e)
{
	throw illegal
	{
		"Specifier `%s' for argument #%u: %s",
		spec.name,
		idx,
		e.what()
	};
}

template<class T,
         class lambda>
bool
fmt::visit_type(const arg &val,
                lambda&& closure)
{
	const auto &ptr(get<0>(val));
	const auto &type(get<1>(val));
	return type == typeid(T)? closure(*static_cast<const T *>(ptr)) : false;
}

bool
fmt::pointer_specifier::operator()(char *&out,
                                   const size_t &max,
                                   const spec &,
                                   const arg &val)
const
{
	using karma::ulong_;
	using karma::eps;
	using karma::maxwidth;

	static const auto throw_illegal([]
	{
		throw illegal("Not a pointer");
	});

	struct generator
	:karma::grammar<char *, uintptr_t()>
	{
		karma::rule<char *, uintptr_t()> pointer_hex
		{
			lit("0x") << karma::hex
		};

		generator(): generator::base_type{pointer_hex} {}
	}
	static const generator;

	const auto &ptr(get<0>(val));
	const auto &type(get<1>(val));
	const void *const p(*static_cast<const void *const *>(ptr));
	return karma::generate(out, maxwidth(max)[generator] | eps[throw_illegal], uintptr_t(p));
}

bool
fmt::char_specifier::operator()(char *&out,
                                const size_t &max,
                                const spec &,
                                const arg &val)
const
{
	using karma::eps;
	using karma::maxwidth;

	static const auto throw_illegal([]
	{
		throw illegal("Not a printable character");
	});

	struct generator
	:karma::grammar<char *, char()>
	{
		karma::rule<char *, char()> printable
		{
			karma::print
			,"character"
		};

		generator(): generator::base_type{printable} {}
	}
	static const generator;

	const auto &ptr(get<0>(val));
	const auto &type(get<1>(val));
	if(type == typeid(const char))
	{
		const auto &c(*static_cast<const char *>(ptr));
		karma::generate(out, maxwidth(max)[generator] | eps[throw_illegal], c);
		return true;
	}
	else return false;
}

bool
fmt::bool_specifier::operator()(char *&out,
                                const size_t &max,
                                const spec &,
                                const arg &val)
const
{
	using karma::eps;
	using karma::maxwidth;

	static const auto throw_illegal([]
	{
		throw illegal("Failed to print signed value");
	});

	const auto closure([&](const bool &boolean)
	{
		using karma::maxwidth;

		struct generator
		:karma::grammar<char *, bool()>
		{
			karma::rule<char *, bool()> rule
			{
				karma::bool_
				,"boolean"
			};

			generator(): generator::base_type{rule} {}
		}
		static const generator;

		return karma::generate(out, maxwidth(max)[generator] | eps[throw_illegal], boolean);
	});

	return !until(types, [&](auto type)
	{
		return !visit_type<decltype(type)>(val, closure);
	});
}

bool
fmt::signed_specifier::operator()(char *&out,
                                  const size_t &max,
                                  const spec &s,
                                  const arg &val)
const
{
	static const auto throw_illegal([]
	{
		throw illegal("Failed to print signed value");
	});

	const auto closure([&](const long &integer)
	{
		using karma::long_;
		using karma::maxwidth;

		struct generator
		:karma::grammar<char *, long()>
		{
			karma::rule<char *, long()> rule
			{
				long_
				,"signed long integer"
			};

			generator(): generator::base_type{rule} {}
		}
		static const generator;

		return karma::generate(out, maxwidth(max)[generator] | eps[throw_illegal], integer);
	});

	return !until(types, [&](auto type)
	{
		return !visit_type<decltype(type)>(val, closure);
	});
}

bool
fmt::unsigned_specifier::operator()(char *&out,
                                    const size_t &max,
                                    const spec &s,
                                    const arg &val)
const
{
	static const auto throw_illegal([]
	{
		throw illegal("Failed to print unsigned value");
	});

	const auto closure([&](const ulong &integer)
	{
		using karma::ulong_;
		using karma::maxwidth;

		struct generator
		:karma::grammar<char *, ulong()>
		{
			karma::rule<char *, ulong()> rule
			{
				ulong_
				,"unsigned long integer"
			};

			generator(): generator::base_type{rule} {}
		}
		static const generator;

		return karma::generate(out, maxwidth(max)[generator] | eps[throw_illegal], integer);
	});

	return !until(types, [&](auto type)
	{
		return !visit_type<decltype(type)>(val, closure);
	});
}

bool
fmt::hex_lowercase_specifier::operator()(char *&out,
                                         const size_t &max,
                                         const spec &s,
                                         const arg &val)
const
{
	static const auto throw_illegal([]
	{
		throw illegal("Failed to print hexadecimal value");
	});

	const auto closure([&](const uint &integer)
	{
		using karma::maxwidth;

		struct generator
		:karma::grammar<char *, uint()>
		{
			karma::rule<char *, uint()> rule
			{
				karma::lower[karma::hex]
				,"unsigned lowercase hexadecimal"
			};

			generator(): generator::base_type{rule} {}
		}
		static const generator;

		return karma::generate(out, maxwidth(max)[generator] | eps[throw_illegal], integer);
	});

	return !until(types, [&](auto type)
	{
		return !visit_type<decltype(type)>(val, closure);
	});
}

bool
fmt::float_specifier::operator()(char *&out,
                                 const size_t &max,
                                 const spec &s,
                                 const arg &val)
const
{
	static const auto throw_illegal([]
	{
		throw illegal("Failed to print floating point value");
	});

	const auto closure([&](const double &floating)
	{
		using karma::double_;
		using karma::maxwidth;

		struct generator
		:karma::grammar<char *, double()>
		{
			karma::rule<char *, double()> rule
			{
				double_
				,"floating point integer"
			};

			generator(): generator::base_type{rule} {}
		}
		static const generator;

		return karma::generate(out, maxwidth(max)[generator] | eps[throw_illegal], floating);
	});

	return !until(types, [&](auto type)
	{
		return !visit_type<decltype(type)>(val, closure);
	});
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
	using karma::unused_type;

	static const auto throw_illegal([]
	{
		throw illegal("Not a printable string");
	});

	struct generator
	:karma::grammar<char *, const string_view &>
	{
		karma::rule<char *, const string_view &> string
		{
			*(karma::print)
			,"string"
		};

		generator() :generator::base_type{string} {}
	}
	static const generator;

	return generate_string(out, maxwidth(max)[generator] | eps[throw_illegal], val);
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
	if(type == typeid(ircd::string_view) ||
	   type == typeid(ircd::json::string) ||
	   type == typeid(ircd::json::object) ||
	   type == typeid(ircd::json::array))
	{
		const auto &str(*static_cast<const ircd::string_view *>(ptr));
		return karma::generate(out, gen, str);
	}
	else if(type == typeid(std::string_view))
	{
		const auto &str(*static_cast<const std::string_view *>(ptr));
		return karma::generate(out, gen, str);
	}
	else if(type == typeid(std::string))
	{
		const auto &str(*static_cast<const std::string *>(ptr));
		return karma::generate(out, gen, string_view{str});
	}
	else if(type == typeid(const char *))
	{
		const char *const &str{*static_cast<const char *const *const>(ptr)};
		return karma::generate(out, gen, string_view{str});
	}

	// This for string literals which have unique array types depending on their size.
	// There is no reasonable way to match them. The best that can be hoped for is the
	// grammar will fail gracefully (most of the time) or not print something bogus when
	// it happens to be legal.
	const auto &str(static_cast<const char *>(ptr));
	return karma::generate(out, gen, string_view{str});
}
