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

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/karma.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/fusion/adapted/std_pair.hpp>
#include <boost/fusion/adapted/std_tuple.hpp>

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
using karma::maxwidth;
using karma::buffer;
using karma::eps;
using karma::attr_cast;

template<class it>
struct input
:qi::grammar<it, string_view>
{
	// insignificant whitespaces
	qi::rule<it> SP                              { lit('\x20'),                            "space" };
	qi::rule<it> HT                              { lit('\x09'),                   "horizontal tab" };
	qi::rule<it> CR                              { lit('\x0D'),                  "carriage return" };
	qi::rule<it> LF                              { lit('\x0A'),                        "line feed" };

	// whitespace skipping
	qi::rule<it> WS                              { SP | HT | CR | LF,                 "whitespace" };
	qi::rule<it> ws                              { *(WS),                      "whitespace monoid" };
	qi::rule<it> wsp                             { +(WS),                   "whitespace semigroup" };

	// structural 
	qi::rule<it> object_begin                    { lit('{'),                        "object begin" };
	qi::rule<it> object_end                      { lit('}'),                          "object end" };
	qi::rule<it> array_begin;
	qi::rule<it> array_end;
	qi::rule<it> name_sep;
	qi::rule<it> value_sep;

	// literal
	qi::rule<it, string_view> lit_true;
	qi::rule<it, string_view> lit_false;
	qi::rule<it, string_view> lit_null;

	qi::rule<it> quote;
	qi::rule<it, string_view> chars;
	qi::rule<it, string_view> string;

	qi::rule<it, string_view> boolean;
	qi::rule<it, string_view> literal;
	qi::rule<it, string_view> number;
	qi::rule<it, string_view> array;
	qi::rule<it, string_view> object;

	qi::rule<it, string_view> name;
	qi::rule<it, string_view> value;
	qi::rule<it, doc::member> member;

	qi::rule<it, int> type;

	input();
};

template<class it>
input<it>::input()
:input<it>::base_type
{
	object
}
,array_begin
{
	lit('[')
	,"array begin"
}
,array_end
{
	lit(']')
	,"array end"
}
,name_sep
{
	lit(':')
	,"name separator"
}
,value_sep
{
	lit(',')
	,"value separator"
}
,lit_true
{
	lit("true")
	,"literal true"
}
,lit_false
{
	lit("false")
	,"literal false"
}
,lit_null
{
	lit("null")
	,"literal null"
}
,quote
{
	lit('\"')
	,"quote"
}
,chars
{
	raw[*(char_ - quote)]
	,"string"
}
,string
{
	quote >> chars >> quote
	,"string"
}
,boolean
{
	lit_true | lit_false
	,"boolean"
}
,literal
{
	lit_true | lit_false | lit_null
	,"literal"
}
,number
{
	raw[double_]
	,"number"
}
,array
{
	array_begin >> -(omit[ws >> value >> ws] % value_sep) >> ws >> array_end
	,"array"
}
,object
{
	object_begin >> -(omit[ws >> member >> ws] % value_sep) >> ws >> object_end
	,"object"
}
,name
{
	string
	,"name"
}
,value
{
	lit_false | lit_true | lit_null | object | array | number | string
	,"value"
}
,member
{
	name >> -ws >> name_sep >> -ws >> value
	,"member"
}
,type
{
	(omit[object_begin]    >> attr(json::OBJECT))   |
	(omit[quote]           >> attr(json::STRING))   |
	(omit[number]          >> attr(json::NUMBER))   |
	(omit[literal]         >> attr(json::LITERAL))  |
	(omit[array_begin]     >> attr(json::ARRAY))
	,"type"
}
{
	array %= array_begin >> -(omit[ws >> value >> ws] % value_sep) >> ws >> array_end;
	object %= object_begin >> -(omit[ws >> member >> ws] % value_sep) >> ws >> object_end;
}

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
	rule<> object_begin                { lit('{')                                   ,"object begin" };
	rule<> object_end                  { lit('}')                                     ,"object end" };
	rule<> array_begin                 { lit('[')                                   ,"array begin"  };
	rule<> array_end                   { lit(']')                                     ,"array end"  };
	rule<> name_sep                    { lit(':')                                 ,"name separator" };
	rule<> value_sep                   { lit(',')                                ,"value separator" };
	rule<> quote                       { lit('"')                                          ,"quote" };

	rule<string_view> lit_true         { karma::string("true")                     ,"literal true" };
	rule<string_view> lit_false        { karma::string("false")                   ,"literal false" };
	rule<string_view> lit_null         { karma::string("null")                     ,"literal null" };

	rule<string_view> boolean          { lit_true | lit_false                           ,"boolean" };
	rule<string_view> literal          { lit_true | lit_false | lit_null                ,"literal" };

	rule<string_view> chars            { *(~char_("\""))                                  ,"chars" };
	rule<string_view> string           { quote << chars << quote                         ,"string" };

	rule<string_view> name             { string                                            ,"name" };
	rule<string_view> value            { karma::string                                    ,"value" };

	rule<const json::array &> elems    { (value % value_sep)                           ,"elements" };
	rule<const json::array &> array    { array_begin << elems << array_end                ,"array" };

	rule<doc::member> dmember          { name << name_sep << value                       ,"member" };
	rule<const json::doc &> dmembers   { (dmember % value_sep)                          ,"members" };
	rule<const json::doc &> document   { object_begin << dmembers << object_end        ,"document" };

	rule<doc::member> member           { name << name_sep << value                       ,"member" };
	rule<const json::obj &> members    { (member % value_sep)                           ,"members" };
	rule<const json::obj &> object     { object_begin << members << object_end           ,"object" };

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

doc serialize(const obj &, char *const &start, char *const &stop);
size_t print(char *const &buf, const size_t &max, const obj &);
size_t print(char *const &buf, const size_t &max, const doc &);

std::ostream &operator<<(std::ostream &, const doc::member &);
std::ostream &operator<<(std::ostream &, const doc &);
std::ostream &operator<<(std::ostream &, const obj &);

} // namespace json
} // namespace ircd

ircd::json::printer::printer()
{
	const auto recursor([this](auto &a, auto &b, auto &c)
	{
		const auto recurse_document([&]
		{
			char *out(const_cast<char *>(a.data()));
			karma::generate(out, maxwidth(a.size())[document], json::doc(a));
			a.resize(size_t(out - a.data()));
		});

		if(likely(!a.empty())) switch(a.front())
		{
			case '{':  recurse_document();  break;
			default:                        break;
		}
	});

	value %= karma::string[recursor];
}

ircd::json::ostreamer::ostreamer()
{
	const auto recursor([this](auto &a, auto &b, auto &c)
	{
		const auto recurse_document([&]
		{
			char *out(const_cast<char *>(a.data()));
			const auto count(print(out, a.size(), json::doc(a)));
			a.resize(count);
		});

		if(likely(!a.empty())) switch(a.front())
		{
			case '{':  recurse_document();  break;
			case '[':  c = false;           break;
			default:                        break;
		}
	});

	value %= karma::string[recursor];
}

size_t
ircd::json::print(char *const &buf,
                  const size_t &max,
                  const obj &obj)
{
	const auto doc(serialize(obj, buf, buf + max));
	return doc.size();
}

ircd::json::doc
ircd::json::serialize(const obj &obj,
                      char *const &start,
                      char *const &stop)
{
	static const auto throws([]
	{
		throw print_error("The JSON generator failed to serialize object");
	});

	static const karma::rule<char *, std::pair<string_view, string_view>> generate_kv
	{
		printer.quote << +char_ << printer.quote << ':' << +char_
	};

	char *out(start);
	const auto generate_member([&out, &stop]
	(const doc::member &member)
	{
		static const karma::rule<char *, std::pair<string_view, string_view>> generate_member
		{
			printer.value_sep << generate_kv
		};

		karma::generate(out, maxwidth(stop - out)[generate_member], member);
	});

	karma::generate(out, maxwidth(stop - out)[printer.object_begin] | eps[throws]);
	if(obj.count())
	{
		const auto &front(*begin(obj));
		karma::generate(out, maxwidth(stop - out)[generate_kv], front);
	}

	std::for_each(std::next(begin(obj), 1), end(obj), generate_member);
	karma::generate(out, maxwidth(stop - out)[printer.object_end] | eps[throws]);
	return string_view{start, out};
}

std::ostream &
ircd::json::operator<<(std::ostream &s, const obj &obj)
{
	static const auto throws([]
	{
		throw print_error("The JSON generator failed to output object to stream");
	});

	karma::ostream_iterator<char> osi(s);
	karma::generate(osi, ostreamer.object | eps[throws], obj);
	return s;
}

ircd::json::obj::obj()
:owns_state{false}
{
}

ircd::json::obj::obj(const doc &doc)
:state{doc}
,idx{std::begin(doc), std::end(doc)}
,owns_state{false}
{
}

ircd::json::obj::obj(const obj &other)
:state{[&other]
{
	const size_t size(other.size());
	std::unique_ptr<char[]> buf(new char[size + 1]);
	const auto ret(serialize(other, buf.get(), buf.get() + size));
	buf.get()[size] = '\0';
	buf.release();
	return ret;
}()}
,idx{std::begin(state), std::end(state)}
,owns_state{true}
{
	//TODO: if idx throws state.data() will leak
}

ircd::json::obj::~obj()
noexcept
{
	if(owns_state)
		delete[] state.data();

	for(const auto &member : idx)
	{
		if(member.owns_first)   delete[] member.first.data();
		if(member.owns_second)  delete[] member.second.data();
	}
}

bool
ircd::json::obj::serialized()
const
{
	return std::all_of(std::begin(idx), std::end(idx), [this]
	(const auto &member)
	{
		return state.contains(member.first) && state.contains(member.second);
	});
}

size_t
ircd::json::obj::size()
const
{
	const size_t ret(1 + idx.empty());
	return std::accumulate(std::begin(idx), std::end(idx), ret, []
	(auto ret, const auto &member)
	{
		return ret += 1 + member.first.size() + 1 + 1 + member.second.size() + 1;
	});
}

ircd::json::obj::delta
ircd::json::obj::operator[](const char *const &name)
{
	const auto it(idx.emplace(idx.end(), string_view{name}, string_view{}));
	auto &member(const_cast<obj::member &>(*it));
/*
	if(pit.second)
	{
		const size_t size(strlen(name));
		std::unique_ptr<char[]> rename(new char[size + 1]);
		strlcpy(rename.get(), name, size + 1);
		member.first = { rename.get(), size };
		member.owns_first = true;
		rename.release();
	}
*/
	return { *this, member, member.second };
}

ircd::json::obj::delta
ircd::json::obj::at(const char *const &name)
{
	const auto it(std::find(std::begin(idx), std::end(idx), string_view{name}));
	if(unlikely(it == idx.end()))
		throw not_found("name \"%s\"", name);

	auto &member(const_cast<obj::member &>(*it));
	return { *this, member, member.second };
}

ircd::json::obj::iterator
ircd::json::obj::begin()
{
	return { *this, std::begin(idx) };
}

ircd::json::obj::iterator
ircd::json::obj::end()
{
	return { *this, std::end(idx) };
}

ircd::json::obj::const_iterator
ircd::json::obj::cbegin()
{
	return { std::begin(idx) };
}

ircd::json::obj::const_iterator
ircd::json::obj::cend()
{
	return { std::end(idx) };
}

ircd::json::obj::const_iterator
ircd::json::obj::begin()
const
{
	return { std::begin(idx) };
}

ircd::json::obj::const_iterator
ircd::json::obj::end()
const
{
	return { std::end(idx) };
}

ircd::json::obj::iterator::iterator(struct obj &obj,
                                    obj::index::iterator it)
:obj{&obj}
,it{it}
{
}

ircd::json::obj::iterator::value_type *
ircd::json::obj::iterator::operator->()
{
	auto &member(const_cast<obj::member &>(*it));
	state = proxy
	{
		{ *obj, member, member.first  },
		{ *obj, member, member.second }
	};
	return &state;
}

ircd::json::obj::iterator &
ircd::json::obj::iterator::operator++()
{
	++it;
	return *this;
}

ircd::json::obj::delta::delta(struct obj &obj,
                              obj::member &member,
                              const string_view &current)
:string_view{current}
,obj{&obj}
,member{&member}
{
}

void
ircd::json::obj::delta::set(const json::obj &obj)
{
	const size_t size(obj.size());
	std::unique_ptr<char[]> buf(new char[size + 1]);
	const auto doc(serialize(obj, buf.get(), buf.get() + size));
	buf.get()[size] = '\0';
	commit(doc);
	member->owns_second = true;
	buf.release();
}

void
ircd::json::obj::delta::set(const string_view &value)
{
	commit(value);
}

void
ircd::json::obj::delta::set(const char *const &string)
{
	const auto size(strlen(string));
	const auto max(size + 1);
	std::unique_ptr<char[]> restart(new char[max]);
	strlcpy(restart.get(), string, max);
	commit(string_view(restart.get(), restart.get() + size));
	member->owns_second = true;
	restart.release();
}

void
ircd::json::obj::delta::set(const std::string &string)
{
	const auto max(string.size() + 1);
	std::unique_ptr<char[]> restart(new char[max]);
	strlcpy(restart.get(), string.data(), max);
	commit(string_view(restart.get(), restart.get() + string.size()));
	member->owns_second = true;
	restart.release();
}

void
ircd::json::obj::delta::set(const bool &boolean)
{
	static const char *const true_p("true");
	static const char *const false_p("false");

	const auto ptr(boolean? true_p : false_p);
	commit(string_view(ptr));
}

void
ircd::json::obj::delta::set(const int32_t &number)
{
	static const size_t max(16);
	std::unique_ptr<char[]> restart(new char[max]);
	commit(lex_cast(number, restart.get(), max));
	member->owns_second = true;
	restart.release();
}

void
ircd::json::obj::delta::set(const uint64_t &number)
{
	static const size_t max(32);
	std::unique_ptr<char[]> restart(new char[max]);
	commit(lex_cast(number, restart.get(), max));
	member->owns_second = true;
	restart.release();
}

void
ircd::json::obj::delta::set(const double &number)
{
	static const size_t max(64);
	std::unique_ptr<char[]> restart(new char[max]);
	commit(lex_cast(number, restart.get(), max));
	member->owns_second = true;
	restart.release();
}

void
ircd::json::obj::delta::commit(const string_view &buf)
{
	if(member->owns_second)
	{
		delete[] member->second.data();
		member->owns_second = false;
	}

	static_cast<string_view &>(*this) = buf;
	member->second = buf;
}

size_t
ircd::json::print(char *const &buf,
                  const size_t &max,
                  const doc &doc)
{
	static const auto throws([]
	{
		throw print_error("The JSON generator failed to print document");
	});

	char *out(buf);
	karma::generate(out, maxwidth(max)[printer.document] | eps[throws], doc);
	return std::distance(buf, out);
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
	karma::generate(osi, ostreamer.dmember | eps[throws], member);
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

	iterator ret(string_view::begin(), string_view::end());
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

ircd::json::array::const_iterator &
ircd::json::array::const_iterator::operator++()
{
	static const qi::rule<const char *, string_view> parse_next
	{
		(parser.array_end | (parser.value_sep >> parser.ws >> raw[parser.value]))
	};

	state = string_view{};
	if(!qi::phrase_parse(start, stop, parse_next, parser.WS, state))
		start = stop;

	return *this;
}

ircd::json::array::const_iterator
ircd::json::array::begin()
const
{
	static const qi::rule<const char *, string_view> parse_begin
	{
		parser.array_begin >> parser.ws >> (parser.array_end | raw[parser.value])
	};

	iterator ret(state.begin(), state.end());
	if(!qi::phrase_parse(ret.start, ret.stop, parse_begin, parser.WS, ret.state))
		ret.start = ret.stop;

	return ret;
}

ircd::json::array::const_iterator
ircd::json::array::end()
const
{
	return { state.end(), state.end() };
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

///////////////////////////////////////////////////////////////////////////////
//
// ircd/json.h
//

/*
const char *const packet_test
{R"({
		"betty": 1,
		"boop": "beep"
})"};
*/

const char *const packet_test[]
{
	R"({"type":"m.login.password"})",

	R"({"type":"m.login.password","user":"jzk","password":"foobar"})",

	R"({"type":"m.login.password","user":"jzk","password":1.337,"number":1337})",

	R"({"user":{"name":"jzk"},"pass":true})",

	R"({"type":"m.login.password","user":{"name":{"text":"jzk"}}})",

	R"({"type":"m.login.password","bap":"boop","user":{"name":"jzk"},"password":"hi"})",

	R"({"type":"m.login.password","user":{"name":"jzk","profile":{"foo":"bar"}},"password":1.337,"logins":1337})",

	R"({"user":{"name":"jzk"}})",

	R"({ "versions": [ "r0.0.1" ,  "r0.1.0" ,  "r0.2.0" ] })",

R"(
 {
      "origin_server_ts": 1444812213737,
      "user_id": "@alice:example.com",
      "event_id": "$1444812213350496Caaaa:example.com",
      "content": {
        "body": "hello world",
        "msgtype":"m.text"
      },
      "room_id":"!Xq3620DUiqCaoxq:example.com",
      "type":"m.room.message",
      "age": 1042
    }
)",

};

void
ircd::json::test()
{
	static char packet[4096] {0};
	strcpy(packet, packet_test[9]);
	const auto max(strlen(packet));

	printf("packet(%zu) @%p [%s]\n\n", max, packet, packet);
	std::cout << std::endl;
	std::cout << std::endl;

	const char *ptr(packet);
	const char *const stop(packet + max);

	const json::doc doc
	{
		string_view { ptr, stop }
	};

	const json::obj obj{doc};
	std::cout << obj["type"] << std::endl;


	//json::delta foo(doc.find("type")->first, 1234);
	//std::cout << "[" << foo.first << "]" << std::endl;
	//std::cout << "[" << foo.second << "]" << std::endl;
	//std::cout << "[" << obj["type"] << "]" << std::endl;
	//obj["type"] = "cya";
	//std::cout << "[" << obj["type"] << "]" << std::endl;

	//printf("test (%zu)\n", ret.size());
//	for(const auto &sv : ret)
//		printf("[%s]\n", sv.data());
	//printf("[%s]\n", ret.data());
}

namespace ircd {
namespace json {

} // namespace json
} // namespace ircd
