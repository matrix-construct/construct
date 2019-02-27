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

	struct input;
	struct output;

	// Instantiations of the grammars
	struct parser extern const parser;
	struct printer extern const printer;

	const size_t &error_show_max {48};
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

struct ircd::json::input
:qi::grammar<const char *, unused_type>
{
	using it = const char *;
	template<class T = unused_type, class... A> using rule = qi::rule<it, T, A...>;

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
	rule<> number
	{
		double_ | long_
		,"number"
	};

	// string
	rule<> unicode
	{
		lit('u') >> qi::uint_parser<uint64_t, 16, 1, 12>{}
		,"escaped unicode"
	};

	rule<> escaped
	{
		lit('"') | lit('\\') | lit('\b') | lit('\f') | lit('\n') | lit('\r') | lit('\t') | lit('\0')
		,"escaped"
	};

	rule<> escaper
	{
		lit('"') | lit('\\') | lit('b') | lit('f') | lit('n') | lit('r') | lit('t') | lit('0') | unicode
		,"escaped"
	};

	rule<> escaper_nc
	{
		escaper | lit('/')
	};

	rule<string_view> chars
	{
		raw[*((char_ - escaped) | (escape >> escaper_nc))]
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
		name >> -ws >> name_sep >> -ws >> value(depth)
		,"member"
	};

	const rule<unused_type(uint)> object
	{
		(eps(depth < json::object::max_recursion_depth) | eps[throws_exceeded]) >>

		object_begin >> -((-ws >> member(depth)) % (-ws >> value_sep)) >> -ws >> object_end
		,"object"
	};

	const rule<unused_type(uint)> array
	{
		(eps(depth < json::array::max_recursion_depth) | eps[throws_exceeded]) >>

		array_begin >> -((-ws >> value(depth)) % (-ws >> value_sep)) >> -ws >> array_end
		,"array"
	};

	const rule<unused_type(uint)> value
	{
		lit_false | lit_null | lit_true | string | number | object(depth + 1) | array(depth + 1)
		,"value"
	};

	rule<int> type
	{
		(omit[quote]           >> attr(json::STRING))  |
		(omit[object_begin]    >> attr(json::OBJECT))  |
		(omit[array_begin]     >> attr(json::ARRAY))   |
		(omit[number >> eoi]   >> attr(json::NUMBER))  |
		(omit[literal >> eoi]  >> attr(json::LITERAL))
		,"type"
	};

	input()
	:input::base_type{rule<>{}}
	{}
};

struct ircd::json::output
:karma::grammar<char *, unused_type>
{
	using it = char *;
	template<class T = unused_type, class... A> using rule = karma::rule<it, T, A...>;

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

	// literal
	rule<string_view> lit_true         { karma::string("true")                     ,"literal true" };
	rule<string_view> lit_false        { karma::string("false")                   ,"literal false" };
	rule<string_view> lit_null         { karma::string("null")                     ,"literal null" };
	rule<string_view> boolean          { lit_true | lit_false                           ,"boolean" };
	rule<string_view> literal          { lit_true | lit_false | lit_null                ,"literal" };

	// number
	rule<string_view> number
	{
		double_
		,"number"
	};

	// string
	std::map<char, const char *> escapes
	{
		{ '"',    "\\\""  },
		{ '\\',   "\\\\"  },
		{ '\b',   "\\b"   },
		{ '\f',   "\\f"   },
		{ '\n',   "\\n"   },
		{ '\r',   "\\r"   },
		{ '\t',   "\\t"   },
		{ '\0',   "\\0"   },
	};

	karma::symbols<char, const char *> escaped
	{
		"escaped"
	};

	rule<char()> character
	{
		escaped | char_
	};

	rule<string_view> string
	{
		quote << *(character) << quote
		,"string"
	};

	rule<string_view> name
	{
		string
		,"name"
	};

	rule<json::object> object
	{
		object_begin << -(member % value_sep) << object_end
		,"object"
	};

	rule<json::array> array
	{
		array_begin << -(value % value_sep) << array_end
		,"array"
	};

	rule<json::object::member> member
	{
		name << name_sep << value
		,"member"
	};

	rule<string_view> value
	{
		  (&object << object)
		| (&array << array)
		| (&literal << literal)
		| (&number << number)
		| string
		,"value"
	};

	output()
	:output::base_type{rule<>{}}
	{
		for(const auto &p : escapes)
			escaped.add(p.first, p.second);
	}
};

struct ircd::json::parser
:input
{
	using input::input;
}
const ircd::json::parser;

struct ircd::json::printer
:output
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

void
ircd::json::input::throws_exceeded()
{
	throw recursion_limit
	{
		"Maximum recursion depth exceeded"
	};
}

///////////////////////////////////////////////////////////////////////////////
//
// json/stack.h
//

ircd::json::stack::stack(const mutable_buffer &buf,
                         flush_callback flusher,
                         const size_t &hiwat,
                         const size_t &lowat)
:buf{buf}
,flusher{std::move(flusher)}
,hiwat{hiwat}
,lowat{lowat}
{
}

ircd::json::stack::stack(stack &&other)
noexcept
:buf{std::move(other.buf)}
,flusher{std::move(other.flusher)}
,eptr{std::move(other.eptr)}
,cp{std::move(other.cp)}
,hiwat{std::move(other.hiwat)}
,lowat{std::move(other.lowat)}
,co{std::move(other.co)}
,ca{std::move(other.ca)}
{
	other.cp = nullptr;
	other.co = nullptr;
	other.ca = nullptr;

	if(cp)
	{
		assert(cp->s == &other);
		cp->s = this;
	}

	if(co)
	{
		assert(co->s == &other);
		co->s = this;
	}

	if(ca)
	{
		assert(ca->s == &other);
		ca->s = this;
	}
}

ircd::json::stack::~stack()
noexcept
{
	assert(closed());
	if(buf.consumed())
		flush(true);

	assert(clean() || done());
}

void
ircd::json::stack::append(const char &c)
noexcept
{
	append(1, [&c](const mutable_buffer &buf)
	{
		buf[0] = c;
		return 1;
	});
}

void
ircd::json::stack::append(const string_view &s)
noexcept
{
	append(size(s), [&s](const mutable_buffer &buf)
	{
		return copy(buf, s);
	});
}

void
ircd::json::stack::append(const size_t &expect,
                          const window_buffer::closure &closure)
noexcept try
{
	if(unlikely(eptr))
		return;

	if(expect > buf.remaining())
	{
		if(unlikely(!flusher)) throw print_error
		{
			"Insufficient buffer. I need %zu more bytes; you only have %zu left (of %zu).",
			expect,
			buf.remaining(),
			size(buf.base)
		};

		if(!flush(true))
			return;

		if(unlikely(expect > buf.remaining())) throw print_error
		{
			"Insufficient flush. I still need %zu more bytes to buffer.",
			expect - buf.remaining()
		};
	}

	buf([&closure](const mutable_buffer &buf)
	{
		return closure(buf);
	});

	if(!buf.remaining())
		flush(true);
	else if(buf.consumed() >= hiwat)
		flush();
}
catch(...)
{
	assert(!this->eptr);
	this->eptr = std::current_exception();
}

void
ircd::json::stack::rethrow_exception()
{
	if(unlikely(eptr))
		std::rethrow_exception(eptr);
}

bool
ircd::json::stack::flush(const bool &force)
noexcept try
{
	if(!flusher)
		return false;

	if(unlikely(eptr))
		return false;

	if(!force && buf.consumed() < lowat)
		return false;

	if(!force && cp)
		return false;

	if(cp)
		cp = nullptr;

	// The user returns the portion of the buffer they were able to flush
	// rather than forcing them to wait on their sink to flush the whole
	// thing, they can continue with us for a little while more.
	const const_buffer flushed
	{
		flusher(buf.completed())
	};

	assert(data(flushed) == data(buf.completed())); // Can only flush front sry
	buf.shift(size(flushed));
	return true;
}
catch(...)
{
	assert(!this->eptr);
	this->eptr = std::current_exception();
	return false;
}

void
ircd::json::stack::clear()
{
	rewind(buf.consumed());
	this->eptr = std::exception_ptr{};
}

size_t
ircd::json::stack::rewind(const size_t &bytes)
{
	const size_t before(buf.consumed());
	const size_t &amount(std::min(bytes, before));
	const size_t after(size(buf.rewind(amount)));
	assert(before >= after);
	assert(before - after == amount);
	return amount;
}

ircd::const_buffer
ircd::json::stack::completed()
const
{
	return buf.completed();
}

size_t
ircd::json::stack::remaining()
const
{
	return buf.remaining();
}

bool
ircd::json::stack::done()
const
{
	return closed() && buf.consumed();
}

bool
ircd::json::stack::clean()
const
{
	return closed() && !buf.consumed();
}

bool
ircd::json::stack::closed()
const
{
	return !opened();
}

bool
ircd::json::stack::opened()
const
{
	return co || ca;
}

//
// object
//

ircd::json::stack::object &
ircd::json::stack::object::top(stack &s)
{
	const chase t{s, true};
	if(unlikely(!t.o))
		throw type_error
		{
			"Top of stack is not of type object. (o:%b a:%b m:%b)",
			bool(t.o),
			bool(t.a),
			bool(t.m),
		};

	return *t.o;
}

const ircd::json::stack::object &
ircd::json::stack::object::top(const stack &s)
{
	const const_chase t{s, true};
	if(unlikely(!t.o))
		throw type_error
		{
			"Top of stack is not of type object. (o:%b a:%b m:%b)",
			bool(t.o),
			bool(t.a),
			bool(t.m),
		};

	return *t.o;
}

ircd::json::stack::object::object(object &&other)
noexcept
:m{std::move(other.m)}
,s{std::move(other.s)}
,pm{std::move(other.pm)}
,pa{std::move(other.pa)}
,cm{std::move(other.cm)}
,mc{std::move(other.mc)}
{
	other.s = nullptr;

	if(s)
	{
		assert(s->co == &other);
		s->co = this;
	}

	if(pm)
	{
		assert(pm->co == &other);
		pm->co = this;
	}
	else if(pa)
	{
		assert(pa->co == &other);
		pa->co = this;
	}

	if(cm)
	{
		assert(cm->po == &other);
		cm->po = this;
	}
}

ircd::json::stack::object::object(stack &s)
:s{&s}
{
	const chase t{s, true};
	if(t.a)
	{
		new (this) object{*t.a};
		return;
	}
	else if(t.m)
	{
		new (this) object{*t.m};
		return;
	}
	else if(t.o)
	{
		assert(0);
		return;
	}

	assert(s.clean());
	s.co = this;
	s.append("{"_sv);
}

ircd::json::stack::object::object(stack &s,
                                  const string_view &name)
:object{object::top(s), name}
{
}

ircd::json::stack::object::object(object &po,
                                  const string_view &name)
:m{po, name}
,s{po.s}
,pm{&m}
{
	assert(s->opened());
	s->rethrow_exception();

	assert(pm->co == nullptr);
	assert(pm->ca == nullptr);
	pm->co = this;
	s->append("{"_sv);
	pm->vc |= true;
}

ircd::json::stack::object::object(member &pm)
:s{pm.s}
,pm{&pm}
{
	assert(s->opened());
	s->rethrow_exception();

	assert(pm.co == nullptr);
	assert(pm.ca == nullptr);
	pm.co = this;
	s->append("{"_sv);
	pm.vc |= true;
}

ircd::json::stack::object::object(array &pa)
:s{pa.s}
,pa{&pa}
{
	assert(s->opened());
	s->rethrow_exception();

	assert(pa.co == nullptr);
	assert(pa.ca == nullptr);
	pa.co = this;

	if(pa.vc)
		s->append(","_sv);

	s->append("{"_sv);
}

void
ircd::json::stack::object::append(const json::object &object)
{
	for(const auto &kv : object)
		json::stack::member
		{
			*this, kv.first, kv.second
		};
}

ircd::json::stack::object::~object()
noexcept
{
	if(!s)
		return; // std::move()'ed away

	const unwind _{[this]
	{
		// Allows ~dtor to be called to close the JSON manually
		s = nullptr;
	}};

	assert(cm == nullptr);
	s->append("}"_sv);

	if(pm) // branch taken if member of object
	{
		assert(pa == nullptr);
		assert(pm->ca == nullptr);
		assert(pm->co == this);
		pm->co = nullptr;
		return;
	}

	if(pa) // branch taken if value in array
	{
		assert(pm == nullptr);
		assert(pa->ca == nullptr);
		assert(pa->co == this);
		pa->vc++;
		pa->co = nullptr;
		return;
	}

	// branch taken if top of stack::
	assert(s->co == this);
	assert(s->ca == nullptr);
	assert(pm == nullptr && pa == nullptr);
	s->co = nullptr;
	assert(s->closed());
}

//
// array
//

ircd::json::stack::array &
ircd::json::stack::array::top(stack &s)
{
	const chase t{s, true};

	if(unlikely(!t.a))
		throw type_error
		{
			"Top of stack is not of type array. (o:%b a:%b m:%b)",
			bool(t.o),
			bool(t.a),
			bool(t.m),
		};

	return *t.a;
}

const ircd::json::stack::array &
ircd::json::stack::array::top(const stack &s)
{
	const const_chase t{s, true};

	if(unlikely(!t.a))
		throw type_error
		{
			"Top of stack is not of type array. (o:%b a:%b m:%b)",
			bool(t.o),
			bool(t.a),
			bool(t.m),
		};

	return *t.a;
}

ircd::json::stack::array::array(array &&other)
noexcept
:m{std::move(other.m)}
,s{std::move(other.s)}
,pm{std::move(other.pm)}
,pa{std::move(other.pa)}
,co{std::move(other.co)}
,ca{std::move(other.ca)}
,vc{std::move(other.vc)}
{
	other.s = nullptr;

	if(s)
	{
		assert(s->ca == &other);
		s->ca = this;
	}

	if(pm)
	{
		assert(pm->ca == &other);
		pm->ca = this;
	}
	else if(pa)
	{
		assert(pa->ca == &other);
		pa->ca = this;
	}

	if(co)
	{
		assert(co->pa == &other);
		co->pa = this;
	}
	else if(ca)
	{
		assert(ca->pa == &other);
		ca->pa = this;
	}
}

ircd::json::stack::array::array(stack &s)
:s{&s}
{
	const chase t{s, true};

	if(t.a)
	{
		new (this) array{*t.a};
		return;
	}
	else if(t.m)
	{
		new (this) array{*t.m};
		return;
	}
	else if(t.o)
	{
		assert(0);
		return;
	}

	assert(s.clean());
	s.ca = this;
	s.append("["_sv);
}

ircd::json::stack::array::array(stack &s,
                                const string_view &name)
:array{object::top(s), name}
{
}

ircd::json::stack::array::array(object &po,
                                const string_view &name)
:m{po, name}
,s{po.s}
,pm{&m}
{
	assert(s->opened());
	s->rethrow_exception();

	assert(pm->co == nullptr);
	assert(pm->ca == nullptr);
	pm->ca = this;
	s->append("["_sv);
	pm->vc |= true;
}

ircd::json::stack::array::array(array &pa)
:s{pa.s}
,pa{&pa}
{
	assert(s->opened());
	s->rethrow_exception();

	assert(pa.co == nullptr);
	assert(pa.ca == nullptr);
	pa.ca = this;

	if(pa.vc)
		s->append(","_sv);

	s->append("["_sv);
}

ircd::json::stack::array::array(member &pm)
:s{pm.s}
,pm{&pm}
{
	assert(s->opened());
	s->rethrow_exception();

	assert(pm.co == nullptr);
	assert(pm.ca == nullptr);
	pm.ca = this;
	s->append("["_sv);
	pm.vc |= true;
}

ircd::json::stack::array::~array()
noexcept
{
	if(!s)
		return; // std::move()'ed away

	const unwind _{[this]
	{
		// Allows ~dtor to be called to close the JSON manually
		s = nullptr;
	}};

	assert(co == nullptr);
	assert(ca == nullptr);
	s->append("]"_sv);

	if(pm) // branch taken if member of object
	{
		assert(pa == nullptr);
		assert(pm->ca == this);
		assert(pm->co == nullptr);
		pm->ca = nullptr;
		return;
	}

	if(pa) // branch taken if value in array
	{
		assert(pm == nullptr);
		assert(pa->ca == this);
		assert(pa->co == nullptr);
		pa->vc++;
		pa->ca = nullptr;
		return;
	}

	// branch taken if top of stack::
	assert(s->ca == this);
	assert(s->co == nullptr);
	assert(pm == nullptr && pa == nullptr);
	s->ca = nullptr;
	assert(s->closed());
}

void
ircd::json::stack::array::append(const json::value &value)
{
	assert(s);
	_pre_append();
	const unwind post{[this]
	{
		_post_append();
	}};

	s->append(serialized(value), [&value]
	(mutable_buffer buf)
	{
		return size(stringify(buf, value));
	});
}

void
ircd::json::stack::array::_pre_append()
{
	if(vc)
		s->append(","_sv);

	s->rethrow_exception();
}

void
ircd::json::stack::array::_post_append()
{
	++vc;
}

//
// member
//

ircd::json::stack::member &
ircd::json::stack::member::top(stack &s)
{
	const chase t{s, true};

	if(unlikely(!t.m))
		throw type_error
		{
			"Top of stack is not of type member. (o:%b a:%b m:%b)",
			bool(t.o),
			bool(t.a),
			bool(t.m),
		};

	return *t.m;
}

const ircd::json::stack::member &
ircd::json::stack::member::top(const stack &s)
{
	const const_chase t{s, true};

	if(unlikely(!t.m))
		throw type_error
		{
			"Top of stack is not of type member. (o:%b a:%b m:%b)",
			bool(t.o),
			bool(t.a),
			bool(t.m),
		};

	return *t.m;
}

ircd::json::stack::member::member(member &&other)
noexcept
:s{std::move(other.s)}
,po{std::move(other.po)}
,name{std::move(other.name)}
,co{std::move(other.co)}
,ca{std::move(other.ca)}
,vc{std::move(other.vc)}
{
	other.s = nullptr;

	if(po)
	{
		assert(po->cm == &other);
		po->cm = this;
	}

	if(co)
	{
		assert(co->pm == &other);
		co->pm = this;
	}
	else if(ca)
	{
		assert(ca->pm == &other);
		ca->pm = this;
	}
}

ircd::json::stack::member::member(stack &s,
                                  const string_view &name)
:member
{
	object::top(s), name
}
{
}

ircd::json::stack::member::member(object &po,
                                  const string_view &name)
:s{po.s}
,po{&po}
,name{name}
{
	assert(s->opened());
	s->rethrow_exception();

	assert(po.cm == nullptr);
	po.cm = this;

	if(po.mc)
		s->append(","_sv);

	thread_local char tmp[2048];
	static const json::printer::rule<string_view> rule
	{
		json::printer.name << json::printer.name_sep
	};

	mutable_buffer buf{tmp};
	if(!printer(buf, rule, name))
		throw error
		{
			"member name overflow: max size is under %zu", sizeof(tmp)
		};

	assert(data(buf) >= tmp);
	s->append(string_view{tmp, size_t(data(buf) - tmp)});
}

ircd::json::stack::member::member(stack &s,
                                  const string_view &name,
                                  const json::value &value)
:member
{
	object::top(s), name, value
}
{
}

ircd::json::stack::member::member(object &po,
                                  const string_view &name,
                                  const json::value &value)
:member{po, name}
{
	append(value);
}

ircd::json::stack::member::~member()
noexcept
{
	if(!s)
		return; // std::move()'ed away

	const unwind _{[this]
	{
		// Allows ~dtor to be called to close the JSON manually
		s = nullptr;
	}};

	if(!vc)
		s->append("null");

	assert(co == nullptr);
	assert(ca == nullptr);
	assert(po);
	assert(po->cm == this);
	po->mc++;
	po->cm = nullptr;
}

void
ircd::json::stack::member::append(const json::value &value)
{
	assert(s);
	_pre_append();
	const unwind post{[this]
	{
		_post_append();
	}};

	s->append(serialized(value), [&value]
	(mutable_buffer buf)
	{
		return size(stringify(buf, value));
	});
}

void
ircd::json::stack::member::_pre_append()
{
	assert(!vc);
}

void
ircd::json::stack::member::_post_append()
{
	vc |= true;
}

//
// stack::checkpoint
//

ircd::json::stack::checkpoint::checkpoint(stack &s)
:s{&s}
,pc{s.cp}
,point
{
	s.buf.consumed()
}
,vc{[&s]
{
	const chase top
	{
		s, true
	};

	return
		top.o?
			top.o->mc:
		top.a?
			top.a->vc:
		top.m?
			top.m->vc:
		0;
}()}
{
	s.cp = this;
}

ircd::json::stack::checkpoint::~checkpoint()
noexcept
{
	if(!s)
		return;

	if(!s->cp)
		return;

	if(std::uncaught_exceptions())
		rollback();

	if(!committing())
	{
		assert(point <= s->buf.consumed());
		s->buf.rewind(s->buf.consumed() - point);

		const chase top
		{
			*s, true
		};

		if(top.o)
			top.o->mc = vc;
		else if(top.a)
			top.a->vc = vc;
		else if(top.m)
			top.m->vc = vc;
	}

	assert(s->cp == this);
	s->cp = pc;
}

bool
ircd::json::stack::checkpoint::rollback()
{
	committed = false;
	return true;
}

bool
ircd::json::stack::checkpoint::recommit()
{
	committed = true;
	return true;
}

bool
ircd::json::stack::checkpoint::committing()
const
{
	return committed;
}

//
// chase
//

namespace ircd::json
{
	template<class chase> static bool _next(chase &);
	template<class chase> static bool _prev(chase &);
}

ircd::json::stack::chase::chase(stack &s,
                                const bool &prechase)
:a{s.ca}
,o{s.co}
,m{nullptr}
{
	if(prechase)
		while(next());
}

bool
ircd::json::stack::chase::next()
{
	return _next(*this);
}

bool
ircd::json::stack::chase::prev()
{
	return _prev(*this);
}

//
// const_chase
//

ircd::json::stack::const_chase::const_chase(const stack &s,
                                            const bool &prechase)
:a{s.ca}
,o{s.co}
,m{nullptr}
{
	if(prechase)
		while(next());
}

bool
ircd::json::stack::const_chase::next()
{
	return _next(*this);
}

bool
ircd::json::stack::const_chase::prev()
{
	return _prev(*this);
}

//
// chase internal
//

template<class chase>
bool
ircd::json::_next(chase &c)
{
	if(c.o)
	{
		if(!c.o->cm)
			return false;

		c.m = c.o->cm;
		c.a = nullptr;
		c.o = nullptr;
		return true;
	}
	else if(c.a)
	{
		if(!c.a->co && !c.a->ca)
			return false;

		c.m = nullptr;
		c.o = c.a->co;
		c.a = c.a->ca;
		return true;
	}
	else if(c.m)
	{
		if(!c.m->co && !c.m->ca)
			return false;

		c.o = c.m->co;
		c.a = c.m->ca;
		c.m = nullptr;
		return true;
	}
	else return false;
}

template<class chase>
bool
ircd::json::_prev(chase &c)
{
	if(c.o)
	{
		if(!c.o->pa && !c.o->pm)
			return false;

		c.a = c.o->pa;
		c.m = c.o->pm;
		c.o = nullptr;
		return true;
	}
	else if(c.a)
	{
		if(!c.a->pa && !c.a->pm)
			return false;

		c.m = c.a->pm;
		c.a = c.a->pa;
		c.o = nullptr;
		return true;
	}
	else if(c.m)
	{
		assert(c.m->po);
		c.o = c.m->po;
		c.a = nullptr;
		c.m = nullptr;
		return true;
	}
	else return false;
}

///////////////////////////////////////////////////////////////////////////////
//
// json/iov.h
//

decltype(ircd::json::iov::max_size)
ircd::json::iov::max_size;

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
	const ctx::critical_assertion ca;
	thread_local const member *m[iov.max_size];
	if(unlikely(size_t(iov.size()) > iov.max_size))
		throw iov::oversize
		{
			"IOV has %zd members but maximum is %zu",
			iov.size(),
			iov.max_size
		};

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
	const string_view ret
	{
		start, begin(buf)
	};

	assert(serialized(iov) == size(ret));
	return ret;
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

ircd::json::value &
ircd::json::iov::at(const string_view &key)
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

ircd::json::iov::push::push(iov &iov,
                            member member)
:node
{
	iov, std::move(member)
}
{
}

ircd::json::iov::push::push(iov &iov,
                            const bool &b,
                            const conditional_member &cp)
:node
{
	b?
		&iov:
		nullptr,

	b?
		member{cp.first, cp.second()}:
		member{}
}
{
}

ircd::json::iov::add::add(iov &iov,
                          member member)
:node
{
	iov, [&iov, &member]
	{
		if(iov.has(member.first))
			throw exists
			{
				"failed to add member '%s': already exists",
				string_view{member.first}
			};

		return std::move(member);
	}()
}
{
}

ircd::json::iov::add::add(iov &iov,
                          const bool &b,
                          const conditional_member &cp)
:node
{
	b?
		&iov:
		nullptr,

	[&iov, &b, &cp]
	{
		if(!b)
			return member{};

		if(iov.has(cp.first))
			throw exists
			{
				"failed to add member '%s': already exists",
				string_view{cp.first}
			};

		return member
		{
			cp.first, cp.second()
		};
	}()
}
{
}

ircd::json::iov::set::set(iov &iov,
                          member member)
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

ircd::json::iov::set::set(iov &iov,
                          const bool &b,
                          const conditional_member &cp)
:node
{
	b?
		&iov:
		nullptr,

	[&iov, &b, &cp]
	{
		if(!b)
			return member{};

		iov.remove_if([&cp](const auto &existing)
		{
			return string_view{existing.first} == cp.first;
		});

		return member
		{
			cp.first, cp.second()
		};
	}()
}
{
}

ircd::json::iov::defaults::defaults(iov &iov,
                                    member member)
:node
{
	!iov.has(member.first)?
		&iov:
		nullptr,

	std::move(member)
}
{
}

ircd::json::iov::defaults::defaults(iov &iov,
                                    bool b,
                                    const conditional_member &cp)
:node
{
	[&iov, &b, &cp]() -> json::iov *
	{
		if(!b)
			return nullptr;

		if(!iov.has(cp.first))
			return &iov;

		b = false;
		return nullptr;
	}(),

	[&iov, &b, &cp]
	{
		if(!b)
			return member{};

		return member
		{
			cp.first, cp.second()
		};
	}()
}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// json/strung.h
//

ircd::json::strung
ircd::json::insert(const strung &s,
                   const json::member &m)
{
	if(!empty(s) && type(s) != type::OBJECT)
		throw type_error
		{
			"Cannot insert member into JSON of type %s",
			reflect(type(s))
		};

	size_t mctr {0};
	thread_local std::array<member, iov::max_size> mb;
	for(const object::member &m : object{s})
		mb.at(mctr++) = member{m};

	mb.at(mctr++) = m;
	return strung
	{
		mb.data(), mb.data() + mctr
	};
}

ircd::json::strung
ircd::json::remove(const strung &s,
                   const string_view &key)
{
	if(empty(s))
		return s;

	if(type(s) != type::OBJECT)
		throw type_error
		{
			"Cannot remove object member '%s' from JSON of type %s",
			key,
			reflect(type(s))
		};

	size_t mctr {0};
	thread_local std::array<object::member, iov::max_size> mb;
	for(const object::member &m : object{s})
		if(m.first != key)
			mb.at(mctr++) = m;

	return strung
	{
		mb.data(), mb.data() + mctr
	};
}

ircd::json::strung
ircd::json::remove(const strung &s,
                   const size_t &idx)
{
	if(empty(s))
		return s;

	if(type(s) != type::ARRAY)
		throw type_error
		{
			"Cannot remove array element [%zu] from JSON of type %s",
			idx,
			reflect(type(s))
		};

	size_t mctr{0}, i{0};
	thread_local std::array<string_view, iov::max_size> mb;
	for(const string_view &m : array{s})
		if(i++ != idx)
			mb.at(mctr++) = m;

	return strung
	{
		mb.data(), mb.data() + mctr
	};
}

ircd::json::strung::operator
json::array()
const
{
	return string_view{*this};
}

ircd::json::strung::operator
json::object()
const
{
	return string_view{*this};
}

///////////////////////////////////////////////////////////////////////////////
//
// json/vector.h
//

ircd::json::vector::value_type
ircd::json::vector::operator[](const size_t &i)
const
{
	const auto it(find(i));
	return it != end()? *it : object{};
}

ircd::json::vector::value_type
ircd::json::vector::at(const size_t &i)
const
{
	const auto it(find(i));
	if(it == end())
		throw not_found
		{
			"indice %zu", i
		};

	return *it;
}

ircd::json::vector::const_iterator
ircd::json::vector::find(size_t i)
const
{
	auto it(begin());
	for(; it != end() && i; ++it, i--);
	return it;
}

__attribute__((warning("Taking string_view::size() not the count() of vector elements")))
size_t
ircd::json::vector::size()
const
{
	return count();
}

size_t
ircd::json::vector::count()
const
{
	return std::distance(begin(), end());
}


//
// vector::const_iterator
//

bool
ircd::json::operator==(const vector::const_iterator &a, const vector::const_iterator &b)
{
	return a.state == b.state;
}

bool
ircd::json::operator!=(const vector::const_iterator &a, const vector::const_iterator &b)
{
	return a.state != b.state;
}

bool
ircd::json::operator<=(const vector::const_iterator &a, const vector::const_iterator &b)
{
	return a.state <= b.state;
}

bool
ircd::json::operator>=(const vector::const_iterator &a, const vector::const_iterator &b)
{
	return a.state >= b.state;
}

bool
ircd::json::operator<(const vector::const_iterator &a, const vector::const_iterator &b)
{
	return a.state < b.state;
}

bool
ircd::json::operator>(const vector::const_iterator &a, const vector::const_iterator &b)
{
	return a.state > b.state;
}

ircd::json::vector::const_iterator &
ircd::json::vector::const_iterator::operator++()
try
{
	static const parser::rule<string_view> parse_next
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
	throw expectation_failure<parse_error>
	{
		e, start, error_show_max
	};
}

ircd::json::vector::const_iterator
ircd::json::vector::begin()
const try
{
	static const parser::rule<string_view> parse_begin
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
	throw expectation_failure<parse_error>
	{
		e, string_view::data(), error_show_max
	};
}

ircd::json::vector::const_iterator
ircd::json::vector::end()
const
{
	return { string_view::end(), string_view::end() };
}

///////////////////////////////////////////////////////////////////////////////
//
// json/object.h
//

decltype(ircd::json::object::max_recursion_depth)
ircd::json::object::max_recursion_depth
{
	32
};

decltype(ircd::json::object::max_sorted_members)
ircd::json::object::max_sorted_members
{
	iov::max_size
};

std::ostream &
ircd::json::operator<<(std::ostream &s, const object &object)
{
	s << json::strung(object);
	return s;
}

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const object &object)
try
{
	using member_array = std::array<object::member, object::max_sorted_members>;
	using member_arrays = std::array<member_array, object::max_recursion_depth>;
	static_assert(sizeof(member_arrays) == 1_MiB); // yay reentrance .. joy :/

	thread_local member_arrays ma;
	thread_local size_t mctr;
	const size_t mc{mctr++};
	assert(mc < ma.size());
	const unwind uw{[&mc]
	{
		--mctr;
		assert(mctr == mc);
	}};

	size_t i(0);
	auto &m{ma.at(mc)};
	for(auto it(begin(object)); it != end(object); ++it, ++i)
		m.at(i) = *it;

	std::sort(begin(m), begin(m) + i, []
	(const object::member &a, const object::member &b)
	{
		return a.first < b.first;
	});

	return stringify(buf, m.data(), m.data() + i);
}
catch(const std::out_of_range &e)
{
	throw print_error
	{
		"Too many members (%zu) for stringifying JSON object",
		size(object)
	};
}

size_t
ircd::json::serialized(const object &object)
{
	const auto b(begin(object));
	const auto e(end(object));
	assert(!empty(object) || b == e);
	const size_t ret(1 + (b == e));
	return std::accumulate(b, e, ret, []
	(auto ret, const object::member &member)
	{
		return ret += serialized(member) + 1;
	});
}

bool
ircd::json::sorted(const object &object)
{
	auto it(begin(object));
	if(it == end(object))
		return true;

	string_view last{it->first};
	for(++it; it != end(object); last = it->first, ++it)
		if(it->first < last)
			return false;

	return true;
}

size_t
ircd::json::size(const object &object)
{
	return object.size();
}

bool
ircd::json::operator!(const object &object)
{
	return empty(object);
}

bool
ircd::json::empty(const object &object)
{
	return object.empty();
}

//
// object
//

ircd::string_view
ircd::json::object::operator[](const string_view &key)
const
{
	const auto it(find(key));
	return it != end()? it->second : string_view{};
}

ircd::string_view
ircd::json::object::get(const string_view &key,
                        const string_view &def)
const
{
	return get<string_view>(key, def);
}

ircd::json::object::operator std::string()
const
{
	return json::strung(*this);
}

bool
ircd::json::object::has(const string_view &key)
const
{
	return find(key) != end();
}

size_t
ircd::json::object::size()
const
{
	return count();
}

size_t
ircd::json::object::count()
const
{
	return std::distance(begin(), end());
}

bool
ircd::json::object::empty()
const
{
	const string_view &sv{*this};
	assert(sv.size() > 2 || (sv.empty() || sv == empty_object));
	return sv.size() <= 2;
}

ircd::json::object::const_iterator
ircd::json::object::find(const name_hash_t &key)
const
{
	return std::find_if(begin(), end(), [&key]
	(const auto &member)
	{
		return name_hash(member.first) == key;
	});
}

ircd::json::object::const_iterator
ircd::json::object::find(const string_view &key)
const
{
	return std::find_if(begin(), end(), [&key]
	(const auto &member)
	{
		return member.first == key;
	});
}

ircd::json::object::const_iterator
ircd::json::object::begin()
const try
{
	static const auto &ws
	{
		parser.ws
	};

	static const parser::rule<json::object::member> object_member
	{
		parser.name >> -ws >> parser.name_sep >> -ws >> raw[parser.value(0)]
		,"object member"
	};

	static const parser::rule<json::object::member> parse_begin
	{
		-ws >> parser.object_begin >> -ws >> (parser.object_end | object_member) >> -ws
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
	throw expectation_failure<parse_error>
	{
		e, string_view::begin(), error_show_max
	};
}

ircd::json::object::const_iterator
ircd::json::object::end()
const
{
	return { string_view::end(), string_view::end() };
}

//
// object::const_iterator
//

bool
ircd::json::operator==(const object::const_iterator &a, const object::const_iterator &b)
{
	return a.start == b.start;
}

bool
ircd::json::operator!=(const object::const_iterator &a, const object::const_iterator &b)
{
	return a.start != b.start;
}

bool
ircd::json::operator<=(const object::const_iterator &a, const object::const_iterator &b)
{
	return a.start <= b.start;
}

bool
ircd::json::operator>=(const object::const_iterator &a, const object::const_iterator &b)
{
	return a.start >= b.start;
}

bool
ircd::json::operator<(const object::const_iterator &a, const object::const_iterator &b)
{
	return a.start < b.start;
}

bool
ircd::json::operator>(const object::const_iterator &a, const object::const_iterator &b)
{
	return a.start > b.start;
}

ircd::json::object::const_iterator &
ircd::json::object::const_iterator::operator++()
try
{
	static const auto &ws
	{
		parser.ws
	};

	static const parser::rule<json::object::member> member
	{
		parser.name >> -ws >> parser.name_sep >> -ws >> raw[parser.value(0)]
		,"next object member"
	};

	static const parser::rule<json::object::member> parse_next
	{
		(parser.object_end | (parser.value_sep >> -ws >> member)) >> -ws
		,"next object member or end"
	};

	state.first = string_view{};
	state.second = string_view{};
	qi::parse(start, stop, eps > parse_next, state);
	return *this;
}
catch(const qi::expectation_failure<const char *> &e)
{
	throw expectation_failure<parse_error>
	{
		e, start, error_show_max
	};
}

//
// object::member
//

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
	printer(buf, printer.name << printer.name_sep, member.first);
	stringify(buf, member.second);
	const string_view ret
	{
		start, begin(buf)
	};

	assert(serialized(member) == size(ret));
	return ret;
}

size_t
ircd::json::serialized(const object::member &member)
{
	const json::value key{member.first, json::STRING};
	return serialized(key) + 1 + serialized(member.second);
}

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const object::member *const &b,
                      const object::member *const &e)
{
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
	const string_view ret
	{
		start, begin(buf)
	};

	assert(serialized(b, e) == size(ret));
	return ret;
}

size_t
ircd::json::serialized(const object::member *const &begin,
                       const object::member *const &end)
{
	const size_t ret(1 + (begin == end));
	return std::accumulate(begin, end, ret, []
	(auto ret, const object::member &member)
	{
		return ret += serialized(member) + 1;
	});
}

bool
ircd::json::sorted(const object::member *const &begin,
                   const object::member *const &end)
{
	return std::is_sorted(begin, end, []
	(const object::member &a, const object::member &b)
	{
		return a.first < b.first;
	});
}

bool
ircd::json::operator==(const object::member &a, const object::member &b)
{
	return a.first == b.first;
}

bool
ircd::json::operator!=(const object::member &a, const object::member &b)
{
	return a.first != b.first;
}

bool
ircd::json::operator<=(const object::member &a, const object::member &b)
{
	return a.first <= b.first;
}

bool
ircd::json::operator>=(const object::member &a, const object::member &b)
{
	return a.first >= b.first;
}

bool
ircd::json::operator<(const object::member &a, const object::member &b)
{
	return a.first < b.first;
}

bool
ircd::json::operator>(const object::member &a, const object::member &b)
{
	return a.first > b.first;
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

std::ostream &
ircd::json::operator<<(std::ostream &s, const array &a)
{
	s << json::strung(a);
	return s;
}

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const array &v)
{
	if(string_view{v}.empty())
	{
		const char *const start{begin(buf)};
		consume(buf, copy(buf, empty_array));
		const string_view ret{start, begin(buf)};
		assert(serialized(v) == size(ret));
		return ret;
	}

	return array::stringify(buf, begin(v), end(v));
}

size_t
ircd::json::serialized(const array &v)
{
	assert(!empty(v) || (begin(v) == end(v)));
	return array::serialized(begin(v), end(v));
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
	return array::serialized(b, e);
}

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const string_view *const &b,
                      const string_view *const &e)
{
	return array::stringify(buf, b, e);
}

size_t
ircd::json::serialized(const string_view *const &b,
                       const string_view *const &e)
{
	return array::serialized(b, e);
}

size_t
ircd::json::size(const array &array)
{
	return array.size();
}

bool
ircd::json::operator!(const array &array)
{
	return empty(array);
}

bool
ircd::json::empty(const array &array)
{
	return array.empty();
}

//
// array::array
//

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
			json::stringify(buf, element);
		}
	};

	using ircd::buffer::begin;
	char *const start(begin(buf));
	printer(buf, printer.array_begin);
	printer::list_protocol(buf, b, e, print_element);
	printer(buf, printer.array_end);
	const string_view ret
	{
		start, begin(buf)
	};

	using ircd::buffer::size;
	assert(serialized(b, e) == size(ret));
	return ret;
}

template<class it>
size_t
ircd::json::array::serialized(const it &b,
                              const it &e)
{
	const size_t ret(1 + (b == e));
	return std::accumulate(b, e, ret, []
	(auto ret, const string_view &value)
	{
		return ret += json::serialized(value) + 1;
	});
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
	static const auto &ws
	{
		parser.ws
	};

	static const parser::rule<string_view> value
	{
		raw[parser.value(0)]
		,"array element"
	};

	static const parser::rule<string_view> parse_begin
	{
		-ws >> parser.array_begin >> -ws >> (parser.array_end | value) >> -ws
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
	throw expectation_failure<parse_error>
	{
		e, string_view::data(), error_show_max
	};
}

ircd::json::array::const_iterator
ircd::json::array::end()
const
{
	return { string_view::end(), string_view::end() };
}


ircd::string_view
ircd::json::array::operator[](const size_t &i)
const
{
	const auto it(find(i));
	return it != end()? *it : string_view{};
}

ircd::string_view
ircd::json::array::at(const size_t &i)
const
{
	const auto it(find(i));
	if(it == end())
		throw not_found
		{
			"indice %zu", i
		};

	return *it;
}

ircd::json::array::const_iterator
ircd::json::array::find(size_t i)
const
{
	auto it(begin());
	for(; it != end() && i; ++it, i--);
	return it;
}

size_t
ircd::json::array::size()
const
{
	return count();
}

size_t
ircd::json::array::count()
const
{
	return std::distance(begin(), end());
}

bool
ircd::json::array::empty()
const
{
	const string_view &sv{*this};
	// Allow empty objects '{}' to pass this assertion; this function is not
	// a type-check. Some serializers (like browser JS) might give an empty
	// object before it has any context that the set is an array; it doesn't
	// matter here for us.
	assert(sv.size() > 2 || sv.empty() || sv == empty_array || sv == empty_object);
	return sv.size() <= 2;
}

//
// array::const_iterator
//

ircd::json::array::const_iterator &
ircd::json::array::const_iterator::operator++()
try
{
	static const auto &ws
	{
		parser.ws
	};

	static const parser::rule<string_view> value
	{
		raw[parser.value(0)]
		,"array element"
	};

	static const parser::rule<string_view> parse_next
	{
		(parser.array_end | (parser.value_sep >> -ws >> value)) >> -ws
		,"next array element or end"
	};

	state = string_view{};
	qi::parse(start, stop, eps > parse_next, state);
	return *this;
}
catch(const qi::expectation_failure<const char *> &e)
{
	throw expectation_failure<parse_error>
	{
		e, start, error_show_max
	};
}

bool
ircd::json::operator==(const array::const_iterator &a, const array::const_iterator &b)
{
	return a.start == b.start;
}

bool
ircd::json::operator!=(const array::const_iterator &a, const array::const_iterator &b)
{
	return a.start != b.start;
}

bool
ircd::json::operator<=(const array::const_iterator &a, const array::const_iterator &b)
{
	return a.start <= b.start;
}

bool
ircd::json::operator>=(const array::const_iterator &a, const array::const_iterator &b)
{
	return a.start >= b.start;
}

bool
ircd::json::operator<(const array::const_iterator &a, const array::const_iterator &b)
{
	return a.start < b.start;
}

bool
ircd::json::operator>(const array::const_iterator &a, const array::const_iterator &b)
{
	return a.start > b.start;
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
try
{
	using member_array = std::array<const member *, object::max_sorted_members>;
	using member_arrays = std::array<member_array, object::max_recursion_depth>;
	static_assert(sizeof(member_arrays) == 256_KiB);

	thread_local member_arrays ma;
	thread_local size_t mctr;
	const size_t mc{mctr++};
	assert(mc < ma.size());
	const unwind uw{[&mc]
	{
		--mctr;
		assert(mctr == mc);
	}};

	size_t i(0);
	auto &m{ma.at(mc)};
	for(auto it(b); it != e; ++it)
		m.at(i++) = it;

	std::sort(begin(m), begin(m) + i, []
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
	printer::list_protocol(buf, m.data(), m.data() + i, print_member);
	printer(buf, printer.object_end);
	const string_view ret
	{
		start, begin(buf)
	};

	assert(serialized(b, e) == size(ret));
	return ret;
}
catch(const std::out_of_range &)
{
	throw print_error
	{
		"Too many members (%zd) for stringifying", std::distance(b, e)
	};
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
	const size_t ret(1 + (begin == end));
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

bool
ircd::json::sorted(const member *const &begin,
                   const member *const &end)
{
	return std::is_sorted(begin, end, []
	(const member &a, const member &b)
	{
		return a < b;
	});
}

bool
ircd::json::operator<(const member &a, const member &b)
{
	return a.first < b.first;
}

bool
ircd::json::operator!=(const member &a, const member &b)
{
	return a.first != b.first;
}

bool
ircd::json::operator==(const member &a, const member &b)
{
	return a.first == b.first;
}

bool
ircd::json::operator<(const member &a, const string_view &b)
{
	return string_view{a.first.string, a.first.len} < b;
}

bool
ircd::json::operator!=(const member &a, const string_view &b)
{
	return string_view{a.first.string, a.first.len} != b;
}

bool
ircd::json::operator==(const member &a, const string_view &b)
{
	return string_view{a.first.string, a.first.len} == b;
}

///////////////////////////////////////////////////////////////////////////////
//
// json/value.h
//

decltype(ircd::json::value::max_string_size)
ircd::json::value::max_string_size
{
	64_KiB
};

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
	const string_view ret
	{
		start, begin(buf)
	};

	assert(serialized(b, e) == size(ret));
	return ret;
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
			if(!v.string)
			{
				consume(buf, copy(buf, empty_string));
				break;
			}

			const string_view sv
			{
				v.string, v.len
			};

			if(v.serial)
			{
				consume(buf, copy(buf, sv));
				break;
			}

			printer(buf, printer.string, sv);
			break;
		}

		case LITERAL:
		{
			if(v.serial)
				printer(buf, printer.literal, string_view{v});
			else if(v.integer)
				consume(buf, copy(buf, "true"_sv));
			else
				consume(buf, copy(buf, "false"_sv));

			break;
		}

		case OBJECT:
		{
			if(v.serial)
			{
				stringify(buf, json::object{string_view{v}});
				break;
			}

			if(v.object)
			{
				stringify(buf, v.object, v.object + v.len);
				break;
			}

			consume(buf, copy(buf, empty_object));
			break;
		}

		case ARRAY:
		{
			if(v.serial)
			{
				stringify(buf, json::array{string_view{v}});
				break;
			}

			if(v.array)
			{
				stringify(buf, v.array, v.array + v.len);
				break;
			}

			consume(buf, copy(buf, empty_array));
			break;
		}

		case NUMBER:
		{
			if(v.serial)
				//printer(buf, printer.number, string_view{v});
				consume(buf, copy(buf, strip(string_view{v}, ' ')));
			else if(v.floats)
				consume(buf, copy(buf, lex_cast(v.floating)));
			else
				consume(buf, copy(buf, lex_cast(v.integer)));

			break;
		}
	}

	const string_view ret
	{
		start, begin(buf)
	};

	assert(serialized(v) == size(ret));
	return ret;
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
	const size_t ret(1 + (begin == end));
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
		case OBJECT: return
			v.serial?
				serialized(json::object{v}):
				serialized(v.object, v.object + v.len);

		case ARRAY: return
			v.serial?
				serialized(json::array{v}):
				serialized(v.array, v.array + v.len);

		case LITERAL: return
			v.serial?
				v.len:
			v.integer?
				size(literal_true):
				size(literal_false);

		case NUMBER:
		{
			thread_local char test_buffer[256];
			mutable_buffer buf{test_buffer};

			if(v.serial)
				//printer(buf, printer.number, string_view{v});
				return size(strip(string_view{v}, ' '));
			else if(v.floats)
				return size(lex_cast(v.floating));
			else
				return size(lex_cast(v.integer));

			return begin(buf) - test_buffer;
		}

		case STRING:
		{
			if(!v.string)
				return size(empty_string);

			if(v.serial)
				return v.len;

			thread_local char test_buffer[value::max_string_size];
			const string_view sv{v.string, v.len};
			mutable_buffer buf{test_buffer};
			printer(buf, printer.string, sv);
			return begin(buf) - test_buffer;
		}
	};

	throw type_error
	{
		"deciding the size of a type[%u] is undefined", int(v.type)
	};
}

size_t
ircd::json::serialized(const bool &b)
{
	constexpr const size_t t
	{
		strlen("true")
	};

	constexpr const size_t f
	{
		strlen("false")
	};

	return b? t : f;
}

bool
ircd::json::defined(const value &a)
{
	return !a.undefined();
}

enum ircd::json::type
ircd::json::type(const value &a)
{
	return a.type;
}

//
// value::value
//

ircd::json::value::value(const nullptr_t &)
:value
{
	literal_null, type::LITERAL
}
{
}

ircd::json::value::value(const json::object &sv)
:value{sv, OBJECT}
{
}

ircd::json::value::value(const json::array &sv)
:value{sv, ARRAY}
{
}

ircd::json::value::value(const bool &boolean)
:value
{
	boolean? literal_true : literal_false, type::LITERAL
}
{
}

ircd::json::value::value(const uint8_t &integer)
:value{int64_t{integer}}
{
}

ircd::json::value::value(const int8_t &integer)
:value{int64_t{integer}}
{
}

ircd::json::value::value(const uint16_t &integer)
:value{int64_t{integer}}
{
}

ircd::json::value::value(const int16_t &integer)
:value{int64_t{integer}}
{
}

ircd::json::value::value(const uint32_t &integer)
:value{int64_t{integer}}
{
}

ircd::json::value::value(const int32_t &integer)
:value{int64_t{integer}}
{
}

ircd::json::value::value(const int64_t &integer)
:integer{integer}
,len{0}
,type{NUMBER}
,serial{false}
,alloc{false}
,floats{false}
{
}

ircd::json::value::value(const double &floating)
:floating{floating}
,len{0}
,type{NUMBER}
,serial{false}
,alloc{false}
,floats{true}
{
}

ircd::json::value::value(const char *const &str)
:value{string_view{str}}
{
}

ircd::json::value::value(const char *const &str,
                         const enum type &type)
:value{string_view{str}, type}
{
}

ircd::json::value::value(const string_view &sv)
:value{sv, json::type(sv, std::nothrow)}
{
}

ircd::json::value::value(const string_view &sv,
                         const enum type &type)
:string{sv.data()}
,len{sv.size()}
,type{type}
,serial{type == STRING? surrounds(sv, '"') : true}
,alloc{false}
,floats{false}
{
}

ircd::json::value::value(const std::string &s)
:value{s, json::type(s, std::nothrow)}
{
}

ircd::json::value::value(const std::string &s,
                         const enum type &type)
:string{nullptr}
,len{0}
,type{type}
,serial{type == STRING? surrounds(s, '"') : true}
,alloc{true}
,floats{false}
{
	const string_view sv{s};
	create_string(serialized(sv), [&sv]
	(mutable_buffer buf)
	{
		json::stringify(buf, sv);
	});
}

ircd::json::value::value(const struct value *const &array,
                         const size_t &len)
:array{array}
,len{len}
,type{ARRAY}
,serial{false}
,alloc{false}
,floats{false}
{
}

ircd::json::value::value(std::unique_ptr<const struct value[]> &&array,
                         const size_t &len)
:array{array.get()}
,len{len}
,type{ARRAY}
,serial{false}
,alloc{true}
,floats{false}
{
	array.release();
}

ircd::json::value::value(const struct member *const &object,
                         const size_t &len)
:object{object}
,len{len}
,type{OBJECT}
,serial{false}
,alloc{false}
,floats{false}
{}

ircd::json::value::value(std::unique_ptr<const struct member[]> &&object,
                         const size_t &len)
:object{object.get()}
,len{len}
,type{OBJECT}
,serial{false}
,alloc{true}
,floats{false}
{
	object.release();
}

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
			json::stringify(buffer, string_view{other});
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

			create_string(serialized(other), [&other]
			(mutable_buffer &buffer)
			{
				json::stringify(buffer, other);
			});
			break;
		}

		case LITERAL:
		case NUMBER:
			break;
	}
}

ircd::json::value::value(value &&other)
noexcept
:integer{other.integer}
,len{other.len}
,type{other.type}
,serial{other.serial}
,alloc{other.alloc}
,floats{other.floats}
{
	other.alloc = false;
}

ircd::json::value &
ircd::json::value::operator=(value &&other)
noexcept
{
	this->~value();
	integer = other.integer;
	len = other.len;
	type = other.type;
	serial = other.serial;
	alloc = other.alloc;
	other.alloc = false;
	floats = other.floats;
	return *this;
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

	throw type_error
	{
		"value type[%d] is not a string", int(type)
	};
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

	throw type_error
	{
		"value type[%d] is not an int64_t", int(type)
	};
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

	throw type_error
	{
		"value type[%d] is not a float", int(type)
	};
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
			else
				return !bool(integer);
	};

	throw type_error
	{
		"deciding if a type[%u] is falsy is undefined", int(type)
	};
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
			               false;
	};

	throw type_error
	{
		"deciding if a type[%u] is empty is undefined", int(type)
	};
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
			return string == nullptr ||
			       string_view{string, len}.null();

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
			       string? literal_null == string:
			               false;
	};

	throw type_error
	{
		"deciding if a type[%u] is null is undefined", int(type)
	};
}

bool
ircd::json::value::undefined()
const
{
	switch(type)
	{
		case NUMBER:
			return integer == undefined_number;

		case STRING:
			return string_view{string, len}.undefined();

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
			               false;
	};

	throw type_error
	{
		"deciding if a type[%u] is undefined is undefined", int(type)
	};
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
// json/util.h
//

const ircd::string_view ircd::json::literal_null   { "null"   };
const ircd::string_view ircd::json::literal_true   { "true"   };
const ircd::string_view ircd::json::literal_false  { "false"  };
const ircd::string_view ircd::json::empty_string   { "\"\""   };
const ircd::string_view ircd::json::empty_object   { "{}"     };
const ircd::string_view ircd::json::empty_array    { "[]"     };

decltype(ircd::json::undefined_number)
ircd::json::undefined_number
{
	std::numeric_limits<decltype(ircd::json::undefined_number)>::min()
};

static_assert
(
	ircd::json::undefined_number != 0
);

ircd::string_view
ircd::json::escape(const mutable_buffer &buf,
                   const string_view &in)
{
	static const printer::rule<string_view> characters
	{
		*(printer.character)
	};

	mutable_buffer out{buf};
	printer(out, characters, in);
	return string_view
	{
		data(buf), data(out)
	};
}

std::string
ircd::json::why(const string_view &s)
try
{
	valid(s);
	return {};
}
catch(const std::exception &e)
{
	return e.what();
}

bool
ircd::json::valid(const string_view &s,
                  std::nothrow_t)
noexcept try
{
	static const parser::rule<> validator
	{
		parser.value(0) >> eoi
	};

	const char *start(begin(s)), *const stop(end(s));
	return qi::parse(start, stop, validator);
}
catch(...)
{
	return false;
}

void
ircd::json::valid(const string_view &s)
try
{
	static const parser::rule<> validator
	{
		eps > parser.value(0) > eoi
	};

	const char *start(begin(s)), *const stop(end(s));
	qi::parse(start, stop, validator);
}
catch(const qi::expectation_failure<const char *> &e)
{
	throw expectation_failure<parse_error>
	{
		e, begin(s), error_show_max
	};
}

void
ircd::json::valid_output(const string_view &sv,
                         const size_t &expected)
{
	if(unlikely(size(sv) != expected))
		throw print_error
		{
			"stringified:%zu != serialized:%zu: %s",
			size(sv),
			expected,
			sv
		};

	if(unlikely(!valid(sv, std::nothrow))) //note: false alarm when T=json::member
		throw print_error
		{
			"strung %zu bytes: %s: %s",
			size(sv),
			why(sv),
			sv
		};
}

ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const string_view &v)
{
	if(v.empty() && defined(v))
	{
		const char *const start{begin(buf)};
		consume(buf, copy(buf, empty_string));
		const string_view ret{start, begin(buf)};
		assert(serialized(v) == size(ret));
		return ret;
	}

	const json::value value{v};
	return stringify(buf, value);
}

size_t
ircd::json::serialized(const string_view &v)
{
	if(v.empty() && defined(v))
		return size(empty_string);

	const json::value value{v};
	return serialized(value);
}

///////////////////////////////////////////////////////////////////////////////
//
// json/json.h
//

enum ircd::json::type
ircd::json::type(const string_view &buf)
{
	static const auto flag
	{
		qi::skip_flag::dont_postskip
	};

	enum type ret;
	if(!qi::phrase_parse(begin(buf), end(buf), parser.type, parser.WS, flag, ret))
		throw type_error
		{
			"Failed to derive JSON value type from input buffer."
		};

	return ret;
}

enum ircd::json::type
ircd::json::type(const string_view &buf,
                 std::nothrow_t)
{
	static const auto flag
	{
		qi::skip_flag::dont_postskip
	};

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

	throw type_error
	{
		"Unknown type %x", uint(type)
	};
}
