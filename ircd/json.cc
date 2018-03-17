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
#include <boost/fusion/include/at.hpp>

namespace ircd::json
{
	using namespace ircd::spirit;

	template<class it> struct input;
	template<class it> struct output;

	// Instantiations of the grammars
	struct parser extern const parser;
	struct printer extern const printer;
	struct ostreamer extern const ostreamer;
}

BOOST_FUSION_ADAPT_STRUCT
(
    ircd::json::member,
    ( decltype(ircd::json::member::first),   first )
    ( decltype(ircd::json::member::second),  second )
)

BOOST_FUSION_ADAPT_STRUCT
(
    ircd::json::object::member,
    ( decltype(ircd::json::object::member::first),   first )
    ( decltype(ircd::json::object::member::second),  second )
)

template<class it>
struct ircd::json::input
:qi::grammar<it, unused_type>
{
	template<class T = unused_type> using rule = qi::rule<it, T>;

	rule<> NUL                         { lit('\0')                                          ,"nul" };

	// insignificant whitespaces
	rule<> SP                          { lit('\x20')                                      ,"space" };
	rule<> HT                          { lit('\x09')                             ,"horizontal tab" };
	rule<> CR                          { lit('\x0D')                            ,"carriage return" };
	rule<> LF                          { lit('\x0A')                                  ,"line feed" };

	// whitespace skipping
	rule<> WS                          { SP | HT | CR | LF                           ,"whitespace" };
	rule<> ws                          { *(WS)                                ,"whitespace monoid" };
	rule<> wsp                         { +(WS)                             ,"whitespace semigroup" };

	// structural
	rule<> object_begin                { lit('{')                                  ,"object begin" };
	rule<> object_end                  { lit('}')                                    ,"object end" };
	rule<> array_begin                 { lit('[')                                   ,"array begin" };
	rule<> array_end                   { lit(']')                                     ,"array end" };
	rule<> name_sep                    { lit(':')                                      ,"name sep" };
	rule<> value_sep                   { lit(',')                                     ,"value sep" };
	rule<> escape                      { lit('\\')                                       ,"escape" };
	rule<> quote                       { lit('"')                                         ,"quote" };

	// literal
	rule<> lit_false                   { lit("false")                             ,"literal false" };
	rule<> lit_true                    { lit("true")                               ,"literal true" };
	rule<> lit_null                    { lit("null")                                       ,"null" };
	rule<> boolean                     { lit_true | lit_false                           ,"boolean" };
	rule<> literal                     { lit_true | lit_false | lit_null                ,"literal" };

	// numerical (TODO: exponent)
	rule<> number                      { double_                                         ,"number" };

	// string
	rule<> unicode
	{
		lit('u') >> qi::uint_parser<char, 16, 4, 4>{}
		,"escaped unicode"
	};

	rule<> escaped
	{
		lit('"') | lit('\\') | lit('\b') | lit('\f') | lit('\n') | lit('\r') | lit('\t')
		,"escaped"
	};

	rule<> escaper
	{
		lit('"') | lit('\\') | lit('b') | lit('f') | lit('n') | lit('r') | lit('t') | unicode
		,"escaped"
	};

	rule<> escaper_nc
	{
		escaper | lit('/')
	};

	rule<string_view> chars
	{
		raw[*((char_ - (escape | quote)) | (escape >> escaper_nc))]
		,"characters"
	};

	rule<string_view> string
	{
		quote >> chars >> (!escape >> quote)
		,"string"
	};

	// container
	rule<string_view> name
	{
		string
		,"name"
	};

	// recursion depth
	_r1_type depth;
	[[noreturn]] static void throws_exceeded();

	const rule<unused_type(uint)> member
	{
		name >> name_sep >> value(depth)
		,"member"
	};

	const rule<unused_type(uint)> object
	{
		(eps(depth < json::object::max_recursion_depth) | eps[throws_exceeded]) >>

		object_begin >> -(member(depth) % value_sep) >> object_end
		,"object"
	};

	const rule<unused_type(uint)> array
	{
		(eps(depth < json::array::max_recursion_depth) | eps[throws_exceeded]) >>

		array_begin >> -(value(depth) % value_sep) >> array_end
		,"array"
	};

	const rule<unused_type(uint)> value
	{
		lit_false | lit_null | lit_true | object(depth + 1) | array(depth + 1) | number | string
		,"value"
	};

	rule<int> type
	{
		(omit[object_begin]    >> attr(json::OBJECT))  |
		(omit[array_begin]     >> attr(json::ARRAY))   |
		(omit[quote]           >> attr(json::STRING))  |
		(omit[number >> eoi]   >> attr(json::NUMBER))  |
		(omit[literal >> eoi]  >> attr(json::LITERAL))
		,"type"
	};

	input()
	:input::base_type{rule<>{}}
	{}
};

template<class it>
struct ircd::json::output
:karma::grammar<it, unused_type>
{
	template<class T = unused_type> using rule = karma::rule<it, T>;

	rule<> NUL                         { lit('\0')                                          ,"nul" };

	// insignificant whitespaces
	rule<> SP                          { lit('\x20')                                      ,"space" };
	rule<> HT                          { lit('\x09')                             ,"horizontal tab" };
	rule<> CR                          { lit('\x0D')                            ,"carriage return" };
	rule<> LF                          { lit('\x0A')                                  ,"line feed" };

	// whitespace skipping
	rule<> WS                          { SP | HT | CR | LF                           ,"whitespace" };
	rule<> ws                          { *(WS)                                ,"whitespace monoid" };
	rule<> wsp                         { +(WS)                             ,"whitespace semigroup" };

	// structural
	rule<> object_begin                { lit('{')                                  ,"object begin" };
	rule<> object_end                  { lit('}')                                    ,"object end" };
	rule<> array_begin                 { lit('[')                                   ,"array begin" };
	rule<> array_end                   { lit(']')                                     ,"array end" };
	rule<> name_sep                    { lit(':')                                ,"name separator" };
	rule<> value_sep                   { lit(',')                               ,"value separator" };
	rule<> quote                       { lit('"')                                         ,"quote" };
	rule<> escape                      { lit('\\')                                       ,"escape" };

	rule<string_view> lit_true         { karma::string("true")                     ,"literal true" };
	rule<string_view> lit_false        { karma::string("false")                   ,"literal false" };
	rule<string_view> lit_null         { karma::string("null")                     ,"literal null" };
	rule<string_view> boolean          { lit_true | lit_false                           ,"boolean" };
	rule<string_view> literal          { lit_true | lit_false | lit_null                ,"literal" };
	rule<string_view> number           { double_                                         ,"number" };

	rule<string_view> chars
	{
		*char_ //TODO: exacting
		,"characters"
	};

	rule<string_view> string
	{
		quote << chars << quote
		,"string"
	};

	rule<string_view> name
	{
		string
		,"name"
	};

	output()
	:output::base_type{rule<>{}}
	{}
};

struct ircd::json::expectation_failure
:parse_error
{
	expectation_failure(const char *const &start,
	                    const qi::expectation_failure<const char *> &e,
	                    const ssize_t &show_max = 64)
	:parse_error
	{
		"Expected %s. You input %zd invalid characters at position %zd: %s",
		ircd::string(e.what_),
		std::distance(e.first, e.last),
		std::distance(start, e.first),
		string_view{e.first, e.first + std::min(std::distance(e.first, e.last), show_max)}
	}{}
};

struct ircd::json::parser
:input<const char *>
{
	using input::input;
}
const ircd::json::parser;

struct ircd::json::printer
:output<char *>
{
	template<class gen,
	         class... attr>
	bool operator()(mutable_buffer &out, gen&&, attr&&...) const;

	template<class gen>
	bool operator()(mutable_buffer &out, gen&&) const;

	template<class it_a,
	         class it_b,
	         class closure>
	static void list_protocol(mutable_buffer &, it_a begin, const it_b &end, closure&&);
}
const ircd::json::printer;

struct ircd::json::ostreamer
:output<karma::ostream_iterator<char>>
{
}
const ircd::json::ostreamer;

template<class gen,
         class... attr>
bool
ircd::json::printer::operator()(mutable_buffer &out,
                                gen&& g,
                                attr&&... a)
const
{
	const auto maxwidth
	{
		karma::maxwidth(size(out))
	};

	const auto gg
	{
		maxwidth[std::forward<gen>(g)]
	};

	const auto throws{[&out]
	{
		throw print_error
		{
			"Failed to print attributes '%s' generator '%s' (%zd bytes in buffer)",
			demangle<decltype(a)...>(),
			demangle<decltype(g)>(),
			size(out)
		};
	}};

	return karma::generate(begin(out), gg | eps[throws], std::forward<attr>(a)...);
}

template<class gen>
bool
ircd::json::printer::operator()(mutable_buffer &out,
                                gen&& g)
const
{
	const auto maxwidth
	{
		karma::maxwidth(size(out))
	};

	const auto gg
	{
		maxwidth[std::forward<gen>(g)]
	};

	const auto throws{[&out]
	{
		throw print_error
		{
			"Failed to print generator '%s' (%zd bytes in buffer)",
			demangle<decltype(g)>(),
			size(out)
		};
	}};

	return karma::generate(begin(out), gg | eps[throws]);
}

template<class it_a,
         class it_b,
         class closure>
void
ircd::json::printer::list_protocol(mutable_buffer &buffer,
                                   it_a it,
                                   const it_b &end,
                                   closure&& lambda)
{
	if(it != end)
	{
		lambda(buffer, *it);
		for(++it; it != end; ++it)
		{
			json::printer(buffer, json::printer.value_sep);
			lambda(buffer, *it);
		}
	}
}

template<class it>
void
ircd::json::input<it>::throws_exceeded()
{
	throw recursion_limit
	{
		"Maximum recursion depth exceeded"
	};
}

///////////////////////////////////////////////////////////////////////////////
//
// iov.h
//

std::ostream &
ircd::json::operator<<(std::ostream &s, const iov &iov)
{
	s << json::strung(iov);
	return s;
}

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const iov &iov)
{
	const member *m[iov.size()];
	std::transform(std::begin(iov), std::end(iov), m, []
	(const member &m)
	{
		return &m;
	});

	std::sort(m, m + iov.size(), []
	(const member *const &a, const member *const &b)
	{
		return *a < *b;
	});

	static const auto print_member
	{
		[](mutable_buffer &buf, const member *const &m)
		{
			printer(buf, printer.name << printer.name_sep, m->first);
			stringify(buf, m->second);
		}
	};

	char *const start{begin(buf)};
	printer(buf, printer.object_begin);
	printer::list_protocol(buf, m, m + iov.size(), print_member);
	printer(buf, printer.object_end);
	return { start, begin(buf) };
}

size_t
ircd::json::serialized(const iov &iov)
{
	const size_t ret
	{
		1U + iov.empty()
	};

	return std::accumulate(std::begin(iov), std::end(iov), ret, []
	(auto ret, const auto &member)
	{
		return ret += serialized(member) + 1;
	});
}

bool
ircd::json::iov::has(const string_view &key)
const
{
	return std::any_of(std::begin(*this), std::end(*this), [&key]
	(const auto &member)
	{
		return string_view{member.first} == key;
	});
}

const ircd::json::value &
ircd::json::iov::at(const string_view &key)
const
{
	const auto it
	{
		std::find_if(std::begin(*this), std::end(*this), [&key]
		(const auto &member)
		{
			return string_view{member.first} == key;
		})
	};

	if(it == std::end(*this))
		throw not_found
		{
			"key '%s' not found", key
		};

	return it->second;
}

ircd::json::iov::add::add(iov &iov,
                          member member)
:node
{
	iov, [&iov, &member]
	{
		if(iov.has(member.first))
			throw exists("failed to add member '%s': already exists",
			             string_view{member.first});

		return std::move(member);
	}()
}
{
}

ircd::json::iov::add_if::add_if(iov &iov,
                                const bool &b,
                                member member)
:node
{
	iov, std::move(member)
}
{
	if(!b)
		iov.pop_front();
}

ircd::json::iov::set::set(iov &iov, member member)
:node
{
	iov, [&iov, &member]
	{
		iov.remove_if([&member](const auto &existing)
		{
			return string_view{existing.first} == string_view{member.first};
		});

		return std::move(member);
	}()
}
{
}

ircd::json::iov::set_if::set_if(iov &iov,
                                const bool &b,
                                member member)
:node
{
	iov, std::move(member)
}
{
	if(!b)
		iov.pop_front();
}

ircd::json::iov::defaults::defaults(iov &iov,
                                    member member)
:node
{
	iov, std::move(member)
}
{
	const auto count
	{
		std::count_if(std::begin(iov), std::end(iov), [&member]
		(const auto &existing)
		{
			return string_view{existing.first} == string_view{member.first};
		})
	};

	if(count > 1)
		iov.pop_front();
}

ircd::json::iov::defaults_if::defaults_if(iov &iov,
                                          const bool &b,
                                          member member)
:node
{
	iov, std::move(member)
}
{
	if(!b)
		iov.pop_front();
}

///////////////////////////////////////////////////////////////////////////////
//
// json/vector.h
//

ircd::json::vector::const_iterator &
ircd::json::vector::const_iterator::operator++()
try
{
	static const qi::rule<const char *, string_view> parse_next
	{
		raw[parser.object(0)] | qi::eoi
		,"next vector element or end"
	};

	string_view state;
	qi::parse(start, stop, eps > parse_next, state);
	this->state = state;
	return *this;
}
catch(const qi::expectation_failure<const char *> &e)
{
	throw expectation_failure(start, e);
}

ircd::json::vector::const_iterator
ircd::json::vector::begin()
const try
{
	static const qi::rule<const char *, string_view> parse_begin
	{
		raw[parser.object(0)]
		,"object vector element"
	};

	const_iterator ret
	{
		string_view::begin(), string_view::end()
	};

	if(!string_view{*this}.empty())
	{
		string_view state;
		qi::parse(ret.start, ret.stop, eps > parse_begin, state);
		ret.state = state;
	}

	return ret;
}
catch(const qi::expectation_failure<const char *> &e)
{
	throw expectation_failure(string_view::begin(), e);
}

ircd::json::vector::const_iterator
ircd::json::vector::end()
const
{
	return { string_view::end(), string_view::end() };
}

///////////////////////////////////////////////////////////////////////////////
//
// json/member.h
//

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const members &list)
{
	return stringify(buf, std::begin(list), std::end(list));
}

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const member &m)
{
	return stringify(buf, &m, &m + 1);
}

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const member *const &b,
                      const member *const &e)
{
	static const auto print_member
	{
		[](mutable_buffer &buf, const member &m)
		{
			printer(buf, printer.name << printer.name_sep, m.first);
			stringify(buf, m.second);
		}
	};

	char *const start{begin(buf)};
	printer(buf, printer.object_begin);
	printer::list_protocol(buf, b, e, print_member);
	printer(buf, printer.object_end);
	return { start, begin(buf) };
}

size_t
ircd::json::serialized(const members &m)
{
	return serialized(std::begin(m), std::end(m));
}

size_t
ircd::json::serialized(const member *const &begin,
                       const member *const &end)
{
	const size_t ret(1 + !std::distance(begin, end));
	return std::accumulate(begin, end, ret, []
	(auto ret, const auto &member)
	{
		return ret += serialized(member) + 1;
	});
}

size_t
ircd::json::serialized(const member &member)
{
	return serialized(member.first) + 1 + serialized(member.second);
}

///////////////////////////////////////////////////////////////////////////////
//
// json/object.h
//

decltype(ircd::json::object::max_recursion_depth)
const ircd::json::object::max_recursion_depth
{
	32
};

std::ostream &
ircd::json::operator<<(std::ostream &s, const object::member &member)
{
	s << json::strung(member);
	return s;
}

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const object::member &member)
{
	char *const start(begin(buf));
	printer(buf, printer.name, member.first);
	printer(buf, printer.name_sep);
	consume(buf, copy(buf, member.second));
	return string_view { start, begin(buf) };
}

size_t
ircd::json::serialized(const object::member &member)
{
	return serialized(member.first) + 1 + serialized(member.second);
}

std::ostream &
ircd::json::operator<<(std::ostream &s, const object &object)
{
	s << json::strung(object);
	return s;
}

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const object &object)
{
	const auto b(std::begin(object));
	const auto e(std::end(object));
	char *const start(begin(buf));
	static const auto stringify_member
	{
		[](mutable_buffer &buf, const object::member &member)
		{
			stringify(buf, member);
		}
	};

	printer(buf, printer.object_begin);
	printer::list_protocol(buf, b, e, stringify_member);
	printer(buf, printer.object_end);
	return { start, begin(buf) };
}

size_t
ircd::json::serialized(const object &object)
{
	const auto begin(std::begin(object));
	const auto end(std::end(object));
	const size_t ret(1 + (begin == end));
	return std::accumulate(begin, end, ret, []
	(auto ret, const object::member &member)
	{
		return ret += serialized(member) + 1;
	});
}

ircd::json::object::const_iterator &
ircd::json::object::const_iterator::operator++()
try
{
	static const qi::rule<const char *, json::object::member> member
	{
		parser.name >> parser.name_sep >> raw[parser.value(0)]
		,"next object member"
	};

	static const qi::rule<const char *, json::object::member> parse_next
	{
		(parser.value_sep >> member) | parser.object_end
		,"next object member or end"
	};

	state.first = string_view{};
	state.second = string_view{};
	qi::parse(start, stop, eps > parse_next, state);
	return *this;
}
catch(const qi::expectation_failure<const char *> &e)
{
	throw expectation_failure(start, e);
}

ircd::json::object::operator std::string()
const
{
	return json::strung(*this);
}

ircd::json::object::const_iterator
ircd::json::object::begin()
const try
{
	static const qi::rule<const char *, json::object::member> object_member
	{
		parser.name >> parser.name_sep >> raw[parser.value(0)]
		,"object member"
	};

	static const qi::rule<const char *, json::object::member> parse_begin
	{
		parser.object_begin >> (parser.object_end | object_member)
		,"object begin and member or end"
	};

	const_iterator ret
	{
		string_view::begin(), string_view::end()
	};

	if(!string_view{*this}.empty())
		qi::parse(ret.start, ret.stop, eps > parse_begin, ret.state);

	return ret;
}
catch(const qi::expectation_failure<const char *> &e)
{
	throw expectation_failure(string_view::begin(), e);
}

ircd::json::object::const_iterator
ircd::json::object::end()
const
{
	return { string_view::end(), string_view::end() };
}

///////////////////////////////////////////////////////////////////////////////
//
// json/array.h
//

decltype(ircd::json::array::max_recursion_depth)
ircd::json::array::max_recursion_depth
{
	32
};

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const array &v)
{
	if(string_view{v}.empty())
	{
		consume(buf, copy(buf, empty_array));
		return empty_array;
	}

	consume(buf, copy(buf, string_view{v}));
	return string_view{v};
}

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const std::string *const &b,
                      const std::string *const &e)
{
	return array::stringify(buf, b, e);
}

size_t
ircd::json::serialized(const std::string *const &b,
                       const std::string *const &e)
{
	const size_t ret(1 + !std::distance(b, e));
	return std::accumulate(b, e, ret, []
	(auto ret, const auto &value)
	{
		return ret += serialized(string_view{value}) + 1;
	});
}

size_t
ircd::json::serialized(const string_view *const &b,
                       const string_view *const &e)
{
	const size_t ret(1 + !std::distance(b, e));
	return std::accumulate(b, e, ret, []
	(auto ret, const auto &value)
	{
		return ret += serialized(value) + 1;
	});
}

template<class it>
ircd::string_view
ircd::json::array::stringify(mutable_buffer &buf,
                             const it &b,
                             const it &e)
{
	static const auto print_element
	{
		[](mutable_buffer &buf, const string_view &element)
		{
			if(!consume(buf, ircd::buffer::copy(buf, element)))
				throw print_error("The JSON generator ran out of space in supplied buffer");
		}
	};

	char *const start(std::begin(buf));
	printer(buf, printer.array_begin);
	printer::list_protocol(buf, b, e, print_element);
	printer(buf, printer.array_end);
	return { start, std::begin(buf) };
}

std::ostream &
ircd::json::operator<<(std::ostream &s, const array &a)
{
	s << json::strung(a);
	return s;
}

ircd::json::array::const_iterator &
ircd::json::array::const_iterator::operator++()
try
{
	static const qi::rule<const char *, string_view> value
	{
		raw[parser.value(0)]
		,"array element"
	};

	static const qi::rule<const char *, string_view> parse_next
	{
		parser.array_end | (parser.value_sep >> value)
		,"next array element or end"
	};

	state = string_view{};
	qi::parse(start, stop, eps > parse_next, state);
	return *this;
}
catch(const qi::expectation_failure<const char *> &e)
{
	throw expectation_failure(start, e);
}

ircd::json::array::operator std::string()
const
{
	return json::strung(*this);
}

ircd::json::array::const_iterator
ircd::json::array::begin()
const try
{
	static const qi::rule<const char *, string_view> value
	{
		raw[parser.value(0)]
		,"array element"
	};

	static const qi::rule<const char *, string_view> parse_begin
	{
		parser.array_begin >> (parser.array_end | value)
		,"array begin and element or end"
	};

	const_iterator ret
	{
		string_view::begin(), string_view::end()
	};

	if(!string_view{*this}.empty())
		qi::parse(ret.start, ret.stop, eps > parse_begin, ret.state);

	return ret;
}
catch(const qi::expectation_failure<const char *> &e)
{
	throw expectation_failure(string_view::begin(), e);
}

ircd::json::array::const_iterator
ircd::json::array::end()
const
{
	return { string_view::end(), string_view::end() };
}

///////////////////////////////////////////////////////////////////////////////
//
// json/value.h
//

const ircd::string_view ircd::json::literal_null   { "null"   };
const ircd::string_view ircd::json::literal_true   { "true"   };
const ircd::string_view ircd::json::literal_false  { "false"  };
const ircd::string_view ircd::json::empty_string   { "\"\""   };
const ircd::string_view ircd::json::empty_object   { "{}"     };
const ircd::string_view ircd::json::empty_array    { "[]"     };

std::ostream &
ircd::json::operator<<(std::ostream &s, const value &v)
{
	s << json::strung(v);
	return s;
}

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const value *const &b,
                      const value *const &e)
{
	static const auto print_value
	{
		[](mutable_buffer &buf, const value &value)
		{
			stringify(buf, value);
		}
	};

	char *const start(begin(buf));
	printer(buf, printer.array_begin);
	printer::list_protocol(buf, b, e, print_value);
	printer(buf, printer.array_end);
	return { start, begin(buf) };
}

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const value &v)
{
	const auto start
	{
		begin(buf)
	};

	switch(v.type)
	{
		case STRING:
		{
			const string_view sv{v};
			printer(buf, printer.string, sv);
			break;
		}

		case LITERAL:
		{
			consume(buf, copy(buf, string_view{v}));
			break;
		}

		case OBJECT:
		{
			if(v.serial)
			{
				consume(buf, copy(buf, string_view{v}));
				break;
			}

			if(v.object)
			{
				stringify(buf, v.object, v.object + v.len);
				break;
			}

			//consume(buf, copy(buf, literal_null));
			consume(buf, copy(buf, empty_object));
			break;
		}

		case ARRAY:
		{
			if(v.serial)
			{
				consume(buf, copy(buf, string_view{v}));
				break;
			}

			if(v.array)
			{
				stringify(buf, v.array, v.array + v.len);
				break;
			}

			//consume(buf, copy(buf, literal_null));
			consume(buf, copy(buf, empty_array));
			break;
		}

		case NUMBER:
		{
			if(v.serial)
			{
				consume(buf, copy(buf, string_view{v}));
				break;
			}

			if(v.floats)
				printer(buf, double_, v.floating);
			else
				printer(buf, long_, v.integer);

			break;
		}
	}

	return { start, begin(buf) };
}

size_t
ircd::json::serialized(const values &v)
{
	return serialized(std::begin(v), std::end(v));
}

size_t
ircd::json::serialized(const value *const &begin,
                       const value *const &end)
{
	// One opening '[' and either one ']' or comma count.
	const size_t ret(1 + !std::distance(begin, end));
	return std::accumulate(begin, end, size_t(ret), []
	(auto ret, const value &v)
	{
		return ret += serialized(v) + 1; // 1 comma
	});
}

size_t
ircd::json::serialized(const value &v)
{
	switch(v.type)
	{
		case OBJECT:
			return v.serial? v.len : serialized(v.object, v.object + v.len);

		case ARRAY:
			return v.serial? v.len : serialized(v.array, v.array + v.len);

		case LITERAL:
		{
			return v.serial? v.len : serialized(bool(v.integer));
		}

		case NUMBER:
		{
			if(v.serial)
				return v.len;

			static thread_local char test_buffer[4096];
			const auto test
			{
				stringify(mutable_buffer{test_buffer}, v)
			};

			return test.size();
		}

		case STRING:
		{
			if(!v.string)
				return 2;

			size_t ret(v.len);
			const string_view sv{v.string, v.len};
			ret += !startswith(sv, '"');
			ret += !endswith(sv, '"');
			return ret;
		}
	};

	throw type_error("deciding the size of a type[%u] is undefined", int(v.type));
}

//
// json::value
//

ircd::json::value::value(const json::members &members)
:string{nullptr}
,len{serialized(members)}
,type{OBJECT}
,serial{true}
,alloc{true}
,floats{false}
{
	create_string(len, [&members]
	(mutable_buffer &buffer)
	{
		json::stringify(buffer, members);
	});
}

ircd::json::value::value(const value &other)
:integer{other.integer}
,len{other.len}
,type{other.type}
,serial{other.serial}
,alloc{other.alloc}
,floats{other.floats}
{
	if(serial)
	{
		create_string(len, [&other]
		(mutable_buffer &buffer)
		{
			consume(buffer, copy(buffer, string_view{other}));
		});
	}
	else switch(type)
	{
		case OBJECT:
		{
			if(!object)
				break;

			const size_t count(this->len);
			create_string(serialized(object, object + count), [this, &count]
			(mutable_buffer &buffer)
			{
				json::stringify(buffer, object, object + count);
			});
			break;
		}

		case ARRAY:
		{
			if(!array)
				break;

			const size_t count(this->len);
			create_string(serialized(array, array + count), [this, &count]
			(mutable_buffer &buffer)
			{
				json::stringify(buffer, array, array + count);
			});
			break;
		}

		case STRING:
		{
			if(!string)
				break;

			create_string(len, [&other]
			(mutable_buffer &buffer)
			{
				copy(buffer, const_buffer{other.string, other.len});
			});
			break;
		}

		case LITERAL:
		case NUMBER:
			break;
	}
}

ircd::json::value &
ircd::json::value::operator=(const value &other)
{
	this->~value();
	new (this) value(other);
	return *this;
}

ircd::json::value::~value()
noexcept
{
	if(!alloc)
		return;

	else if(serial)
		delete[] string;

	else switch(type)
	{
		case STRING:
			delete[] string;
			break;

		case OBJECT:
			delete[] object;
			break;

		case ARRAY:
			delete[] array;
			break;

		default:
			break;
	}
}

ircd::json::value::operator std::string()
const
{
	return json::strung(*this);
}

ircd::json::value::operator string_view()
const
{
	switch(type)
	{
		case STRING:
			return unquote(string_view{string, len});

		case NUMBER:
			return serial? string_view{string, len}:
			       floats? byte_view<string_view>{floating}:
			               byte_view<string_view>{integer};
		case ARRAY:
		case OBJECT:
		case LITERAL:
			if(likely(serial))
				return string_view{string, len};
			else
				break;
	}

	throw type_error("value type[%d] is not a string", int(type));
}

ircd::json::value::operator int64_t()
const
{
	switch(type)
	{
		case NUMBER:
			return likely(!floats)? integer : floating;

		case STRING:
			return lex_cast<int64_t>(string_view{*this});

		case ARRAY:
		case OBJECT:
		case LITERAL:
			break;
	}

	throw type_error("value type[%d] is not an int64_t", int(type));
}

ircd::json::value::operator double()
const
{
	switch(type)
	{
		case NUMBER:
			return likely(floats)? floating : integer;

		case STRING:
			return lex_cast<double>(string_view{*this});

		case ARRAY:
		case OBJECT:
		case LITERAL:
			break;
	}

	throw type_error("value type[%d] is not a float", int(type));
}

bool
ircd::json::value::operator!()
const
{
	switch(type)
	{
		case NUMBER:
			return floats?  !(floating > 0.0 || floating < 0.0):
			                !bool(integer);

		case STRING:
			return string?  !len || string_view{*this} == empty_string:
			                true;

		case OBJECT:
			return serial?  !len || string_view{*this} == empty_object:
			       object?  !len:
			                true;

		case ARRAY:
			return serial?  !len || (string_view{*this} == empty_array):
			       array?   !len:
			                true;

		case LITERAL:
			if(serial)
				return string == nullptr ||
				       string_view{*this} == literal_false ||
				       string_view{*this} == literal_null;
			break;
	};

	throw type_error("deciding if a type[%u] is falsy is undefined", int(type));
}

bool
ircd::json::value::empty()
const
{
	switch(type)
	{
		case NUMBER:
			return serial? !len:
			       floats? !(floating > 0.0 || floating < 0.0):
			               !bool(integer);

		case STRING:
			return !string || !len || (serial && string_view{*this} == empty_string);

		case OBJECT:
			return serial? !len || string_view{*this} == empty_object:
			       object? !len:
			               true;

		case ARRAY:
			return serial? !len || string_view{*this} == empty_array:
			       array?  !len:
			               true;

		case LITERAL:
			return serial? !len:
			               true;
	};

	throw type_error("deciding if a type[%u] is empty is undefined", int(type));
}

bool
ircd::json::value::null()
const
{
	switch(type)
	{
		case NUMBER:
			return floats?  !(floating > 0.0 || floating < 0.0):
			                !bool(integer);

		case STRING:
			return string == nullptr;

		case OBJECT:
			return serial? string == nullptr:
			       object? false:
			               true;

		case ARRAY:
			return serial? string == nullptr:
			       array?  false:
			               true;

		case LITERAL:
			return serial? string == nullptr:
			               true;
	};

	throw type_error("deciding if a type[%u] is null is undefined", int(type));
}

bool
ircd::json::value::undefined()
const
{
	switch(type)
	{
		case NUMBER:
			return false;

		case STRING:
			return string == nullptr;

		case OBJECT:
			return serial? string == nullptr:
			       object? false:
			               true;

		case ARRAY:
			return serial? string == nullptr:
			       array?  false:
			               true;

		case LITERAL:
			return serial? string == nullptr:
			               true;
	};

	throw type_error("deciding if a type[%u] is undefined is undefined", int(type));
}

void
ircd::json::value::create_string(const size_t &len,
                                 const create_string_closure &closure)
{
	const size_t max
	{
		len + 1
	};

	std::unique_ptr<char[]> string
	{
		new char[max]
	};

	mutable_buffer buffer
	{
		string.get(), len
	};

	closure(buffer);
	(string.get())[len] = '\0';
	this->alloc = true;
	this->serial = true;
	this->len = len;
	this->string = string.release();
}

bool
ircd::json::operator>(const value &a, const value &b)
{
    if(unlikely(type(a) != STRING || type(b) != STRING))
        throw type_error("cannot compare values");

    return static_cast<string_view>(a) > static_cast<string_view>(b);
}

bool
ircd::json::operator<(const value &a, const value &b)
{
    if(unlikely(type(a) != STRING || type(b) != STRING))
        throw type_error("cannot compare values");

    return static_cast<string_view>(a) < static_cast<string_view>(b);
}

bool
ircd::json::operator>=(const value &a, const value &b)
{
    if(unlikely(type(a) != STRING || type(b) != STRING))
        throw type_error("cannot compare values");

    return static_cast<string_view>(a) >= static_cast<string_view>(b);
}

bool
ircd::json::operator<=(const value &a, const value &b)
{
    if(unlikely(type(a) != STRING || type(b) != STRING))
        throw type_error("cannot compare values");

    return static_cast<string_view>(a) <= static_cast<string_view>(b);
}

bool
ircd::json::operator!=(const value &a, const value &b)
{
    if(unlikely(type(a) != STRING || type(b) != STRING))
        throw type_error("cannot compare values");

    return static_cast<string_view>(a) != static_cast<string_view>(b);
}

bool
ircd::json::operator==(const value &a, const value &b)
{
    if(unlikely(type(a) != STRING || type(b) != STRING))
        throw type_error("cannot compare values");

    return static_cast<string_view>(a) == static_cast<string_view>(b);
}

///////////////////////////////////////////////////////////////////////////////
//
// json.h
//

std::string
ircd::json::canonize(const string_view &in)
{
	std::string ret(size(in), char{});
	ret.resize(size(canonize(mutable_buffer{ret}, in)));
	return ret;
}

ircd::string_view
ircd::json::canonize(const mutable_buffer &out,
                     const string_view &in)
try
{
	//TODO: XXX
	assert(0);
	return in;
}
catch(const qi::expectation_failure<const char *> &e)
{
	throw expectation_failure(begin(in), e);
}

bool
ircd::json::valid(const string_view &s,
                  std::nothrow_t)
noexcept try
{
	const char *start(begin(s)), *const stop(end(s));
	return qi::parse(start, stop, parser.value(0) >> eoi);
}
catch(...)
{
	return false;
}

void
ircd::json::valid(const string_view &s)
try
{
	const char *start(begin(s)), *const stop(end(s));
	qi::parse(start, stop, eps > (parser.value(0) >> eoi));
}
catch(const qi::expectation_failure<const char *> &e)
{
	throw expectation_failure(begin(s), e);
}

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const string_view &v)
{
	if(v.empty() && defined(v))
	{
		consume(buf, copy(buf, empty_string));
		return empty_string;
	}

	consume(buf, copy(buf, string_view{v}));
	return string_view{v};
}

size_t
ircd::json::serialized(const string_view &s)
{
	size_t ret
	{
		s.size()
	};

	switch(type(s, std::nothrow))
	{
		case NUMBER:
		case OBJECT:
		case ARRAY:
		case LITERAL:
			break;

		case STRING:
			ret += !startswith(s, '"');
			ret += !endswith(s, '"');
			break;
	}

	return ret;
}

enum ircd::json::type
ircd::json::type(const string_view &buf)
{
	static const auto flag(qi::skip_flag::dont_postskip);

	enum type ret;
	if(!qi::phrase_parse(begin(buf), end(buf), parser.type, parser.WS, flag, ret))
		throw type_error("Failed to get type from buffer");

	return ret;
}

enum ircd::json::type
ircd::json::type(const string_view &buf,
                 std::nothrow_t)
{
	static const auto flag(qi::skip_flag::dont_postskip);

	enum type ret;
	if(!qi::phrase_parse(begin(buf), end(buf), parser.type, parser.WS, flag, ret))
		return STRING;

	return ret;
}

ircd::string_view
ircd::json::reflect(const enum type &type)
{
	switch(type)
	{
		case NUMBER:   return "NUMBER";
		case OBJECT:   return "OBJECT";
		case ARRAY:    return "ARRAY";
		case LITERAL:  return "LITERAL";
		case STRING:   return "STRING";
	}

	return {};
}
