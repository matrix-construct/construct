// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma GCC visibility push(internal)
namespace ircd::json
{
	using namespace ircd::spirit;

	// Instantiations of the grammars
	struct parser extern const parser;
	struct printer extern const printer;

	const size_t &error_show_max {48};
}
#pragma GCC visibility pop

#pragma GCC visibility push(internal)
BOOST_FUSION_ADAPT_STRUCT
(
    ircd::json::member,
    ( decltype(ircd::json::member::first),   first )
    ( decltype(ircd::json::member::second),  second )
)
#pragma GCC visibility pop

#pragma GCC visibility push(internal)
BOOST_FUSION_ADAPT_STRUCT
(
    ircd::json::object::member,
    ( decltype(ircd::json::object::member::first),   first )
    ( decltype(ircd::json::object::member::second),  second )
)
#pragma GCC visibility pop

struct [[gnu::visibility("internal")]]
ircd::json::parser
:qi::grammar<const char *, unused_type>
{
	using it = const char *;

	template<class T = unused_type,
	         class... A>
	using rule = qi::rule<it, T, A...>;

	const rule<> NUL                   { lit('\0')                                          ,"nul" };

	// insignificant whitespaces
	const rule<> SP                    { lit('\x20')                                      ,"space" };
	const rule<> HT                    { lit('\x09')                             ,"horizontal tab" };
	const rule<> CR                    { lit('\x0D')                            ,"carriage return" };
	const rule<> LF                    { lit('\x0A')                                  ,"line feed" };

	// whitespace skipping
	const rule<> WS                    { SP | HT | CR | LF                           ,"whitespace" };
	const rule<> ws                    { *(WS)                                ,"whitespace monoid" };
	const rule<> wsp                   { +(WS)                             ,"whitespace semigroup" };

	// structural
	const rule<> object_begin          { lit('{')                                  ,"object begin" };
	const rule<> object_end            { lit('}')                                    ,"object end" };
	const rule<> array_begin           { lit('[')                                   ,"array begin" };
	const rule<> array_end             { lit(']')                                     ,"array end" };
	const rule<> name_sep              { lit(':')                                      ,"name sep" };
	const rule<> value_sep             { lit(',')                                     ,"value sep" };
	const rule<> escape                { lit('\\')                                       ,"escape" };
	const rule<> quote                 { lit('"')                                         ,"quote" };

	// literal
	const rule<> lit_false             { lit("false")                             ,"literal false" };
	const rule<> lit_true              { lit("true")                               ,"literal true" };
	const rule<> lit_null              { lit("null")                                       ,"null" };
	const rule<> boolean               { lit_true | lit_false                           ,"boolean" };
	const rule<> literal               { lit_true | lit_false | lit_null                ,"literal" };

	// numerical (TODO: exponent)
	const rule<> number
	{
		double_ | long_
		,"number"
	};

	const rule<> utf16_surrogate
	{
		qi::uint_parser
		<
			uint16_t,     // 16 bit width
			16U,          // base-16 (hex)
			4U,           // minimum digits
			4U            // maximum digits
		>{}
		,"UTF-16 surrogate"
	};

	const rule<> unicode
	{
		lit('u') >> utf16_surrogate
		,"escaped unicode"
	};

	const rule<> control
	{
		char_('\x00', '\x1F')
		,"control character"
	};

	// characters that must be escaped
	const rule<> escaped
	{
		quote | escape | control
		,"escaped character"
	};

	// characters that should appear after an escaping solidus
	const rule<> escaper
	{
		char_("btnfr0\"\\") | unicode
		,"escaper"
	};

	// cscapers supersetting the rule above with addl non-canonical chars
	const rule<> escaper_nc
	{
		escaper | lit('/')
		,"escaper"
	};

	const rule<string_view> chars
	{
		raw[*((char_ - escaped) | (escape >> escaper_nc))]
		,"characters"
	};

	const rule<string_view> string
	{
		quote >> chars >> (!escape >> quote)
		,"string"
	};

	// container
	const rule<string_view> name
	{
		string.alias()
		,"name"
	};

	// recursion depth
	_r1_type depth;
	[[noreturn]] static void throws_exceeded();

	rule<unused_type(uint)> member
	{
		name >> -ws >> name_sep >> -ws >> value(depth)
		,"member"
	};

	rule<unused_type(uint)> object
	{
		(eps(depth < json::object::max_recursion_depth) | eps[throws_exceeded]) >>

		object_begin >> -((-ws >> member(depth)) % (-ws >> value_sep)) >> -ws >> object_end
		,"object"
	};

	rule<unused_type(uint)> array
	{
		(eps(depth < json::array::max_recursion_depth) | eps[throws_exceeded]) >>

		array_begin >> -((-ws >> value(depth)) % (-ws >> value_sep)) >> -ws >> array_end
		,"array"
	};

	// primary recursive rule
	rule<unused_type(uint)> value
	{
		(&quote >> string)
		| (&object_begin >> object(depth + 1))
		| (&array_begin >> array(depth + 1))
		| number
		| lit_true
		| lit_false
		| lit_null
		,"value"
	};

	parser()
	:parser::base_type{rule<>{}} // required by spirit
	{
		// synthesized repropagation of recursive rules
		value %= (&quote >> string)
		| (&object_begin >> object(depth + 1))
		| (&array_begin >> array(depth + 1))
		| number
		| lit_true
		| lit_false
		| lit_null
		;
	}
}
const ircd::json::parser;

struct [[gnu::visibility("internal")]]
ircd::json::printer
:karma::grammar<char *, unused_type>
{
	using it = char *;

	template<class T = unused_type,
	         class... A>
	using rule = karma::rule<it, T, A...>;

	const rule<> NUL                   { lit('\0')                                          ,"nul" };

	// insignificant whitespaces
	const rule<> SP                    { lit('\x20')                                      ,"space" };
	const rule<> HT                    { lit('\x09')                             ,"horizontal tab" };
	const rule<> CR                    { lit('\x0D')                            ,"carriage return" };
	const rule<> LF                    { lit('\x0A')                                  ,"line feed" };

	// whitespace skipping
	const rule<> WS                    { SP | HT | CR | LF                           ,"whitespace" };
	const rule<> ws                    { *(WS)                                ,"whitespace monoid" };
	const rule<> wsp                   { +(WS)                             ,"whitespace semigroup" };

	// structural
	const rule<> object_begin          { lit('{')                                  ,"object begin" };
	const rule<> object_end            { lit('}')                                    ,"object end" };
	const rule<> array_begin           { lit('[')                                   ,"array begin" };
	const rule<> array_end             { lit(']')                                     ,"array end" };
	const rule<> name_sep              { lit(':')                                ,"name separator" };
	const rule<> value_sep             { lit(',')                               ,"value separator" };
	const rule<> quote                 { lit('"')                                         ,"quote" };
	const rule<> escape                { lit('\\')                                       ,"escape" };

	// literal
	const rule<string_view> lit_true   { karma::string("true")                     ,"literal true" };
	const rule<string_view> lit_false  { karma::string("false")                   ,"literal false" };
	const rule<string_view> lit_null   { karma::string("null")                     ,"literal null" };
	const rule<string_view> boolean    { lit_true | lit_false                           ,"boolean" };
	const rule<string_view> literal    { lit_true | lit_false | lit_null                ,"literal" };

	// number
	const rule<string_view> number
	{
		double_
		,"number"
	};

	// string
	struct string_state;
	struct character_state;
	using character_prototype = char(const string_view &, string_state &);
	template<class context> static void character_dfa(char &, context &, bool &) noexcept;
	const rule<character_prototype, locals<character_state>> character
	{
		repeat[char_[([](auto &out, auto &gen, auto &ret)
		{
			character_dfa(out, gen, ret);
		})]]
		,"character"
	};

	_a_type _string_state;
	_val_type _string_input;
	const rule<string_view(), locals<string_state>> string
	{
		quote << *(character(_string_input, _string_state)) << quote
		,"string"
	};

	const rule<string_view()> name
	{
		string.alias()
		,"name"
	};

	// primary recursive rule
	rule<string_view> value
	{
		rule<string_view>{}
		,"value"
	};

	rule<json::object::member> member
	{
		rule<json::object::member>{}
		,"member"
	};

	rule<json::object> object
	{
		rule<json::object>{}
		,"object"
	};

	rule<json::array> array
	{
		rule<json::array>{}
		,"array"
	};

	template<class it_a,
	         class it_b,
	         class closure>
	static void list_protocol(mutable_buffer &, it_a begin, const it_b &end, closure&&);

	template<class gen,
	         class... attr>
	bool operator()(mutable_buffer &out, gen&&, attr&&...) const;

	template<class gen>
	bool operator()(mutable_buffer &out, gen&&) const;

	printer()
	:printer::base_type{rule<>{}}
	{
		// synthesized repropagation of recursive rules
		member %= name << name_sep << value;
		object %= object_begin << -(member % value_sep) << object_end;
		array %= array_begin << -(value % value_sep) << array_end;
		value %= (&object << object)
		       | (&array << array)
		       | (&literal << literal)
		       | (&number << number)
		       | string
		       ;
	}
}
const ircd::json::printer;

struct ircd::json::printer::string_state
{
	uint32_t pos {0};
	bool escaped {0};
};

struct ircd::json::printer::character_state
{
	static const char ctrl_tab[0x20][8];

	enum mode
	{
		PASS,
		LEAVE,
		CTRL,
		QUOTE,
		ESCAPE,
		ESCAPED,
	}
	mode {PASS};
	uint8_t pos {0};
};

decltype(ircd::json::printer::character_state::ctrl_tab)
ircd::json::printer::character_state::ctrl_tab
{
	"\\0",
	"\\u0001", "\\u0002", "\\u0003",
	"\\u0004", "\\u0005", "\\u0006",
	"\\u0007",
	"\\b",
	"\\t",
	"\\n",
	"\\u000B",
	"\\f",
	"\\r",
	"\\u000E", "\\u000F", "\\u0010",
	"\\u0011", "\\u0012", "\\u0013",
	"\\u0014", "\\u0015", "\\u0016",
	"\\u0017", "\\u0018", "\\u0019",
	"\\u001A", "\\u001B", "\\u001C",
	"\\u001D", "\\u001E", "\\u001F",
};

template<class gen>
inline void
ircd::json::printer::character_dfa(char &out,
                                   gen &g,
                                   bool &ret)
noexcept
{
	using mode = decltype(character_state::mode);

	#if __has_builtin(__builtin_assume)
		__builtin_assume(ret == true);
	#endif

	const string_view &str(attr_at<1>(g));    // Whole input string.
	const uint8_t &in(attr_at<0>(g));         // Current character in input.

	string_state &sst(attr_at<2>(g));         // Whole input string state.
	auto &st(local_at<0>(g));                 // Current character state.

	out = in;
	st.mode =
		st.mode != mode::PASS?  st.mode:
		sst.escaped?            mode::ESCAPED:
		in < 0x20U?             mode::CTRL:
		in == '"'?              mode::QUOTE:
		in == '\\'?             mode::ESCAPE:
		                        mode::PASS;
	switch(st.mode)
	{
		[[likely]]
		case mode::PASS:
			st.mode = mode::LEAVE;
			break; // mode::PASS

		[[likely]]
		case mode::LEAVE:
			ret = false;
			sst.pos++;
			assert(sst.pos <= str.size());
			break; // mode::LEAVE

		case mode::CTRL:
			out = st.ctrl_tab[in][st.pos++];
			ret &= out != '\0'; // break loop at this iteration
			sst.pos += !ret;
			assert(st.pos <= 8);
			break; // mode::CTRL

		case mode::QUOTE:
			out = "\\\""_sv[st.pos++];
			ret &= out != '\0'; // break loop at this iteration
			sst.pos += !ret;
			assert(st.pos <= 8);
			break; // mode::QUOTE

		case mode::ESCAPE:
			st.mode = sst.pos + 1 < str.size()?
				mode::LEAVE:
				mode::PASS; // must spin if last char of string is esc
			sst.escaped = true;
			assert(sst.pos < str.size());
			break; // mode::ESCAPE

		case mode::ESCAPED:
		{
			const auto ok
			{
				(in == 'u') | (in == '"') | (in == '\\')
			};

			sst.escaped = false;
			out = ok? out: '\\';
			st.mode = ok?
				mode::LEAVE:
				mode::PASS;
			break; // mode::ESCAPED
		}
	}
}

template<class gen,
         class... attr>
[[gnu::visibility("internal")]]
bool
ircd::json::printer::operator()(mutable_buffer &out,
                                gen&& g,
                                attr&&... a)
const
{
	if(unlikely(!ircd::generate(out, std::forward<gen>(g), std::forward<attr>(a)...)))
		throw print_error
		{
			"Failed to generate JSON"
		};

	return true;
}

template<class gen>
[[gnu::visibility("internal")]]
bool
ircd::json::printer::operator()(mutable_buffer &out,
                                gen&& g)
const
{
	if(unlikely(!ircd::generate(out, std::forward<gen>(g))))
		throw print_error
		{
			"Failed to generate JSON"
		};

	return true;
}

template<class it_a,
         class it_b,
         class closure>
[[gnu::visibility("internal")]]
inline void
ircd::json::printer::list_protocol(mutable_buffer &out,
                                   it_a it,
                                   const it_b &end,
                                   closure&& lambda)
{
	if(likely(it != end))
	{
		lambda(out, *it);
		for(++it; it != end; ++it)
		{
			static const auto &printer(json::printer);
			printer(out, printer.value_sep);
			lambda(out, *it);
		}
	}
}

[[gnu::visibility("internal")]]
void
ircd::json::parser::throws_exceeded()
{
	throw recursion_limit
	{
		"Maximum recursion depth exceeded"
	};
}

///////////////////////////////////////////////////////////////////////////////
//
// json/tool.h
//

ircd::json::strung
ircd::json::replace(const strung &s,
                    const json::members &r)
{
	static const auto in
	{
		[](const json::members &r, const object::member &m)
		{
			return std::any_of(begin(r), end(r), [&m]
			(const json::member &r)
			{
				return string_view{r.first} == m.first;
			});
		}
	};

	if(!empty(s) && type(s) != type::OBJECT)
		throw type_error
		{
			"Cannot replace member into JSON of type %s",
			reflect(type(s))
		};

	size_t mctr {0};
	thread_local std::array<member, iov::max_size> mb;
	for(const object::member &m : object{s})
		if(!in(r, m))
			mb.at(mctr++) = member{m};

	for(const json::member &m : r)
		mb.at(mctr++) = m;

	return strung
	{
		mb.data(), mb.data() + mctr
	};
}

ircd::json::strung
ircd::json::replace(const strung &s,
                    const json::member &m_)
{
	if(!empty(s) && type(s) != type::OBJECT)
		throw type_error
		{
			"Cannot replace member into JSON of type %s",
			reflect(type(s))
		};

	size_t mctr {0};
	thread_local std::array<member, iov::max_size> mb;
	for(const object::member &m : object{s})
		if(m.first != string_view{m_.first})
			mb.at(mctr++) = member{m};

	mb.at(mctr++) = m_;
	return strung
	{
		mb.data(), mb.data() + mctr
	};
}

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

void
ircd::json::merge(stack::object &out,
                  const vector &v)
{
	struct val
	{
		//TODO: optimize with std::pmr::monotonic_buffer_resource et al
		std::map<string_view, val, std::less<>> o;
		std::vector<string_view> a;
		string_view v;

		void _merge_object(const json::object &o)
		{
			for(const auto &m : o)
			{
				val &v(this->o[m.first]);
				v.merge(m.second);
			}
		}

		void _merge_array(const json::array &a)
		{
			for(const auto &v : a)
				this->a.emplace_back(v);
		}

		void merge(const string_view &v)
		{
			if(type(v) == json::OBJECT)
				_merge_object(v);
			else if(type(v) == json::ARRAY)
				_merge_array(v);
			else
				this->v = v;
		}

		void _compose_object(json::stack &out, json::stack::object &object) const
		{
			for(const auto &m : o)
			{
				json::stack::member member{object, m.first};
				m.second.compose(out);
			}
		}

		void _compose_object(json::stack &out, json::stack::member &member) const
		{
			json::stack::object object{member};
			_compose_object(out, object);
		}

		void _compose_object(json::stack &out) const
		{
			json::stack::chase c{out, true};
			if(c.m)
				_compose_object(out, *c.m);
			else if(c.o)
				_compose_object(out, *c.o);
		}

		void _compose_array(json::stack &out) const
		{
			json::stack::array array{out};
			for(const auto &v : a)
				array.append(v);
		}

		void _compose_value(json::stack &out) const
		{
			json::stack::chase c{out, true};
			if(c.a)
				c.a->append(v);
			else if(c.m)
				c.m->append(v);
			else
				assert(0);
		}

		void compose(json::stack &out) const
		{
			if(!o.empty())
				_compose_object(out);
			else if(!a.empty())
				_compose_array(out);
			else if(!v.empty())
				_compose_value(out);
		}

		val() = default;
		val(const string_view &v)
		{
			merge(v);
		}
	};

	val top;
	for(const auto &o : v)
		top.merge(o);

	assert(out.s);
	top.compose(*out.s);
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
,appended{std::move(other.appended)}
,flushed{std::move(other.flushed)}
,level{std::move(other.level)}
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
	append(s.size(), [&s]
	(const mutable_buffer &buf)
	{
		assert(ircd::size(buf) >= s.size());
		return ircd::copy(buf, s);
	});
}

void
ircd::json::stack::append(const size_t &expect,
                          const window_buffer::closure &closure)
noexcept try
{
	if(!expect || failed())
		return;

	// Minimum bytes we keep available all times to allow the JSON to close
	// correctly without complication on the user's stack unwind; hinted by
	// the recursion level.
	const size_t buf_min
	{
		level + 8
	};

	// Calculated buffer bytes required.
	const size_t buf_req
	{
		expect + buf_min
	};

	// Since all appends are atomic, we need to have buffer available to print
	// the JSON without having to flush while doing so. If we're low on buffer,
	// this branch triggers a flush. Afterward, if there is still not enough
	// buffer that's an error so the user needs to flush enough when called.
	if(buf_req > buf.remaining())
	{
		if(unlikely(!flusher))
			throw print_panic
			{
				"Insufficient buffer. I need %zu more bytes; you only have %zu left (of %zu).",
				buf_req,
				buf.remaining(),
				size(buf.base)
			};

		if(!flush(true))
			return;

		if(unlikely(buf_req > buf.remaining()))
			throw print_error
			{
				"Insufficient flush. I still need %zu more bytes to buffer.",
				buf_req - buf.remaining(),
			};
	}

	// Print the JSON to the buffer and advance the window pointer
	const const_buffer appended
	{
		buf([&expect, &closure](const mutable_buffer &buf)
		{
			const size_t appended
			{
				closure(buf)
			};

			assert(appended <= size(buf));
			assert(appended == expect);
			return const_buffer
			{
				data(buf), appended
			};
		})
	};

	this->appended += expect;
	assert(size(appended) >= expect);
	assert(this->appended >= size(appended));

	// Conditions to courtesy flush after a sufficiently large dump; when
	// there's no buffer remaining we'll inevitably have to flush; the call
	// is force=true so the flusher must accomplish something.
	if(!buf.remaining())
		flush(true);

	// The high-watermark feature triggers a flush when the buffer has exceeded
	// the threshold from accumulated writes; force is not set to true.
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

	if(unlikely(failed()))
		return false;

	if(!force && buf.consumed() < lowat)
		return false;

	if(!force && cp)
		return false;

	if(unlikely(cp))
	{
		const size_t invalidated
		{
			invalidate_checkpoints()
		};

		log::dwarning
		{
			"Flushing json::stack(%p) bytes:%zu level:%zu checkpoints:%zu",
			this,
			size(buf.completed()),
			level,
			invalidated,
		};
	}

	// The user returns the portion of the buffer they were able to flush
	// rather than forcing them to wait on their sink to flush the whole
	// thing, they can continue with us for a little while more.
	const const_buffer flushed
	{
		flusher(buf.completed())
	};

	assert(data(flushed) == data(buf.completed())); // Can only flush front sry
	this->flushed += size(flushed);
	buf.shift(size(flushed));
	return true;
}
catch(...)
{
	assert(!this->eptr);
	this->eptr = std::current_exception();
	return false;
}

size_t
ircd::json::stack::invalidate_checkpoints()
{
	size_t ret(0);
	for(auto cp(this->cp); cp; cp = cp->pc, ++ret)
		cp->s = nullptr;

	this->cp = nullptr;
	return ret;
}

void
ircd::json::stack::clear()
{
	const size_t rewound
	{
		rewind(buf.consumed())
	};

	this->eptr = std::exception_ptr{};
}

size_t
ircd::json::stack::rewind(const size_t &bytes)
{
	const size_t before
	{
		buf.consumed()
	};

	assert(appended >= before);
	const size_t &amount
	{
		std::min(bytes, before)
	};

	assert(appended >= amount);
	const size_t after
	{
		size(buf.rewind(amount))
	};

	assert(before >= after);
	assert(before - after == amount);
	appended -= amount;
	assert(appended >= after);
	return amount;
}

ircd::string_view
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
ircd::json::stack::failed()
const
{
	return bool(eptr);
}

bool
ircd::json::stack::done()
const
{
	assert((opened() && level) || !level);
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
	s.append('{');
	s.level++;
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
	s->append('{');
	pm->vc |= true;
	s->level++;
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
	s->append('{');
	pm.vc |= true;
	s->level++;
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
		s->append(',');

	s->append('{');
	s->level++;
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

#if defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("no-lifetime-dse")))
#endif
ircd::json::stack::object::~object()
noexcept
{
	if(!s)
	{
		assert(!m.s);
		return; // std::move()'ed away
	}

	const unwind _{[this]
	{
		// Allows ~dtor to be called to close the JSON manually
		s = nullptr;
	}};

	assert(cm == nullptr);
	s->append('}');
	s->level--;

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
	s.append('[');
	s.level++;
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
	s->append('[');
	pm->vc |= true;
	s->level++;
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
		s->append(',');

	s->append('[');
	s->level++;
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
	s->append('[');
	pm.vc |= true;
	s->level++;
}

#if defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("no-lifetime-dse")))
#endif
ircd::json::stack::array::~array()
noexcept
{
	if(!s)
	{
		assert(!m.s);
		return; // std::move()'ed away
	}

	const unwind _{[this]
	{
		// Allows ~dtor to be called to close the JSON manually
		s = nullptr;
	}};

	assert(co == nullptr);
	assert(ca == nullptr);
	s->append(']');
	s->level--;

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
		s->append(',');

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
		s->append(',');

	static const printer::rule<string_view> rule
	{
		printer.name << printer.name_sep
	};

	char tmp[512];
	mutable_buffer buf{tmp};
	if(unlikely(!printer(buf, rule, name)))
		throw error
		{
			"member name overflow: max size is under %zu",
			sizeof(tmp)
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

#if defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("no-lifetime-dse")))
#endif
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

ircd::json::stack::checkpoint::checkpoint(stack &s,
                                          const bool &committed,
                                          const bool &exception_rollback)
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
,committed
{
	committed
}
,exception_rollback
{
	exception_rollback
}
{
	s.cp = this;
}

ircd::json::stack::checkpoint::~checkpoint()
noexcept
{
	if(std::uncaught_exceptions() && exception_rollback)
		committing(false);

	if(!committing())
		rollback();

	if(!s)
		return;

	assert(s->cp == this);
	s->cp = pc;

	// Certain uses of json::stack in loops might create and destroy
	// checkpoints without any appends between their lifetimes. This results
	// in the buffer filling up and inevitably force-flushing under an unlucky
	// checkpoint (which is bad). This non-forced flush here prevents that once
	// the buffer reaches the low-watermark and there is no parent checkpoint.
	if(committing())
		s->flush();
}

bool
ircd::json::stack::checkpoint::rollback()
{
	if(!s)
	{
		log::dwarning
		{
			"Attempting rollback of invalidated checkpoint(%p).",
			this,
		};

		return false;
	}

	assert(point <= s->buf.consumed());
	s->rewind(s->buf.consumed() - point);

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

	committing(true);
	return true;
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
	static const auto addressof //TODO: XXX
	{
		[](const member &m) noexcept
		{
			return std::addressof(m);
		}
	};

	static const auto less_member
	{
		[](const member *const &a, const member *const &b) noexcept
		{
			return *a < *b;
		}
	};

	static const auto print_member
	{
		[](mutable_buffer &buf, const member *const &m)
		{
			printer(buf, printer.name << printer.name_sep, m->first);
			stringify(buf, m->second);
		}
	};

	thread_local const member *m[iov::max_size];
	const ctx::critical_assertion ca;
	if(unlikely(size_t(iov.size()) > iov.max_size))
		throw iov::oversize
		{
			"IOV has %zd members but maximum is %zu",
			iov.size(),
			iov.max_size
		};

	const auto start(begin(buf));
	std::transform(std::begin(iov), std::end(iov), m, addressof);
	std::sort(m, m + iov.size(), less_member);
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

namespace ircd::json
{
	[[gnu::visibility("internal")]]
	extern const parser::rule<string_view>
	vector_object,
	vector_next_parse,
	vector_begin_parse;
}

decltype(ircd::json::vector_object)
ircd::json::vector_object
{
	raw[parser.object(0)]
	,"vector object"
};

decltype(ircd::json::vector_next_parse)
ircd::json::vector_next_parse
{
	expect[eoi | (vector_object >> -parser.ws)]
	,"next object vector element or end"
};

decltype(ircd::json::vector_begin_parse)
ircd::json::vector_begin_parse
{
	expect[-parser.ws >> (eoi | (vector_object >> -parser.ws))]
	,"object vector element"
};

bool
ircd::json::operator!(const vector &v)
{
	return v.empty();
}

size_t
ircd::json::size(const vector &v)
{
	return v.size();
}

bool
ircd::json::empty(const vector &v)
{
	return v.empty();
}

//
// vector::vector
//

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

ircd::json::vector::operator
bool()
const
{
	return !empty();
}

bool
ircd::json::vector::empty()
const
{
	const string_view &sv
	{
		*static_cast<const string_view *>(this)
	};

	return sv.empty();
}

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

ircd::json::vector::const_iterator
ircd::json::vector::begin()
const try
{
	const_iterator ret
	{
		string_view::begin(), string_view::end()
	};

	string_view &state(ret.state);
	qi::parse(ret.start, ret.stop, vector_begin_parse, state);
	return ret;
}
catch(const qi::expectation_failure<const char *> &e)
{
	throw expectation_failure<parse_error>
	{
		e, string_view::data(), error_show_max
	};
}

//
// vector::const_iterator::const_iterator
//

ircd::json::vector::const_iterator &
ircd::json::vector::const_iterator::operator++()
try
{
	this->state = {};
	string_view &state(this->state);
	qi::parse(start, stop, vector_next_parse, state);
	return *this;
}
catch(const qi::expectation_failure<const char *> &e)
{
	throw expectation_failure<parse_error>
	{
		e, start, error_show_max
	};
}

///////////////////////////////////////////////////////////////////////////////
//
// json/object.h
//

namespace ircd::json
{
	[[gnu::visibility("internal")]]
	extern const parser::rule<object::member>
	object_member,
	object_next,
	object_begin,
	object_next_parse,
	object_begin_parse;
}

decltype(ircd::json::object::max_recursion_depth)
ircd::json::object::max_recursion_depth
{
	96
};

decltype(ircd::json::object::max_sorted_members)
ircd::json::object::max_sorted_members
{
	iov::max_size
};

decltype(ircd::json::object_member)
ircd::json::object_member
{
	parser.name >> -parser.ws >> parser.name_sep >> -parser.ws >> raw[parser.value(0)]
	,"object member"
};

decltype(ircd::json::object_next)
ircd::json::object_next
{
	(parser.value_sep >> -parser.ws >> object_member) |
	(parser.object_end >> -parser.ws >> eoi)
	,"object member"
};

decltype(ircd::json::object_begin)
ircd::json::object_begin
{
	parser.object_begin >> -parser.ws >> (parser.object_end | object_member)
	,"object"
};

decltype(ircd::json::object_next_parse)
ircd::json::object_next_parse
{
	expect[object_next >> -parser.ws]
	,"object increment"
};

decltype(ircd::json::object_begin_parse)
ircd::json::object_begin_parse
{
	expect[-parser.ws >> (eoi | (object_begin >> -parser.ws))]
	,"object begin"
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
	static_assert(sizeof(member_arrays) == 3_MiB); // yay reentrance .. joy :/

	thread_local member_arrays ma;
	thread_local size_t mctr;
	const size_t mc{mctr};
	const scope_count _mc{mctr};
	assert(mc < ma.size());

	size_t i(0);
	auto &m{ma.at(mc)};
	for(auto it(begin(object)); it != end(object); ++it, ++i)
		m.at(i) = *it;

	std::sort(begin(m), begin(m) + i, []
	(const object::member &a, const object::member &b) noexcept
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
	const_iterator ret
	{
		string_view::begin(), string_view::end()
	};

	qi::parse(ret.start, ret.stop, object_begin_parse, ret.state);
	return ret;
}
catch(const qi::expectation_failure<const char *> &e)
{
	const auto type
	{
		json::type(*this)
	};

	if(type != type::OBJECT)
		throw type_error
		{
			"Expected JSON type OBJECT, not %s.", reflect(type)
		};

	throw expectation_failure<parse_error>
	{
		e, string_view::begin(), error_show_max
	};
}

//
// object::const_iterator
//

ircd::json::object::const_iterator &
ircd::json::object::const_iterator::operator++()
try
{
	assert(start != stop);

	state = {};
	qi::parse(start, stop, object_next_parse, state);
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
	assert(!surrounds(member.first, '"'));
	consume(buf, copy(buf, '"'));
	consume(buf, copy(buf, member.first));
	consume(buf, copy(buf, "\":"_sv));
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
	return 1 + size(member.first) + 1 + 1 + serialized(member.second);
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

///////////////////////////////////////////////////////////////////////////////
//
// json/array.h
//

namespace ircd::json
{
	[[gnu::visibility("internal")]]
	extern const parser::rule<string_view>
	array_value,
	array_next,
	array_begin,
	array_next_parse,
	array_begin_parse;
}

decltype(ircd::json::array::max_recursion_depth)
ircd::json::array::max_recursion_depth
{
	96
};

decltype(ircd::json::array_value)
ircd::json::array_value
{
	raw[parser.value(0)]
	,"array element"
};

decltype(ircd::json::array_next)
ircd::json::array_next
{
	parser.array_end | (parser.value_sep >> -parser.ws >> array_value)
	,"next array element"
};

decltype(ircd::json::array_begin)
ircd::json::array_begin
{
	parser.array_begin >> -parser.ws >> (parser.array_end | array_value)
	,"array begin element"
};

decltype(ircd::json::array_next_parse)
ircd::json::array_next_parse
{
	expect[array_next >> -parser.ws]
	,"array next"
};

decltype(ircd::json::array_begin_parse)
ircd::json::array_begin_parse
{
	expect[-parser.ws >> (eoi | (array_begin >> -parser.ws))]
	,"array begin"
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
	if(likely(!string_view{v}.empty()))
		return array::stringify(buf, begin(v), end(v));

	const char *const start{begin(buf)};
	consume(buf, copy(buf, empty_array));
	const string_view ret{start, begin(buf)};
	assert(serialized(v) == size(ret));
	return ret;
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
	const_iterator ret
	{
		string_view::begin(), string_view::end()
	};

	qi::parse(ret.start, ret.stop, array_begin_parse, ret.state);
	return ret;
}
catch(const qi::expectation_failure<const char *> &e)
{
	throw expectation_failure<parse_error>
	{
		e, string_view::data(), error_show_max
	};
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
	assert(start != stop);

	state = string_view{};
	qi::parse(start, stop, array_next_parse, state);
	return *this;
}
catch(const qi::expectation_failure<const char *> &e)
{
	throw expectation_failure<parse_error>
	{
		e, start, error_show_max
	};
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
	using member_array = std::array<const member *, object::max_sorted_members>;
	using member_arrays = std::array<member_array, object::max_recursion_depth>;
	static_assert(sizeof(member_arrays) == 768_KiB);

	static const auto less_member
	{
		[](const member *const &a, const member *const &b)
		noexcept
		{
			return *a < *b;
		}
	};

	static const auto print_member
	{
		[](mutable_buffer &buf, const member *const &m)
		{
			assert(type(m->first) == STRING);
			stringify(buf, m->first);
			printer(buf, printer.name_sep);
			stringify(buf, m->second);
		}
	};

	const size_t count(std::distance(b, e));
	if(unlikely(count > object::max_sorted_members))
		throw print_error
		{
			"json::member vector of %zu exceeds maximum %zu.",
			count,
			object::max_sorted_members,
		};

	thread_local member_arrays ma;
	thread_local size_t mctr;
	const size_t mc{mctr};
	const scope_count _mc{mctr};
	assert(mc < ma.size());
	auto &m(ma.at(mc));
	for(size_t i(0); i < count; ++i)
		m[i] = b + i;

	std::sort(begin(m), begin(m) + count, less_member);
	const char *const start(begin(buf));
	printer(buf, printer.object_begin);
	printer::list_protocol(buf, begin(m), begin(m) + count, print_member);
	printer(buf, printer.object_end);
	const string_view ret
	{
		start, begin(buf)
	};

	assert(serialized(b, e) == size(ret));
	return ret;
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
// json/string.h
//

ircd::const_buffer
ircd::json::unescape(const mutable_buffer &buf,
                     const string &in)
{
	throw ircd::not_implemented{};
}

ircd::json::string
ircd::json::escape(const mutable_buffer &buf,
                   const string_view &in)
{
	mutable_buffer out{buf};
	const bool ok
	{
		printer(out, printer.string, in)
	};

	const string_view ret
	{
		data(buf), data(out)
	};

	assert(ok);
	return ret;
}

///////////////////////////////////////////////////////////////////////////////
//
// json/value.h
//

decltype(ircd::json::value::max_string_size)
ircd::json::value::max_string_size;

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

			if(unlikely(v.len > value::max_string_size))
				throw print_error
				{
					"String value cannot have length:%zu which exceeds limit:%zu",
					v.len,
					value::max_string_size,
				};

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
	static constexpr const size_t t
	{
		_constexpr_strlen("true")
	};

	static constexpr const size_t f
	{
		_constexpr_strlen("false")
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
	return static_cast<enum json::type>(a.type);
}

//
// value::value
//

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
	(mutable_buffer &buffer)
	{
		json::stringify(buffer, sv);
	});
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
			json::stringify(buffer, other);
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

ircd::json::value &
ircd::json::value::operator=(value &&other)
noexcept
{
	this->~value();
	new (this) value(std::move(other));
	assert(other.alloc == false);
	return *this;
}

ircd::json::value &
ircd::json::value::operator=(const value &other)
{
	this->~value();
	new (this) value(other);
	return *this;
}

[[gnu::hot]]
ircd::json::value::~value()
noexcept
{
	if(alloc) switch(serial? STRING : static_cast<enum type>(type))
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
			return string?  !len || (serial && string_view{string, len} == empty_string):
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
			return !string || !len || (serial && string_view{string, len} == empty_string);

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

	if(unlikely(max > max_string_size))
		throw print_panic
		{
			"Value cannot have string length:%zu which exceeds limit:%zu",
			max,
			max_string_size,
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
ircd::json::operator<(const value &a, const value &b)
{
	if(type(a) == type(b)) switch(type(b))
	{
		case NUMBER:
			assert(!a.serial && !b.serial);
			assert(a.floats == b.floats);
			return b.floats?
				a.floating < b.floating:
				a.integer < b.integer;

		case STRING:
			return static_cast<string_view>(a) < static_cast<string_view>(b);

		default:
			break;
	}

	throw type_error
	{
		"Cannot compare type[%u] %s to type[%u] %s",
		uint(type(a)),
		reflect(type(a)),
		uint(type(b)),
		reflect(type(b)),
	};
}

bool
ircd::json::operator==(const value &a, const value &b)
{
	if(a.serial && b.serial)
		return string_view(a) == string_view(b);

	if(type(a) == type(b)) switch(type(b))
	{
		case NUMBER:
			assert(!a.serial && !b.serial);
			assert(!a.floats && !b.floats);
			if(unlikely(a.floats || b.floats))
				break;

			return a.integer == b.integer;

		case STRING:
			return static_cast<string_view>(a) == static_cast<string_view>(b);

		default:
			break;
	}

	throw type_error
	{
		"Cannot compare type[%u] %s to type[%u] %s",
		uint(type(a)),
		reflect(type(a)),
		uint(type(b)),
		reflect(type(b)),
	};
}

///////////////////////////////////////////////////////////////////////////////
//
// json/util.h
//

namespace ircd::json
{
	[[gnu::visibility("internal")]]
	extern const parser::rule<>
	validation,
	validation_expect;
}

decltype(ircd::json::validation)
ircd::json::validation
{
	parser.value(0) >> eoi
};

decltype(ircd::json::validation_expect)
ircd::json::validation_expect
{
	expect[validation]
};

const ircd::string_view
ircd::json::literal_null   { "null"   },
ircd::json::literal_true   { "true"   },
ircd::json::literal_false  { "false"  },
ircd::json::empty_string   { "\"\""   },
ircd::json::empty_object   { "{}"     },
ircd::json::empty_array    { "[]"     };

decltype(ircd::json::undefined_number)
ircd::json::undefined_number
{
	std::numeric_limits<decltype(ircd::json::undefined_number)>::min()
};

static_assert
(
	ircd::json::undefined_number != 0
);

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
	const char *start(begin(s)), *const stop(end(s));
	return qi::parse(start, stop, validation);
}
catch(...)
{
	assert(false);
	return false;
}

void
ircd::json::valid(const string_view &s)
try
{
	const char *start(begin(s)), *const stop(end(s));
	const bool ret
	{
		qi::parse(start, stop, validation_expect)
	};

	assert(ret);
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
		throw print_panic
		{
			"stringified:%zu != serialized:%zu :%s",
			size(sv),
			expected,
			sv
		};

	if(unlikely(!valid(sv, std::nothrow))) //note: false alarm when T=json::member
		throw print_panic
		{
			"strung %zu bytes :%s :%s",
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
// json/type.h
//

namespace ircd::json
{
	[[gnu::visibility("internal")]]
	extern const parser::rule<>
	type_parse_is[5],
	type_parse_is_strict[5];

	[[gnu::visibility("internal")]]
	extern const parser::rule<enum json::type>
	type_parse,
	type_parse_strict;
}

//TODO: XXX array designated initializers
decltype(ircd::json::type_parse_is)
ircd::json::type_parse_is
{
	{ -parser.ws >> parser.quote                         },
	{ -parser.ws >> parser.object_begin                  },
	{ -parser.ws >> parser.array_begin                   },
	{ -parser.ws >> parser.number >> -parser.ws >> eoi   },
	{ -parser.ws >> parser.literal >> -parser.ws >> eoi  },
};

//TODO: XXX array designated initializers
decltype(ircd::json::type_parse_is_strict)
ircd::json::type_parse_is_strict
{
	{ -parser.ws >> &parser.quote >> parser.string >> -parser.ws >> eoi            },
	{ -parser.ws >> &parser.object_begin >> parser.object(0) >> -parser.ws >> eoi  },
	{ -parser.ws >> &parser.array_begin >> parser.array(0) >> -parser.ws >> eoi    },
	{ -parser.ws >> parser.number >> -parser.ws >> eoi                             },
	{ -parser.ws >> parser.literal >> -parser.ws >> eoi                            },
};

decltype(ircd::json::type_parse)
ircd::json::type_parse
{
	(omit[type_parse_is[json::STRING]]  >> attr(json::STRING))  |
	(omit[type_parse_is[json::OBJECT]]  >> attr(json::OBJECT))  |
	(omit[type_parse_is[json::ARRAY]]   >> attr(json::ARRAY))   |
	(omit[type_parse_is[json::NUMBER]]  >> attr(json::NUMBER))  |
	(omit[type_parse_is[json::LITERAL]] >> attr(json::LITERAL))
	,"type check"
};

decltype(ircd::json::type_parse_strict)
ircd::json::type_parse_strict
{
	(omit[type_parse_is_strict[json::STRING]]  >> attr(json::STRING))  |
	(omit[type_parse_is_strict[json::OBJECT]]  >> attr(json::OBJECT))  |
	(omit[type_parse_is_strict[json::ARRAY]]   >> attr(json::ARRAY))   |
	(omit[type_parse_is_strict[json::NUMBER]]  >> attr(json::NUMBER))  |
	(omit[type_parse_is_strict[json::LITERAL]] >> attr(json::LITERAL))
	,"type check strict"
};

bool
ircd::json::type(const string_view &buf,
                 const enum type &type)
{
	const bool ret
	{
		qi::parse(begin(buf), end(buf), type_parse_is[type])
	};

	return ret;
}

bool
ircd::json::type(const string_view &buf,
                 const enum type &type,
                 strict_t)
{
	const bool ret
	{
		qi::parse(begin(buf), end(buf), type_parse_is_strict[type])
	};

	return ret;
}

enum ircd::json::type
ircd::json::type(const string_view &buf)
{
	enum type ret;
	if(!qi::parse(begin(buf), end(buf), type_parse, ret))
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
	enum type ret;
	if(!qi::parse(begin(buf), end(buf), type_parse, ret))
		return STRING;

	return ret;
}

enum ircd::json::type
ircd::json::type(const string_view &buf,
                 strict_t)
{
	enum type ret;
	if(!qi::parse(begin(buf), end(buf), type_parse_strict, ret))
		throw type_error
		{
			"Failed to derive JSON value type from input buffer."
		};

	return ret;
}

enum ircd::json::type
ircd::json::type(const string_view &buf,
                 strict_t,
                 std::nothrow_t)
{
	enum type ret;
	if(!qi::parse(begin(buf), end(buf), type_parse_strict, ret))
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
