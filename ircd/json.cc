/*
 * charybdis: standing on the shoulders of giant build times
 *
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
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

#include <ircd/spirit.h>
#include <boost/fusion/include/at.hpp>

BOOST_FUSION_ADAPT_STRUCT
(
    ircd::json::object::member,
    ( decltype(ircd::json::object::member::first),   first )
    ( decltype(ircd::json::object::member::second),  second )
)

BOOST_FUSION_ADAPT_STRUCT
(
    ircd::json::member,
    ( decltype(ircd::json::member::first),   first )
    ( decltype(ircd::json::member::second),  second )
)

namespace ircd::json
{
	namespace spirit = boost::spirit;
	namespace ascii = spirit::ascii;
	namespace karma = spirit::karma;
	namespace qi = spirit::qi;

	using spirit::unused_type;

	using qi::lit;
	using qi::char_;
	using qi::long_;
	using qi::double_;
	using qi::raw;
	using qi::omit;
	using qi::matches;
	using qi::hold;
	using qi::eoi;
	using qi::eps;
	using qi::attr;

	using karma::lit;
	using karma::char_;
	using karma::long_;
	using karma::double_;
	using karma::bool_;
	using karma::maxwidth;
	using karma::buffer;
	using karma::eps;
	using karma::attr_cast;

	template<class it> struct input;
	template<class it> struct output;

	// Instantiations of the grammars
	struct parser extern const parser;
	struct printer extern const printer;
	struct ostreamer extern const ostreamer;
}

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

	// literal
	rule<string_view> lit_false        { lit("false")                             ,"literal false" };
	rule<string_view> lit_true         { lit("true")                               ,"literal true" };
	rule<string_view> lit_null         { lit("null")                                       ,"null" };

	rule<> quote                       { lit('"')                                         ,"quote" };
	rule<string_view> chars            { raw[*(char_ - quote)]                       ,"characters" };
	rule<string_view> string           { quote >> chars >> quote                         ,"string" };
	rule<string_view> name             { quote >> raw[+(char_ - quote)] >> quote           ,"name" };

	rule<string_view> boolean          { lit_true | lit_false                           ,"boolean" };
	rule<string_view> literal          { lit_true | lit_false | lit_null                ,"literal" };
	rule<string_view> number           { raw[double_]                                    ,"number" };

	rule<json::object::member> member
	{
		name >> name_sep >> value
		,"member"
	};

	rule<string_view> object
	{
		raw[object_begin >> -(member % value_sep) >> object_end]
		,"object"
	};

	rule<string_view> array
	{
		raw[array_begin >> -(value % value_sep) >> array_end]
		,"array"
	};

	rule<string_view> value
	{
		raw[lit_false | lit_null | lit_true | object | array | number | string]
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
	{
		member %= name >> name_sep >> value;
		array %= raw[array_begin >> -(value % value_sep) >> array_end];
		object %= raw[object_begin >> -(member % value_sep) >> object_end];
	}
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

	rule<string_view> lit_true         { karma::string("true")                     ,"literal true" };
	rule<string_view> lit_false        { karma::string("false")                   ,"literal false" };
	rule<string_view> lit_null         { karma::string("null")                     ,"literal null" };
	rule<string_view> boolean          { lit_true | lit_false                           ,"boolean" };
	rule<string_view> literal          { lit_true | lit_false | lit_null                ,"literal" };
	rule<string_view> chars            { *(~char_('"'))                              ,"characters" };
	rule<string_view> string           { quote << chars << quote                         ,"string" };
	rule<string_view> number           { double_                                         ,"number" };
	rule<string_view> name             { quote << +(~char_('"')) << quote                  ,"name" };

	output()
	:output::base_type{rule<>{}}
	{}
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
	template<class generator,
	         class attribute>
	bool operator()(char *&out, char *const &stop, generator&& gen, attribute&& a) const;

	template<class generator>
	bool operator()(char *&out, char *const &stop, generator&& gen) const;

	template<class... args>
	bool operator()(mutable_buffer &out, args&&... a) const
	{
		return operator()(begin(out), end(out), std::forward<args>(a)...);
	}
}
const ircd::json::printer;

struct ircd::json::ostreamer
:output<karma::ostream_iterator<char>>
{
}
const ircd::json::ostreamer;

template<class gen,
         class attr>
bool
ircd::json::printer::operator()(char *&out,
                                char *const &stop,
                                gen&& g,
                                attr&& a)
const
{
	const auto throws([&out, &stop]
	{
		throw print_error("Failed to print attribute '%s' generator '%s' (%zd bytes in buffer)",
		                  demangle<decltype(a)>(),
		                  demangle<decltype(g)>(),
		                  size_t(stop - out));
	});

	const auto gg
	{
		maxwidth(size_t(stop - out))[std::forward<gen>(g)] | eps[throws]
	};

	return karma::generate(out, gg, std::forward<attr>(a));
}

template<class gen>
bool
ircd::json::printer::operator()(char *&out,
                                char *const &stop,
                                gen&& g)
const
{
	const auto throws([&out, &stop]
	{
		throw print_error("Failed to print generator '%s' (%zd bytes in buffer)",
		                  demangle<decltype(g)>(),
		                  size_t(stop - out));
	});

	const auto gg
	{
		maxwidth(size_t(stop - out))[std::forward<gen>(g)] | eps[throws]
	};

	return karma::generate(out, gg);
}

///////////////////////////////////////////////////////////////////////////////
//
// iov.h
//

std::ostream &
ircd::json::operator<<(std::ostream &s, const iov &iov)
{
	s << json::string(iov);
	return s;
}

ircd::string_view
ircd::json::stringify(mutable_buffer &head,
                      const iov &iov)
{
	const auto num{iov.size()};
	const member *m[num];

	size_t i(0);
	std::for_each(std::begin(iov), std::end(iov), [&i, &m]
	(const auto &member)
	{
		m[i++] = &member;
	});

	return stringify(head, m, m + num);
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
	const auto it(std::find_if(std::begin(*this), std::end(*this), [&key]
	(const auto &member)
	{
		return string_view{member.first} == key;
	}));

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
                      const member *const &begin,
                      const member *const &end)
{
	const auto num(std::distance(begin, end));
	const member *vec[num];
	for(auto i(0); i < num; ++i)
		vec[i] = begin + i;

	return stringify(buf, vec, vec + num);
}

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const member *const *const &b,
                      const member *const *const &e)
{
	const auto &print_member
	{
		[&](const member &member)
		{
			printer(buf, printer.name << printer.name_sep, member.first);
			stringify(buf, member.second);
		}
	};

	char *const start(begin(buf));
	printer(buf, printer.object_begin);

	auto it(b);
	if(it != e)
	{
		print_member(**it);
		for(++it; it != e; ++it)
		{
			printer(buf, printer.value_sep);
			print_member(**it);
		}
	}

	printer(buf, printer.object_end);
	return string_view{start, begin(buf)};
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

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const object &object)
{
	const char *const start(begin(buf));
	consume(buf, copy(buf, string_view{object}));
	return { start, begin(buf) };
}

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const object::member &member)
{
	static const auto throws([]
	{
		throw print_error("The JSON generator failed to stringify object::member");
	});

	const char *const start(begin(buf));
	consume(buf, copy(buf, string_view{member.first}));
	printer(buf, printer.name_sep | eps[throws]);
	consume(buf, copy(buf, string_view{member.second}));
	return string_view{start, begin(buf)};
}

std::ostream &
ircd::json::operator<<(std::ostream &s, const object &object)
{
	s << json::string(object);
	return s;
}

std::ostream &
ircd::json::operator<<(std::ostream &s, const object::member &member)
{
	s << json::string(member);
	return s;
}

ircd::json::object::const_iterator &
ircd::json::object::const_iterator::operator++()
try
{
	static const qi::rule<const char *, json::object::member> member
	{
		parser.name >> parser.name_sep >> parser.value
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
	const auto rule(ircd::string(e.what_));
	const long size(std::distance(e.first, e.last));
	throw parse_error("Expected JSON %s. You input %zu invalid characters starting with `%s`.",
	                  between(rule, "<", ">"),
	                  size,
	                  string_view(e.first, e.first + std::min(size, 64L)));
}

ircd::json::object::operator std::string()
const
{
	return json::string(*this);
}

ircd::json::object::const_iterator
ircd::json::object::begin()
const try
{
	static const qi::rule<const char *, json::object::member> object_member
	{
		parser.name >> parser.name_sep >> parser.value
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

	if(!empty())
		qi::parse(ret.start, ret.stop, eps > parse_begin, ret.state);

	return ret;
}
catch(const qi::expectation_failure<const char *> &e)
{
	const auto rule(ircd::string(e.what_));
	const long size(std::distance(e.first, e.last));
	throw parse_error("Expected JSON %s. You input %zu invalid characters starting with `%s`.",
	                  between(rule, "<", ">"),
	                  size,
	                  string_view(e.first, e.first + std::min(size, 64L)));
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

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const array &v)
{
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

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const string_view *const &b,
                      const string_view *const &e)
{
	return array::stringify(buf, b, e);
}

template<class it>
ircd::string_view
ircd::json::array::stringify(mutable_buffer &buf,
                             const it &b,
                             const it &e)
{
	const auto print_element
	{
		[&buf](const string_view &element)
		{
			if(!consume(buf, buffer::copy(buf, element)))
				throw print_error("The JSON generator ran out of space in supplied buffer");
		}
	};

	char *const start(std::begin(buf));
	printer(buf, printer.array_begin);

	auto i(b);
	if(i != e)
	{
		print_element(*i);
		for(++i; i != e; ++i)
		{
			printer(buf, printer.value_sep);
			print_element(*i);
		}
	}

	printer(buf, printer.array_end);
	return string_view{start, std::begin(buf)};
}

std::ostream &
ircd::json::operator<<(std::ostream &s, const array &a)
{
	s << json::string(a);
	return s;
}

ircd::json::array::const_iterator &
ircd::json::array::const_iterator::operator++()
try
{
	static const qi::rule<const char *, string_view> parse_next
	{
		parser.array_end | (parser.value_sep >> parser.value)
		,"next array element or end"
	};

	state = string_view{};
	qi::parse(start, stop, eps > parse_next, state);
	return *this;
}
catch(const qi::expectation_failure<const char *> &e)
{
	const auto rule(ircd::string(e.what_));
	const long size(std::distance(e.first, e.last));
	throw parse_error("Expected JSON %s. You input %zd invalid characters starting with `%s`.",
	                  between(rule, "<", ">"),
	                  size,
	                  string_view(e.first, e.first + std::min(size, 64L)));
}

ircd::json::array::operator std::string()
const
{
	return json::string(*this);
}

ircd::json::array::const_iterator
ircd::json::array::begin()
const try
{
	static const qi::rule<const char *, string_view> parse_begin
	{
		parser.array_begin >> (parser.array_end | parser.value)
		,"array begin and element or end"
	};

	const_iterator ret
	{
		string_view::begin(), string_view::end()
	};

	if(!empty())
		qi::parse(ret.start, ret.stop, eps > parse_begin, ret.state);

	return ret;
}
catch(const qi::expectation_failure<const char *> &e)
{
	const auto rule(ircd::string(e.what_));
	const long size(std::distance(e.first, e.last));
	throw parse_error("Expected JSON %s. You input %zd invalid characters starting with `%s`.",
	                  between(rule, "<", ">"),
	                  size,
	                  string_view(e.first, e.first + std::min(size, 64L)));
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

const ircd::string_view ircd::json::value::literal_null {"null"};
const ircd::string_view ircd::json::value::literal_true {"true"};
const ircd::string_view ircd::json::value::literal_false {"false"};
const ircd::string_view ircd::json::value::empty_string {"\"\""};
const ircd::string_view ircd::json::value::empty_number {"0"};
const ircd::string_view ircd::json::value::empty_object {"{}"};
const ircd::string_view ircd::json::value::empty_array {"[]"};

std::ostream &
ircd::json::operator<<(std::ostream &s, const value &v)
{
	s << json::string(v);
	return s;
}

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const value *const &b,
                      const value *const &e)
{
	static const auto throws([]
	{
		throw print_error("The JSON generator failed to print array values");
	});

	char *const start(begin(buf));
	printer(buf, printer.array_begin);

	auto it(b);
	if(it != e)
	{
		stringify(buf, *it);
		for(++it; it != e; ++it)
		{
			printer(buf, printer.value_sep);
			stringify(buf, *it);
		}
	}

	printer(buf, printer.array_end);
	return string_view{start, begin(buf)};
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

			consume(buf, copy(buf, v.literal_null));
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

			consume(buf, copy(buf, v.literal_null));
			break;
		}

		case NUMBER:
		{
			if(v.serial)
			{
				if(v.floats)
					printer(buf, double_, string_view{v});
				else
					printer(buf, long_, string_view{v});

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
			return v.len;

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
	(mutable_buffer buffer)
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
	if(alloc && serial)
	{
		create_string(len, [this](mutable_buffer buffer)
		{
			copy(buffer, string_view{*this});
		});
	}
	else switch(type)
	{
		case OBJECT:
			if(!serial && object)
			{
				const size_t count(this->len);
				create_string(serialized(object, object + count), [this, &count]
				(mutable_buffer buffer)
				{
					json::stringify(buffer, object, object + count);
				});
			}

			break;

		case ARRAY:
			if(!serial && array)
			{
				const size_t count(this->len);
				create_string(serialized(array, array + count), [this, &count]
				(mutable_buffer buffer)
				{
					json::stringify(buffer, array, array + count);
				});
			}
			break;

		case STRING:
			if(!serial && alloc && string)
			{
				create_string(serialized(*this), [this]
				(mutable_buffer buffer)
				{
					json::stringify(buffer, *this);
				});
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
	return json::string(*this);
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
			return !string || !len || string_view{*this} == empty_string;

		case OBJECT:
			return serial? !len || string_view{*this} == empty_object:
			       object? !len:
			               true;

		case ARRAY:
			return serial? !len || string_view{*this} == empty_array:
			       array?  false:
			               true;            //TODO: XXX arr

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
			       array?  array == nullptr:
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

	const mutable_buffer buffer
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

size_t
ircd::json::serialized(const string_view &s)
{
	if(!s.empty()) switch(type(s, std::nothrow))
	{
		case NUMBER:
		case OBJECT:
		case ARRAY:
		case LITERAL:
			return s.size();

		case STRING:
			break;
	}

	size_t ret(s.size());
	ret += !startswith(s, '"');
	ret += !endswith(s, '"');
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
