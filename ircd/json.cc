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
    ircd::json::object::member,
    ( decltype(ircd::json::object::member::first),   first )
    ( decltype(ircd::json::object::member::second),  second )
)

namespace ircd {
namespace json {

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

template<class it>
struct input
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
	rule<string_view> lit_true         { lit("true")                               ,"literal true" };
	rule<string_view> lit_false        { lit("false")                             ,"literal false" };
	rule<string_view> lit_null         { lit("null")                               ,"literal null" };

	rule<> quote                       { lit('"')                                         ,"quote" };
	rule<string_view> chars            { raw[*(char_ - quote)]                       ,"characters" };
	rule<string_view> string           { quote >> chars >> quote                         ,"string" };
	rule<string_view> name             { quote >> raw[+(char_ - quote)] >> quote           ,"name" };

	rule<string_view> boolean          { lit_true | lit_false                           ,"boolean" };
	rule<string_view> literal          { lit_true | lit_false | lit_null                ,"literal" };
	rule<string_view> number           { raw[double_]                                    ,"number" };

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

	rule<object::member> member
	{
		name >> ws >> name_sep >> ws >> value
		,"member"
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
	rule<string_view> chars            { *(~char_('"'))                              ,"characters" };
	rule<string_view> string           { quote << chars << quote                         ,"string" };
	rule<string_view> number           { double_                                         ,"number" };
	rule<string_view> name             { quote << +(~char_('"')) << quote                  ,"name" };
	rule<string_view> value            { rule<string_view>{}  /* subclass implemented */  ,"value" };

	rule<const string_view &> elem
	{
		value
		,"element"
	};

	rule<const json::array &> elems
	{
		-(value % value_sep)
		,"elements"
	};

	rule<const json::array &> array
	{
		array_begin << elems << array_end
		,"array"
	};

	rule<const json::object::member &> member
	{
		name << name_sep << value
		,"member"
	};

	rule<const json::object &> members
	{
		-(member % value_sep)
		,"members"
	};

	rule<const json::object &> object
	{
		object_begin << members << object_end
		,"object"
	};

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

	printer();
}
const printer;

struct ostreamer
:output<karma::ostream_iterator<char>>
{
	ostreamer();
}
const ostreamer;

} // namespace json
} // namespace ircd

namespace ircd {
namespace json {
namespace      {

void
failed_to_serialize_object()
{
	throw print_error("The JSON generator failed to serialize object");
}

void
failed_to_stream_object()
{
	throw print_error("The JSON generator failed to stream object");
}

} // namespace <anonymous>
} // namespace json
} // namespace ircd

namespace ircd {
namespace db   {

std::string
merge_operator(const string_view &key,
               const std::pair<string_view, string_view> &delta)
{
	ircd::json::index index{delta.first};
	index += delta.second;
	return index;
}

} // namespace db
} // namespace ircd

ircd::json::printer::printer()
{
	const auto recursor([this](auto &a, auto &b, auto &c)
	{
		const auto recurse_array([&a]
		{
			mutable_buffer mb
			{
				const_cast<char *>(a.data()), a.size()
			};

			const auto r(stringify(mb, json::array(a)));
			a.resize(r.size());
		});

		const auto recurse_object([&a]
		{
			mutable_buffer mb
			{
				const_cast<char *>(a.data()), a.size()
			};

			const auto r(stringify(mb, json::object(a)));
			a.resize(r.size());
		});

		const auto quote_string([&]
		{
			a.insert(a.end(), '"');
			a.insert(a.begin(), '"');
		});

		if(likely(!a.empty())) switch(a.front())
		{
			case '{':  recurse_object();    break;
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

template<class gen,
         class attr>
bool
ircd::json::printer::operator()(char *&out,
                                char *const &stop,
                                gen&& g,
                                attr&& a)
const
{
	const auto gg
	{
		maxwidth(size_t(stop - out))[std::forward<gen>(g)] | eps[failed_to_serialize_object]
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
	const auto gg
	{
		maxwidth(size_t(stop - out))[std::forward<gen>(g)] | eps[failed_to_serialize_object]
	};

	return karma::generate(out, gg);
}

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const std::vector<json::object> &v)
{
	const auto print_member([&buf](const string_view &elem)
	{
		//printer(buf, printer.elem, elem);
		const auto cpsz(std::min(size(buf), elem.size()));
		memcpy(begin(buf), elem.data(), cpsz);
		begin(buf) += cpsz;
	});

	char *const start(begin(buf));
	printer(buf, printer.array_begin);

	auto it(std::begin(v));
	if(it != std::end(v))
	{
		print_member(*it);
		for(++it; it != std::end(v); ++it)
		{
			printer(buf, printer.value_sep);
			print_member(*it);
		}
	}

	printer(buf, printer.array_end);
	return string_view{start, begin(buf)};
}

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const members &list)
{
	return stringify(buf, std::begin(list), std::end(list));
}

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const index &obj)
{
	const member *const &begin(&obj.idx.front());
	const member *const &end(begin + obj.idx.size());
	return stringify(buf, begin, end);
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
	const auto print_string([&buf](const value &value)
	{
		printer(buf, printer.string, string_view{value});
	});

	const auto print_literal([&buf](const value &value)
	{
		printer(buf, printer.literal, string_view{value});
	});

	const auto print_object([&buf](const value &value)
	{
		if(value.serial)
		{
			printer(buf, printer.object, string_view{value});
			return;
		}

		assert(value.object);
		stringify(buf, *value.object);
	});

	const auto print_array([&buf](const value &value)
	{
		if(value.serial)
		{
			//printer(out, stop, printer.array, value);
			const string_view data(value);
			const auto cpsz(std::min(size(buf), data.size()));
			memcpy(begin(buf), data.data(), cpsz);
			begin(buf) += cpsz;
			return;
		}

		assert(0);
		//assert(value.object);
		//serialize(*value.object, out, stop);
	});

	const auto print_number([&buf](const value &value)
	{
		if(value.serial)
		{
			if(value.floats)
				printer(buf, double_, string_view{value});
			else
				printer(buf, long_, string_view{value});

			return;
		}

		if(value.floats)
			printer(buf, double_, value.floating);
		else
			printer(buf, long_, value.integer);
	});

	const auto print_member([&](const member &member)
	{
		printer(buf, printer.name << printer.name_sep, member.first);

		switch(member.second.type)
		{
			case STRING:   print_string(member.second);   break;
			case OBJECT:   print_object(member.second);   break;
			case ARRAY:    print_array(member.second);    break;
			case NUMBER:   print_number(member.second);   break;
			case LITERAL:  print_literal(member.second);  break;
		}
	});

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

ircd::json::ostreamer::ostreamer()
{
	const auto recursor([this](auto &a, auto &b, auto &c)
	{
		const auto recurse_array([&a]
		{
			mutable_buffer mb
			{
				const_cast<char *>(a.data()), a.size()
			};

			const auto r(stringify(mb, json::array(a)));
			a.resize(r.size());
		});

		const auto recurse_object([&a]
		{
			mutable_buffer mb
			{
				const_cast<char *>(a.data()), a.size()
			};

			const auto r(stringify(mb, json::object(a)));
			a.resize(r.size());
		});

		const auto quote_string([&]
		{
			a.insert(a.end(), '"');
			a.insert(a.begin(), '"');
		});

		if(likely(!a.empty())) switch(a.front())
		{
			case '{':  recurse_object();    break;
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

std::ostream &
ircd::json::operator<<(std::ostream &s, const index &obj)
{
	karma::ostream_iterator<char> osi(s);

	const auto stream_string([&osi](const value &value)
	{
		karma::generate(osi, ostreamer.string, string_view{value});
	});

	const auto stream_literal([&osi](const value &value)
	{
		karma::generate(osi, ostreamer.literal, string_view{value});
	});

	const auto stream_object([&osi, &s](const value &value)
	{
		if(value.serial)
		{
			karma::generate(osi, ostreamer.object, string_view{value});
			return;
		}

		assert(value.object);
		s << *value.object;
	});

	const auto stream_array([&osi](const value &value)
	{
		if(value.serial)
		{
			karma::generate(osi, ostreamer.array, string_view{value});
			return;
		}

		assert(0);
		//assert(value.object);
		//s << *value.object;
	});

	const auto stream_number([&osi](const value &value)
	{
		if(value.serial)
		{
			if(value.floats)
				karma::generate(osi, double_, string_view{value});
			else
				karma::generate(osi, long_, string_view{value});

			return;
		}

		if(value.floats)
			karma::generate(osi, double_, value.floating);
		else
			karma::generate(osi, long_, value.integer);
	});

	const auto stream_member([&](const member &member)
	{
		karma::generate(osi, ostreamer.name << ostreamer.name_sep, string_view(member.first));

		switch(member.second.type)
		{
			case STRING:   stream_string(member.second);   break;
			case OBJECT:   stream_object(member.second);   break;
			case ARRAY:    stream_array(member.second);    break;
			case NUMBER:   stream_number(member.second);   break;
			case LITERAL:  stream_literal(member.second);  break;
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
	return serialized(member.first) + 1 + 1 + serialized(member.second);
}

ircd::json::index
ircd::json::operator+(const object &left, const object &right)
{
	index ret(left);
	ret += right;
	return ret;
}

ircd::json::index &
ircd::json::operator+=(index &left, const object &right)
{
	for(const auto &b : right)
	{
		const auto deletion
		{
			// empty replacement value indicates deletion
			b.second.empty()    ||
			b.second == "\"\""  ||
			b.second == "{}"    ||
			b.second == "[]"
		};

		const auto it(left.find(b.first));
		if(it == left.end())
		{
			if(deletion)   // prevents adding the empty indicator as the new key!
				continue;

			left.idx.emplace_back(member{b});
			continue;
		}

		if(deletion)
		{
			left.erase(it);
			continue;
		}

		auto &a(const_cast<member &>(*it));
		switch(type(a.second))
		{
			default:
			{
				a = member{b};
				continue;
			}

			// merge recursively
			case OBJECT:
			{
				auto merged(std::make_unique<index>(json::object{a.second}));
				*merged += json::object{b.second};
				if(merged->empty())
				{
					// Child merge was empty value. Now we can also remove this empty parent.
					left.erase(it);
					continue;
				}

				a = member{string_view{a.first}, std::move(merged)};
				continue;
			}
		}
	}

	return left;
}

ircd::json::index::index(recursive_t recursive,
                         const object &object)
:idx{object.count()}
{
	std::transform(std::begin(object), std::end(object), std::begin(idx), [&]
	(const object::member &m) -> member
	{
		switch(type(m.second))
		{
			case OBJECT:
				return member{m.first, std::make_unique<index>(recursive, m.second)};

			default:
				return member{m};
		};
	});
}

ircd::json::index::index(const object &object)
:idx{object.count()}
{
	std::transform(std::begin(object), std::end(object), std::begin(idx), []
	(const object::member &m) -> member
	{
		return member{m};
	});
}

ircd::json::index::index(std::initializer_list<member> builder)
:idx{builder.size()}
{
	std::transform(std::begin(builder), std::end(builder), std::begin(idx), []
	(auto&& m)
	{
		return std::move(const_cast<member &>(m));
	});

	const auto empty([](const auto &member)
	{
		return member.first.empty();
	});

	idx.erase(std::remove_if(std::begin(idx), std::end(idx), empty), idx.end());
}

bool
ircd::json::index::erase(const string_view &name)
{
	const auto it(find(name));
	if(it == end())
		return false;

	erase(it);
	return true;
}

void
ircd::json::index::erase(const const_iterator &it)
{
	idx.erase(it);
}

ircd::json::index::const_iterator
ircd::json::index::erase(const const_iterator &start,
                         const const_iterator &stop)
{
	return { idx.erase(start, stop) };
}

const ircd::json::value &
ircd::json::index::at(const string_view &path)
const
{
	const auto elem(split(path, '.'));
	const auto ident(split(elem.first, '['));
	const auto &name(ident.first);
	const auto &indice(ident.second);

	const auto it(find(name));
	if(unlikely(it == end()))
		throw not_found("'%s'", name);

	const auto &val(it->second);
	if(!indice.empty())
	{
		const auto idx(lex_cast<size_t>(rstrip(indice, ']')));

		if(type(val) != ARRAY)
			throw not_found("cannot recurse through non-array \"%s\" for indice [%zu]", name, idx);

		if(val.serial)
			throw not_found("cannot recurse through unparsed array \"%s\" for indice [%zu]", name, idx);

		assert(0);
		//return val.array->at(idx);
	}
	else if(!elem.second.empty())
	{
		if(type(val) != OBJECT)
			throw not_found("cannot recurse through non-object \"%s\" for \"%s\"", name, elem.second);

		if(val.serial)
			throw not_found("cannot recurse through unparsed object \"%s\" for \"%s\"", name, elem.second);

		return val.object->at(elem.second);
	}

	return it->second;
}

size_t
ircd::json::serialized(const index &index)
{
	return index.serialized();
}

size_t
ircd::json::index::serialized()
const
{
	const member *const &begin(&idx.front());
	const member *const &end(begin + idx.size());
	return json::serialized(begin, end);
}

ircd::json::index::operator std::string()
const
{
	const member *const &begin(&idx.front());
	const member *const &end(begin + idx.size());
	return json::string(begin, end);
}

///////////////////////////////////////////////////////////////////////////////
//
// json/value.h
//

ircd::json::value::~value()
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

ircd::json::value::operator std::string()
const
{
	switch(type)
	{
		case STRING:
		case LITERAL:
			return std::string(unquote(string_view(*this)));

		case OBJECT:
			if(serial)
				return std::string(string_view(*this));
			else
				return json::string(*object);

		case ARRAY:
			if(serial)
				return std::string(string_view(*this));
			else
				break;

		case NUMBER:
			if(serial)
				return std::string(string_view(*this));
			else if(floats)
				return std::string(lex_cast(floating));
			else
				return std::string(lex_cast(integer));
	}

	throw type_error("cannot index type[%d]", int(type));
}

ircd::json::value::operator string_view()
const
{
	switch(type)
	{
		case STRING:
			return unquote(string_view{string, len});

		case NUMBER:
			if(serial)
				return string_view{string, len};
			else if(floats)
				return byte_view<string_view>{floating};
			else
				return byte_view<string_view>{integer};

		case ARRAY:
		case OBJECT:
		case LITERAL:
			if(serial)
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
			assert(serial);
			return lex_cast<int64_t>(string_view(*this));

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
			assert(serial);
			return lex_cast<double>(string_view(*this));

		case ARRAY:
		case OBJECT:
		case LITERAL:
			break;
	}

	throw type_error("value type[%d] is not a float", int(type));
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
			       object? object->empty():
			               true;

		case ARRAY:
			return serial? string == nullptr:
			       array?  array == nullptr:
			               true;

		case LITERAL:
			return serial? string == nullptr:
			               true;
	};

	throw type_error("deciding if a type[%u] is undefined is undefined", int(type));
}

bool
ircd::json::value::empty()
const
{
	switch(type)
	{
		case NUMBER:   return lex_cast(integer).empty();
		case STRING:   return !len;
		case OBJECT:   return serial? !len : object? object->empty() : true;
		case ARRAY:    return serial? !len : array? false : true;            //TODO: XXX arr
		case LITERAL:  return serial? !len : true;
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
			                bool(integer);

		case STRING:
			return string == nullptr;

		case OBJECT:
			return serial? string == nullptr:
			       object? object->empty():
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

size_t
ircd::json::serialized(const value &v)
{
	switch(v.type)
	{
		case NUMBER:   return lex_cast(v.integer).size();
		case STRING:   return 1 + v.len + 1;
		case OBJECT:   return v.serial? v.len : serialized(*v.object);
		case ARRAY:    return v.serial? v.len : 2;
		case LITERAL:  return v.len;
	};

	throw type_error("deciding the size of a type[%u] is undefined", int(v.type));
}

std::ostream &
ircd::json::operator<<(std::ostream &s, const value &v)
{
	switch(v.type)
	{
		case STRING:
		case LITERAL:
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

		case NUMBER:
			if(v.serial)
				s << string_view(v);
			else if(v.floats)
				s << v.floating;
			else
				s << v.integer;
			break;
	}

	return s;
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

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const object &object)
{
	static const auto throws([]
	{
		throw print_error("The JSON generator failed to print object");
	});

	char *const start(begin(buf));
	karma::generate(begin(buf), maxwidth(size(buf))[printer.object] | eps[throws], object);

	return string_view
	{
		start, begin(buf)
	};
}

std::ostream &
ircd::json::operator<<(std::ostream &s, const object &object)
{
	const auto &os(ostreamer);
	static const auto throws([]
	{
		throw print_error("The JSON generator failed to output object to stream");
	});

	karma::ostream_iterator<char> osi(s);
	karma::generate(osi, os.object | eps[throws], object);
	return s;
}

std::ostream &
ircd::json::operator<<(std::ostream &s, const object::member &member)
{
	static const auto throws([]
	{
		throw print_error("The JSON generator failed to output object member to stream");
	});

	karma::ostream_iterator<char> osi(s);
	karma::generate(osi, ostreamer.member | eps[throws], member);
	return s;
}

ircd::json::object::const_iterator &
ircd::json::object::const_iterator::operator++()
{
	static const qi::rule<const char *, json::object::member> member
	{
		parser.name >> parser.ws >> parser.name_sep >> parser.ws >> raw[parser.value]
	};

	static const qi::rule<const char *, json::object::member> parse_next
	{
		parser.object_end | (parser.value_sep >> parser.ws >> member)
	};

	state.first = string_view{};
	state.second = string_view{};
	if(!qi::phrase_parse(start, stop, parse_next, parser.WS, state))
		start = stop;

	return *this;
}

ircd::json::object::operator std::string()
const
{
	//TODO: tmp
	std::stringstream ret;
	ret << (*this);
	return ret.str();
}

ircd::json::object::const_iterator
ircd::json::object::begin()
const
{
	static const qi::rule<const char *, json::object::member> member
	{
		parser.name >> parser.ws >> parser.name_sep >> parser.ws >> raw[parser.value]
	};

	static const qi::rule<const char *, json::object::member> parse_begin
	{
		parser.object_begin >> parser.ws >> (parser.object_end | member)
	};

	const_iterator ret(string_view::begin(), string_view::end());
	if(!qi::phrase_parse(ret.start, ret.stop, parse_begin, parser.WS, ret.state))
		ret.start = ret.stop;

	return ret;
}

ircd::json::object::const_iterator
ircd::json::object::end()
const
{
	return { string_view::end(), string_view::end() };
}

ircd::string_view
ircd::json::stringify(mutable_buffer &b,
                      const array &a)
{
	static const auto throws([]
	{
		throw print_error("The JSON generator failed to print array");
	});

	char *const start(begin(b));
	karma::generate(begin(b), maxwidth(size(b))[printer.array_begin] | eps[throws]);

	auto it(begin(a));
	if(it != end(a))
	{
		karma::generate(begin(b), maxwidth(size(b))[printer.elem] | eps[throws], *it);
		for(++it; it != end(a); ++it)
			karma::generate(begin(b), maxwidth(size(b))[printer.value_sep << printer.elem] | eps[throws], *it);
	}

	karma::generate(begin(b), maxwidth(size(b))[printer.array_end] | eps[throws]);
	return string_view{start, begin(b)};
}

std::ostream &
ircd::json::operator<<(std::ostream &s, const array &a)
{
	const auto &os(ostreamer);
	static const auto throws([]
	{
		throw print_error("The JSON generator failed to output array to stream");
	});

	karma::ostream_iterator<char> osi(s);
	karma::generate(osi, ostreamer.array_begin | eps[throws]);

	auto it(begin(a));
	if(it != end(a))
	{
		karma::generate(osi, ostreamer.elem | eps[throws], *it);
		for(++it; it != end(a); ++it)
			karma::generate(osi, (ostreamer.value_sep << ostreamer.elem) | eps[throws], *it);
	}

	karma::generate(osi, ostreamer.array_end | eps[throws]);
	return s;
}

ircd::json::array::const_iterator &
ircd::json::array::const_iterator::operator++()
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

ircd::json::array::operator std::string()
const
{
	//TODO: tmp
	std::stringstream ret;
	ret << (*this);
	return ret.str();
}

ircd::json::array::const_iterator
ircd::json::array::begin()
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

ircd::json::array::const_iterator
ircd::json::array::end()
const
{
	return { string_view::end(), string_view::end() };
}

size_t
ircd::json::serialized(const string_view &s)
{
	switch(type(s, std::nothrow))
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

///////////////////////////////////////////////////////////////////////////////
//
// iov.h
//

ircd::string_view
ircd::json::stringify(mutable_buffer &head,
                      const iov &iov)
{
	const auto num{count(iov)};
	const member *m[num];

	size_t i(0);
	for_each(iov, [&i, &m]
	(const auto &member)
	{
		m[i++] = &member;
	});

	return stringify(head, m, m + num);
}

size_t
ircd::json::count(const iov &iov)
{
	return count(iov, []
	(const auto &)
	{
		return true;
	});
}

size_t
ircd::json::count(const iov &iov,
                  const member_closure_bool &closure)
{
	size_t ret(0);
	for_each(iov, [&closure, &ret]
	(const auto &member)
	{
		ret += closure(member);
	});

	return ret;
}

void
ircd::json::for_each(const iov &b,
                     const member_closure &closure)
{
	if(b.b)
		std::for_each(b.b, b.e, closure);
	else if(!b.m.first.empty())
		closure(b.m);

	if(b.child)
		for_each(*b.child, closure);
}

bool
ircd::json::until(const iov &b,
                  const member_closure_bool &closure)
{
	if(b.b && b.e)
	{
		if(!ircd::until(b.b, b.e, closure))
			return false;
	}
	else if(!b.m.first.empty() && !closure(b.m))
		return false;

	if(b.child)
		return until(*b.child, closure);

	return true;
}

bool
ircd::json::iov::has(const string_view &key)
const
{
	const auto *const member(find(key));
	return member != nullptr;
}

const ircd::json::value &
ircd::json::iov::at(const string_view &key)
const
{
	const auto *const member(find(key));
	if(!member)
		throw not_found("'%s'", key);

	return member->second;
}

const ircd::json::member *
ircd::json::iov::find(const string_view &key)
const
{
	const member *ret;
	const auto test
	{
		[&key, &ret](const auto &member) -> bool
		{
			if(key == string_view{member.first})
			{
				ret = &member;
				return false;
			}
			else return true;
		}
	};

	return !until(*this, test)? ret : nullptr;
}

bool
ircd::json::iov::test(const member_closure_bool &closure)
const
{
	if(b && likely(e))
		return ircd::until(b, e, closure);

	if(!m.first.empty())
		return closure(m);

	return true;
}

ircd::json::iov::add::add(iov &head, const members &ms)
:iov{&head, nullptr, ms}
{
	const auto existing(json::find(&head, [&ms](const iov &existing)
	{
		return existing.test([&ms](const member &existing)
		{
			return std::all_of(begin(ms), end(ms), [&existing]
			(const auto &member)
			{
				return member.first == existing.first;
			});
		});
	}));

	if(existing)
		throw exists("failed to add member '%s': already exists",
		             string_view{existing->m.first}); //TODO BUG

	tail(&head)->child = this;
}

ircd::json::iov::add::add(iov &head, member member)
:iov{&head, nullptr, std::move(member)}
{
	const auto existing(json::find(&head, [&member](const iov &existing)
	{
		return existing.test([&member](const auto &existing)
		{
			return member.first == existing.first;
		});
	}));

	if(existing)
		throw exists("failed to add member '%s': already exists",
		             string_view{member.first});

	tail(&head)->child = this;
}

ircd::json::iov::set::set(iov &head, const members &ms)
:iov{&head, nullptr, ms}
{
	const auto existing(json::find(&head, [&ms](const iov &existing)
	{
		return existing.test([&ms](const member &existing)
		{
			return std::all_of(begin(ms), end(ms), [&existing]
			(const auto &member)
			{
				return member.first == existing.first;
			});
		});
	}));

	if(existing)
	{
		const auto p(prev(existing));
		if(p)
		{
			p->child = this;
			this->child = existing->child;
		}
	}
	else tail(&head)->child = this;
}

ircd::json::iov::set::set(iov &head, member member)
:iov{&head, nullptr, std::move(member)}
{
	const auto existing(json::find(&head, [&member](const iov &existing)
	{
		return existing.test([&member](const auto &existing)
		{
			return member.first == existing.first;
		});
	}));

	if(existing)
	{
		const auto p(prev(existing));
		if(p)
		{
			p->child = this;
			this->child = existing->child;
		}
	}
	else tail(&head)->child = this;
}
