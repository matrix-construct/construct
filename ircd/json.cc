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

BOOST_FUSION_ADAPT_STRUCT
(
    ircd::json::doc::member,
    ( decltype(ircd::json::doc::member::first),   first )
    ( decltype(ircd::json::doc::member::second),  second )
)

namespace ircd {
namespace json {

namespace spirit = boost::spirit;
namespace ascii = spirit::ascii;
namespace karma = spirit::karma;
namespace qi = spirit::qi;

using spirit::unused_type;

using qi::lit;
using qi::int_;
using qi::char_;
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
using karma::int_;
using karma::double_;
using karma::bool_;
using karma::maxwidth;
using karma::buffer;
using karma::eps;
using karma::attr_cast;

template<class it>
struct input
:qi::grammar<it, unused_type>
{
	template<class T = unused_type> using rule = qi::rule<it, T>;

	// insignificant whitespaces
	rule<> SP                          { lit('\x20')                                     ,"space" };
	rule<> HT                          { lit('\x09')                            ,"horizontal tab" };
	rule<> CR                          { lit('\x0D')                           ,"carriage return" };
	rule<> LF                          { lit('\x0A')                                 ,"line feed" };

	// whitespace skipping
	rule<> WS                          { SP | HT | CR | LF                          ,"whitespace" };
	rule<> ws                          { *(WS)                               ,"whitespace monoid" };
	rule<> wsp                         { +(WS)                            ,"whitespace semigroup" };

	// structural 
	rule<> object_begin                { lit('{')                                 ,"object begin" };
	rule<> object_end                  { lit('}')                                   ,"object end" };
	rule<> array_begin                 { lit('[')                                  ,"array begin" };
	rule<> array_end                   { lit(']')                                    ,"array end" };
	rule<> name_sep                    { lit(':')                                     ,"name sep" };
	rule<> value_sep                   { lit(',')                                    ,"value sep" };

	// literal
	rule<string_view> lit_true         { lit("true")                              ,"literal true" };
	rule<string_view> lit_false        { lit("false")                            ,"literal false" };
	rule<string_view> lit_null         { lit("null")                              ,"literal null" };

	rule<> quote                       { lit("\"")                                       ,"quote" };
	rule<string_view> chars            { raw[*(char_ - quote)]                      ,"characters" };
	rule<string_view> string           { quote >> chars >> quote                        ,"string" };
	rule<string_view> name             { string                                           ,"name" };

	rule<string_view> boolean          { lit_true | lit_false                          ,"boolean" };
	rule<string_view> literal          { lit_true | lit_false | lit_null               ,"literal" };
	rule<string_view> number           { raw[double_]                                   ,"number" };

	rule<string_view> array
	{
		array_begin >> -(omit[ws >> value >> ws] % value_sep) >> ws >> array_end
		,"array"
	};

	rule<string_view> object
	{
		object_begin >> -(omit[ws >> member >> ws] % value_sep) >> ws >> object_end
		,"object"
	};

	rule<string_view> value
	{
		lit_false | lit_true | lit_null | object | array | number | string
		,"value"
	};

	rule<doc::member> member
	{
		name >> ws >> name_sep >> ws >> value
		,"member"
	};

	rule<int> type
	{
		(omit[object_begin]    >> attr(json::OBJECT))   |
		(omit[array_begin]     >> attr(json::ARRAY))    |
		(omit[quote]           >> attr(json::STRING))   |
		(omit[number]          >> attr(json::NUMBER))   |
		(omit[literal]         >> attr(json::LITERAL))
		,"type"
	};

	input()
	:input::base_type{rule<>{}}
	{
		array %= array_begin >> -(omit[ws >> value >> ws] % value_sep) >> ws >> array_end;
		object %= object_begin >> -(omit[ws >> member >> ws] % value_sep) >> ws >> object_end;
	}
};

template<class it>
struct output
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
	rule<> array_begin                 { lit('[')                                  ,"array begin"  };
	rule<> array_end                   { lit(']')                                    ,"array end"  };
	rule<> name_sep                    { lit(':')                                ,"name separator" };
	rule<> value_sep                   { lit(',')                               ,"value separator" };
	rule<> quote                       { lit('"')                                         ,"quote" };

	rule<string_view> lit_true         { karma::string("true")                     ,"literal true" };
	rule<string_view> lit_false        { karma::string("false")                   ,"literal false" };
	rule<string_view> lit_null         { karma::string("null")                     ,"literal null" };

	rule<string_view> boolean          { lit_true | lit_false                           ,"boolean" };
	rule<string_view> literal          { lit_true | lit_false | lit_null                ,"literal" };

	rule<string_view> chars            { *(~char_("\""))                             ,"characters" };
	rule<string_view> string           { quote << chars << quote                         ,"string" };

	rule<string_view> number           { double_                                         ,"number" };

	rule<string_view> name             { string                                            ,"name" };
	rule<string_view> value            { rule<string_view>{}                              ,"value" };

	rule<const json::arr &> elems      { (value % value_sep)                           ,"elements" };
	rule<const json::arr &> array      { array_begin << elems << array_end                ,"array" };

	rule<doc::member> member           { name << name_sep << value                       ,"member" };
	rule<const json::doc &> members    { (member % value_sep)                           ,"members" };
	rule<const json::doc &> document   { object_begin << members << object_end         ,"document" };

	output()
	:output::base_type{rule<>{}}
	{}
};

} // namespace json
} // namespace ircd

namespace ircd {
namespace json {

struct parser
:input<const char *>
{
	using input<const char *>::input;
}
const parser;

struct printer
:output<char *>
{
	printer();
}
const printer;

struct ostreamer
:output<karma::ostream_iterator<char>>
{
	ostreamer();
}
const ostreamer;

size_t print(char *const &buf, const size_t &max, const arr &);
size_t print(char *const &buf, const size_t &max, const doc &);
size_t print(char *const &buf, const size_t &max, const obj &);
doc serialize(const obj &, char *&start, char *const &stop);

std::ostream &operator<<(std::ostream &, const arr &);
std::ostream &operator<<(std::ostream &, const doc::member &);
std::ostream &operator<<(std::ostream &, const doc &);
std::ostream &operator<<(std::ostream &, const obj &);

} // namespace json
} // namespace ircd

ircd::json::printer::printer()
{
	const auto recursor([this](auto &a, auto &b, auto &c)
	{
		const auto recurse_array([&]
		{
			char *out(const_cast<char *>(a.data()));
			karma::generate(out, maxwidth(a.size())[array], json::arr(a));
			a.resize(size_t(out - a.data()));
		});

		const auto recurse_document([&]
		{
			char *out(const_cast<char *>(a.data()));
			karma::generate(out, maxwidth(a.size())[document], json::doc(a));
			a.resize(size_t(out - a.data()));
		});

		const auto quote_string([&]
		{
			a.insert(a.end(), '"');
			a.insert(a.begin(), '"');
		});

		if(likely(!a.empty())) switch(a.front())
		{
			case '{':  recurse_document();  break;
			case '[':  recurse_array();     break;
			case '"':                       break;
			case '0':                       break;
			case '1':                       break;
			case '2':                       break;
			case '3':                       break;
			case '4':                       break;
			case '5':                       break;
			case '6':                       break;
			case '7':                       break;
			case '8':                       break;
			case '9':                       break;
			case 't':
			case 'f':
			case 'n':
				if(a == "true" || a == "false" || a == "null")
					break;

			default:
				quote_string();
				break;
		}
	});

	value %= karma::string[recursor];
}

ircd::json::ostreamer::ostreamer()
{
	const auto recursor([this](auto &a, auto &b, auto &c)
	{
		const auto recurse_array([&]
		{
			char *out(const_cast<char *>(a.data()));
			const auto count(print(out, a.size() + 1, json::arr(a)));
			a.resize(count);
		});

		const auto recurse_document([&]
		{
			char *out(const_cast<char *>(a.data()));
			const auto count(print(out, a.size() + 1, json::doc(a)));
			a.resize(count);
		});

		const auto quote_string([&]
		{
			a.insert(a.end(), '"');
			a.insert(a.begin(), '"');
		});

		if(likely(!a.empty())) switch(a.front())
		{
			case '{':  recurse_document();  break;
			case '[':  recurse_array();     break;
			case '"':                       break;
			case '0':                       break;
			case '1':                       break;
			case '2':                       break;
			case '3':                       break;
			case '4':                       break;
			case '5':                       break;
			case '6':                       break;
			case '7':                       break;
			case '8':                       break;
			case '9':                       break;
			case 't':
			case 'f':
			case 'n':
				if(a == "true" || a == "false" || a == "null")
					break;

			default:
				quote_string();
				break;
		}
	});

	value %= karma::string[recursor];
}

size_t
ircd::json::print(char *const &buf,
                  const size_t &max,
                  const obj &obj)
{
	if(unlikely(!max))
		return 0;

	char *out(buf);
	serialize(obj, out, out + (max - 1));
	*out = '\0';
	return out - buf;
}

ircd::json::doc
ircd::json::serialize(const obj &obj,
                      char *&out,
                      char *const &stop)
{
	static const auto throws([]
	{
		throw print_error("The JSON generator failed to serialize object");
	});

	const auto print_string([&stop, &out](const val &val)
	{
		karma::generate(out, maxwidth(stop - out)[printer.string] | eps[throws], val);
	});

	const auto print_object([&stop, &out](const val &val)
	{
		if(val.serial)
		{
			karma::generate(out, maxwidth(stop - out)[printer.document] | eps[throws], val);
			return;
		}

		assert(val.object);
		serialize(*val.object, out, stop);
	});

	const auto print_array([&stop, &out](const val &val)
	{
		if(val.serial)
		{
			karma::generate(out, maxwidth(stop - out)[printer.array] | eps[throws], val);
			return;
		}

		//assert(val.object);
		//serialize(*val.object, out, stop);
	});

	const auto print_member([&](const obj::member &member)
	{
		const auto generate_name
		{
			maxwidth(stop - out)[printer.name << printer.name_sep] | eps[throws]
		};

		karma::generate(out, generate_name, member.first);

		switch(member.second.type)
		{
			case STRING:   print_string(member.second);   break;
			case OBJECT:   print_object(member.second);   break;
			case ARRAY:    print_array(member.second);    break;
			default:
				throw type_error("Cannot stream unsupported member type");
		}
	});

	char *const start(out);
	karma::generate(out, maxwidth(stop - out)[printer.object_begin] | eps[throws]);

	auto it(begin(obj));
	if(it != end(obj))
	{
		print_member(*it);
		for(++it; it != end(obj); ++it)
		{
			karma::generate(out, maxwidth(stop - out)[printer.value_sep] | eps[throws]);
			print_member(*it);
		}
	}

	karma::generate(out, maxwidth(stop - out)[printer.object_end] | eps[throws]);
	return string_view{start, out};
}

std::ostream &
ircd::json::operator<<(std::ostream &s, const obj &obj)
{
	karma::ostream_iterator<char> osi(s);

	static const auto throws([]
	{
		throw print_error("The JSON generator failed to output object to stream");
	});

	const auto stream_string([&osi](const val &val)
	{
		karma::generate(osi, ostreamer.string, val);
	});

	const auto stream_object([&osi, &s](const val &val)
	{
		if(val.serial)
		{
			karma::generate(osi, ostreamer.document, val);
			return;
		}

		assert(val.object);
		s << *val.object;
	});

	const auto stream_array([&osi, &s](const val &val)
	{
		if(val.serial)
		{
			karma::generate(osi, ostreamer.array, val);
			return;
		}

		assert(0);
		//assert(val.object);
		//s << *val.object;
	});

	const auto stream_member([&](const obj::member &member)
	{
		karma::generate(osi, ostreamer.name << ostreamer.name_sep, string_view(member.first));

		switch(member.second.type)
		{
			case STRING:  stream_string(member.second);  break;
			case OBJECT:  stream_object(member.second);  break;
			case ARRAY:   stream_array(member.second);   break;
			default:
				throw type_error("cannot stream unsupported member type");
		}
	});

	karma::generate(osi, ostreamer.object_begin);

	auto it(begin(obj));
	if(it != end(obj))
	{
		stream_member(*it);
		for(++it; it != end(obj); ++it)
		{
			karma::generate(osi, ostreamer.value_sep);
			stream_member(*it);
		}
	}

	karma::generate(osi, ostreamer.object_end);
	return s;
}

ircd::json::obj::obj(std::initializer_list<member> builder)
{
	std::transform(std::begin(builder), std::end(builder), std::back_inserter(idx), []
	(auto&& m)
	{
		return std::move(const_cast<member &>(m));
	});
}

ircd::json::obj::obj(const doc &doc)
{
	std::transform(std::begin(doc), std::end(doc), std::back_inserter(idx), []
	(const doc::member &m) -> obj::member
	{
		return { val { m.first }, val { m.second, type(m.second), true } };
	});
}

bool
ircd::json::obj::erase(const string_view &name)
{
	const auto it(find(name));
	if(it == end())
		return false;

	erase(it);
	return true;
}

void
ircd::json::obj::erase(const const_iterator &it)
{
	idx.erase(it);
}

ircd::json::obj::const_iterator
ircd::json::obj::erase(const const_iterator &start,
                       const const_iterator &stop)
{
	return { idx.erase(start, stop) };
}

size_t
ircd::json::obj::size()
const
{
	const size_t ret(1 + idx.empty());
	return std::accumulate(std::begin(idx), std::end(idx), ret, [this]
	(auto ret, const auto &member)
	{
		return ret += member.first.size() + 1 + 1 + member.second.size() + 1;
	});
}

ircd::json::obj::operator std::string()
const
{
	std::string ret(size(), char());
	ret.resize(print(const_cast<char *>(ret.data()), ret.size() + 1, *this));
	return ret;
}

std::ostream &
ircd::json::operator<<(std::ostream &s, const val &v)
{
	switch(v.type)
	{
		case STRING:
			s << string_view(v);
			break;

		case OBJECT:
			if(v.serial)
				s << string_view(v);
			else
				s << *v.object;
			break;

		case ARRAY:
			if(v.serial)
				s << string_view(v);
			else
				assert(0);
			break;

		default:
			throw type_error("cannot stream value type[%d]", int(v.type));
	}

	return s;
}

inline
ircd::json::val::~val()
noexcept
{
	switch(type)
	{
		case STRING:  if(alloc)  delete[] string;  break;
		case OBJECT:  if(alloc)  delete object;    break;
		//case ARRAY:   if(alloc)  delete array;     break;
		default:                                   break;
	}
}

ircd::json::val::operator std::string()
const
{
	switch(type)
	{
		case STRING:
			return std::string(unquote(string_view(*this)));

		case OBJECT:
			if(serial)
				return std::string(string_view(*this));
			else
				return std::string(*object);

		case ARRAY:
			if(serial)
				return std::string(string_view(*this));
			else
				break;

		default:
			break;
	}

	throw type_error("cannot stringify type[%d]", int(type));
}

ircd::json::val::operator string_view()
const
{
	switch(type)
	{
		case STRING:
			return unquote(string_view{string, len});

		case ARRAY:
		case OBJECT:
			if(serial)
				return string_view{string, len};
			else
				break;

		default:
			break;
	}

	throw type_error("value type[%d] is not a string", int(type));
}

size_t
ircd::json::val::size()
const
{
	switch(type)
	{
		case NUMBER:  return lex_cast(integer).size();
		case STRING:  return 1 + len + 1;
		case OBJECT:  return serial? len : object->size();
		case ARRAY:   return serial? len : 2;
		default:      break;
	};

	throw type_error("cannot size type[%u]", int(type));
}

size_t
ircd::json::print(char *const &buf,
                  const size_t &max,
                  const doc &doc)
{
	if(unlikely(!max))
		return 0;

	char *out(buf);
	serialize(doc, out, out + (max - 1));
	*out = '\0';
	return std::distance(buf, out);
}

ircd::json::doc
ircd::json::serialize(const doc &doc,
                      char *&out,
                      char *const &stop)
{
	static const auto throws([]
	{
		throw print_error("The JSON generator failed to print document");
	});

	char *const start(out);
	karma::generate(out, maxwidth(stop - start)[printer.document] | eps[throws], doc);
	return string_view{start, out};
}

std::ostream &
ircd::json::operator<<(std::ostream &s, const doc &doc)
{
	const auto &os(ostreamer);
	static const auto throws([]
	{
		throw print_error("The JSON generator failed to output document to stream");
	});

	karma::ostream_iterator<char> osi(s);
	karma::generate(osi, os.document | eps[throws], doc);
	return s;
}

std::ostream &
ircd::json::operator<<(std::ostream &s, const doc::member &member)
{
	static const auto throws([]
	{
		throw print_error("The JSON generator failed to output document member to stream");
	});

	karma::ostream_iterator<char> osi(s);
	karma::generate(osi, ostreamer.member | eps[throws], member);
	return s;
}

ircd::json::doc::const_iterator &
ircd::json::doc::const_iterator::operator++()
{
	static const qi::rule<const char *, json::doc::member> member
	{
		parser.name >> parser.ws >> parser.name_sep >> parser.ws >> raw[parser.value]
	};

	static const qi::rule<const char *, json::doc::member> parse_next
	{
		parser.object_end | (parser.value_sep >> parser.ws >> member)
	};

	state.first = string_view{};
	state.second = string_view{};
	if(!qi::phrase_parse(start, stop, parse_next, parser.WS, state))
		start = stop;

	return *this;
}

ircd::json::doc::const_iterator
ircd::json::doc::begin()
const
{
	static const qi::rule<const char *, json::doc::member> member
	{
		parser.name >> parser.ws >> parser.name_sep >> parser.ws >> raw[parser.value]
	};

	static const qi::rule<const char *, json::doc::member> parse_begin
	{
		parser.object_begin >> parser.ws >> (parser.object_end | member)
	};

	const_iterator ret(string_view::begin(), string_view::end());
	if(!qi::phrase_parse(ret.start, ret.stop, parse_begin, parser.WS, ret.state))
		ret.start = ret.stop;

	return ret;
}

ircd::json::doc::const_iterator
ircd::json::doc::end()
const
{
	return { string_view::end(), string_view::end() };
}

size_t
ircd::json::print(char *const &buf,
                  const size_t &max,
                  const arr &arr)
{
	if(unlikely(!max))
		return 0;

	char *out(buf);
	serialize(arr, out, out + (max - 1));
	*out = '\0';
	return std::distance(buf, out);
}

ircd::json::arr
ircd::json::serialize(const arr &a,
                      char *&out,
                      char *const &stop)
{
	static const auto throws([]
	{
		throw print_error("The JSON generator failed to print array");
	});

	const karma::rule<char *, const json::arr &> grammar
	{
		printer.array_begin << (printer.value % printer.value_sep) << printer.array_end
	};

	char *const start(out);
	karma::generate(out, maxwidth(stop - start)[grammar] | eps[throws], a);
	return string_view{start, out};
}

std::ostream &
ircd::json::operator<<(std::ostream &s, const arr &arr)
{
	const auto &os(ostreamer);
	static const auto throws([]
	{
		throw print_error("The JSON generator failed to output array to stream");
	});

	karma::ostream_iterator<char> osi(s);
	karma::generate(osi, os.array | eps[throws], arr);
	return s;
}

ircd::json::arr::const_iterator &
ircd::json::arr::const_iterator::operator++()
{
	static const qi::rule<const char *, string_view> parse_next
	{
		parser.array_end | (parser.value_sep >> parser.ws >> raw[parser.value])
	};

	state = string_view{};
	if(!qi::phrase_parse(start, stop, parse_next, parser.WS, state))
		start = stop;

	return *this;
}

ircd::json::arr::const_iterator
ircd::json::arr::begin()
const
{
	static const qi::rule<const char *, string_view> parse_begin
	{
		parser.array_begin >> parser.ws >> (parser.array_end | raw[parser.value])
	};

	const_iterator ret(string_view::begin(), string_view::end());
	if(!qi::phrase_parse(ret.start, ret.stop, parse_begin, parser.WS, ret.state))
		ret.start = ret.stop;

	return ret;
}

ircd::json::arr::const_iterator
ircd::json::arr::end()
const
{
	return { string_view::end(), string_view::end() };
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
