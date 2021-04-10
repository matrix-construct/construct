// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::lex
{
	struct to_float_policy;
	struct to_double_policy;
	struct to_long_double_policy;

	struct from_float_policy;
	struct from_double_policy;
	struct from_long_double_policy;

	template<class T,
	         class... A>
	using rule_to = spirit::qi::rule<const char *, T, A...>
	__attribute__((visibility("internal")));

	template<class T,
	         class... A>
	using rule_from = spirit::karma::rule<char *, T, A...>
	__attribute__((visibility("internal")));

	template<class T,
	         class rule,
	         class exception>
	[[noreturn]]
	static void throw_error(const rule &, const exception &e);

	template<class T,
	         const rule_to<T> &r>
	static bool test(const string_view &) noexcept;

	template<class T,
	         const rule_to<T> &r>
	static T cast(const string_view &);

	template<class T,
	         const rule_from<T()> &r>
	static string_view cast(const mutable_buffer &, T);

	#pragma GCC visibility push(internal)
	extern const rule_to<bool> to_bool;
	extern const rule_to<int8_t> to_int8_t;
	extern const rule_to<uint8_t> to_uint8_t;
	extern const rule_to<short> to_short;
	extern const rule_to<ushort> to_ushort;
	extern const rule_to<int> to_int;
	extern const rule_to<uint> to_uint;
	extern const rule_to<long> to_long;
	extern const rule_to<ulong> to_ulong;
	extern const rule_to<float> to_float;
	extern const rule_to<double> to_double;
	extern const rule_to<long double> to_long_double;
	#pragma GCC visibility pop

	#pragma GCC visibility push(internal)
	extern const rule_from<bool()> from_bool;
	extern const rule_from<int8_t()> from_int8_t;
	extern const rule_from<uint8_t()> from_uint8_t;
	extern const rule_from<short()> from_short;
	extern const rule_from<ushort()> from_ushort;
	extern const rule_from<int()> from_int;
	extern const rule_from<uint()> from_uint;
	extern const rule_from<long()> from_long;
	extern const rule_from<ulong()> from_ulong;
	extern const rule_from<float()> from_float;
	extern const rule_from<double()> from_double;
	extern const rule_from<long double()> from_long_double;
	#pragma GCC visibility pop
}

static_assert
(
	ircd::LEX_CAST_BUFS == 256 && sizeof(ircd::lex_cast_buf_head) == 1,
	"ircd::lex_cast ring buffer integer may not modulate as intended"
);

/// Indicates the next buffer to be used in the ring. See below.
thread_local
decltype(ircd::lex_cast_buf_head)
ircd::lex_cast_buf_head
alignas(16);

/// This is a static "ring buffer" to simplify a majority of lex_cast uses.
/// The returned string_view of data from this buffer is only valid for
/// several more calls to lex_cast before it is overwritten.
thread_local
decltype(ircd::lex_cast_buf)
ircd::lex_cast_buf
alignas(64);

template<class T,
         const ircd::lex::rule_from<T()> &rule>
[[gnu::always_inline]]
inline ircd::string_view
ircd::lex::cast(const mutable_buffer &out,
                T in)
try
{
	constexpr bool truncation
	{
		false
	};

	mutable_buffer buf
	{
		out
	};

	const bool pass
	{
		ircd::generate<truncation>
		(
			buf, rule | spirit::eps, in
		)
	};

	assert(pass);
	return string_view
	{
		data(out), data(buf)
	};
}
catch(const std::exception &e)
{
	throw_error<T>(rule, e);
	__builtin_unreachable();
}

template<class T,
         const ircd::lex::rule_to<T> &rule>
[[gnu::always_inline]]
inline T
ircd::lex::cast(const string_view &s)
try
{
	T ret;
	const char
	*start(begin(s)),
	*const stop(end(s));
	const bool pass
	{
		ircd::parse(start, stop, spirit::expect[rule], ret)
	};

	assert(pass);
	return ret;
}
catch(const std::exception &e)
{
	throw_error<T>(rule, e);
	__builtin_unreachable();
}

template<class T,
         const ircd::lex::rule_to<T> &rule>
[[gnu::always_inline]]
inline bool
ircd::lex::test(const string_view &s)
noexcept
{
	const char
	*start(begin(s)),
	*const stop(end(s));
	return ircd::parse(start, stop, &rule);
}

template<class T,
         class rule,
         class exception>
[[noreturn, gnu::noinline, gnu::cold]]
void
ircd::lex::throw_error(const rule &r,
                       const exception &e)
{
	throw bad_lex_cast
	{
		"Invalid lexical conversion of %s (%s).",
		r.name(),
		demangle(typeid(T).name()),
	};
}

//
// bool
//

decltype(ircd::lex::to_bool)
ircd::lex::to_bool
{
	spirit::qi::bool_
	,"boolean"
};

decltype(ircd::lex::from_bool)
ircd::lex::from_bool
{
	spirit::karma::bool_
	,"boolean"
};

template<> ircd::string_view
ircd::lex_cast(bool i,
               const mutable_buffer &buf)
{
	return lex::cast<bool, lex::from_bool>(buf, i);
}

template<> bool
ircd::lex_cast(const string_view &s)
{
	return s == "true"? true:
	       s == "false"? false:
	       lex::cast<bool, lex::to_bool>(s);
}

template<> bool
ircd::lex_castable<bool>(const string_view &s)
noexcept
{
	return lex::test<bool, lex::to_bool>(s);
}

//
// int8_t
//

decltype(ircd::lex::to_int8_t)
ircd::lex::to_int8_t
{
	spirit::qi::short_
	,"signed byte"
};

decltype(ircd::lex::from_int8_t)
ircd::lex::from_int8_t
{
	spirit::karma::short_
	,"signed byte"
};

template<> ircd::string_view
ircd::lex_cast(int8_t i,
               const mutable_buffer &buf)
{
	return lex::cast<int8_t, lex::from_int8_t>(buf, i);
}

template<> int8_t
ircd::lex_cast(const string_view &s)
{
	return lex::cast<int8_t, lex::to_int8_t>(s);
}

template<> bool
ircd::lex_castable<int8_t>(const string_view &s)
noexcept
{
	return lex::test<int8_t, lex::to_int8_t>(s);
}

//
// uint8_t
//

decltype(ircd::lex::to_uint8_t)
ircd::lex::to_uint8_t
{
	spirit::qi::ushort_
	,"unsigned byte"
};

decltype(ircd::lex::from_uint8_t)
ircd::lex::from_uint8_t
{
	spirit::karma::ushort_
	,"unsigned byte"
};

template<> ircd::string_view
ircd::lex_cast(uint8_t i,
               const mutable_buffer &buf)
{
	return lex::cast<uint8_t, lex::from_uint8_t>(buf, i);
}

template<> uint8_t
ircd::lex_cast(const string_view &s)
{
	return lex::cast<uint8_t, lex::to_uint8_t>(s);
}

template<> bool
ircd::lex_castable<uint8_t>(const string_view &s)
noexcept
{
	return lex::test<uint8_t, lex::to_uint8_t>(s);
}

//
// short
//

decltype(ircd::lex::to_short)
ircd::lex::to_short
{
	spirit::qi::short_
	,"signed short integer"
};

decltype(ircd::lex::from_short)
ircd::lex::from_short
{
	spirit::karma::short_
	,"signed short integer"
};

template<> ircd::string_view
ircd::lex_cast(short i,
               const mutable_buffer &buf)
{
	return lex::cast<short, lex::from_short>(buf, i);
}

template<> short
ircd::lex_cast(const string_view &s)
{
	return lex::cast<short, lex::to_short>(s);
}

template<> bool
ircd::lex_castable<short>(const string_view &s)
noexcept
{
	return lex::test<short, lex::to_short>(s);
}

//
// ushort
//

decltype(ircd::lex::to_ushort)
ircd::lex::to_ushort
{
	spirit::qi::ushort_
	,"unsigned short integer"
};

decltype(ircd::lex::from_ushort)
ircd::lex::from_ushort
{
	spirit::karma::ushort_
	,"unsigned short integer"
};

template<> ircd::string_view
ircd::lex_cast(ushort i,
               const mutable_buffer &buf)
{
	return lex::cast<ushort, lex::from_ushort>(buf, i);
}

template<> ushort
ircd::lex_cast(const string_view &s)
{
	return lex::cast<ushort, lex::to_ushort>(s);
}

template<> bool
ircd::lex_castable<ushort>(const string_view &s)
noexcept
{
	return lex::test<ushort, lex::to_ushort>(s);
}

//
// signed
//

decltype(ircd::lex::to_int)
ircd::lex::to_int
{
	spirit::qi::int_
	,"signed integer"
};

decltype(ircd::lex::from_int)
ircd::lex::from_int
{
	spirit::karma::int_
	,"signed integer"
};

template<> ircd::string_view
ircd::lex_cast(int i,
               const mutable_buffer &buf)
{
	return lex::cast<int, lex::from_int>(buf, i);
}

template<> int
ircd::lex_cast(const string_view &s)
{
	return lex::cast<int, lex::to_int>(s);
}

template<> bool
ircd::lex_castable<int>(const string_view &s)
noexcept
{
	return lex::test<int, lex::to_int>(s);
}

//
// unsigned
//

decltype(ircd::lex::to_uint)
ircd::lex::to_uint
{
	spirit::qi::uint_
	,"unsigned integer"
};

decltype(ircd::lex::from_uint)
ircd::lex::from_uint
{
	spirit::karma::uint_
	,"unsigned integer"
};

template<> ircd::string_view
ircd::lex_cast(uint i,
               const mutable_buffer &buf)
{
	return lex::cast<uint, lex::from_uint>(buf, i);
}

template<> unsigned int
ircd::lex_cast(const string_view &s)
{
	return lex::cast<uint, lex::to_uint>(s);
}

template<> bool
ircd::lex_castable<uint>(const string_view &s)
noexcept
{
	return lex::test<uint, lex::to_uint>(s);
}

//
// long
//

decltype(ircd::lex::to_long)
ircd::lex::to_long
{
	spirit::qi::long_
	,"long integer"
};

decltype(ircd::lex::from_long)
ircd::lex::from_long
{
	spirit::karma::long_
	,"long integer"
};

template<> ircd::string_view
ircd::lex_cast(long i,
               const mutable_buffer &buf)
{
	return lex::cast<long, lex::from_long>(buf, i);
}

template<> long
ircd::lex_cast(const string_view &s)
{
	return lex::cast<long, lex::to_long>(s);
}

template<> bool
ircd::lex_castable<long>(const string_view &s)
noexcept
{
	return lex::test<long, lex::to_long>(s);
}

//
// ulong
//

decltype(ircd::lex::to_ulong)
ircd::lex::to_ulong
{
	spirit::qi::ulong_
	,"long unsigned integer"
};

decltype(ircd::lex::from_ulong)
ircd::lex::from_ulong
{
	spirit::karma::ulong_
	,"long unsigned integer"
};

template<> ircd::string_view
ircd::lex_cast(ulong i,
               const mutable_buffer &buf)
{
	return lex::cast<ulong, lex::from_ulong>(buf, i);
}

template<> unsigned long
ircd::lex_cast(const string_view &s)
{
	return lex::cast<ulong, lex::to_ulong>(s);
}

template<> bool
ircd::lex_castable<ulong>(const string_view &s)
noexcept
{
	return lex::test<ulong, lex::to_ulong>(s);
}

//
// float
//

struct [[gnu::visibility("internal")]]
ircd::lex::to_float_policy
:spirit::qi::real_policies<float>
{
	static const bool
	allow_leading_dot   { false },
	allow_trailing_dot  { false },
	expect_dot          { false };
};

struct [[gnu::visibility("internal")]]
ircd::lex::from_float_policy
:spirit::karma::real_policies<float>
{

};

decltype(ircd::lex::to_float)
ircd::lex::to_float
{
	spirit::qi::real_parser<float, to_float_policy>{}
	,"single floating point precision"
};

decltype(ircd::lex::from_float)
ircd::lex::from_float
{
	spirit::karma::real_generator<float, from_float_policy>{}
	,"single floating point precision"
};

template<> ircd::string_view
ircd::lex_cast(float i,
               const mutable_buffer &buf)
{
	return lex::cast<float, lex::from_float>(buf, i);
}

template<> float
ircd::lex_cast(const string_view &s)
{
	return lex::cast<float, lex::to_float>(s);
}

template<> bool
ircd::lex_castable<float>(const string_view &s)
noexcept
{
	return lex::test<float, lex::to_float>(s);
}

//
// double
//

struct [[gnu::visibility("internal")]]
ircd::lex::to_double_policy
:spirit::qi::real_policies<double>
{
	static const bool
	allow_leading_dot   { false },
	allow_trailing_dot  { false },
	expect_dot          { false };
};

struct [[gnu::visibility("internal")]]
ircd::lex::from_double_policy
:spirit::karma::real_policies<double>
{

};

decltype(ircd::lex::to_double)
ircd::lex::to_double
{
	spirit::qi::real_parser<double, to_double_policy>{}
	,"double floating point precision"
};

decltype(ircd::lex::from_double)
ircd::lex::from_double
{
	spirit::karma::real_generator<double, from_double_policy>{}
	,"double floating point precision"
};

template<> ircd::string_view
ircd::lex_cast(double i,
               const mutable_buffer &buf)
{
	return lex::cast<double, lex::from_double>(buf, i);
}

template<> double
ircd::lex_cast(const string_view &s)
{
	return lex::cast<double, lex::to_double>(s);
}

template<> bool
ircd::lex_castable<double>(const string_view &s)
noexcept
{
	return lex::test<double, lex::to_double>(s);
}

//
// long double
//

struct [[gnu::visibility("internal")]]
ircd::lex::to_long_double_policy
:spirit::qi::real_policies<long double>
{
	static const bool
	allow_leading_dot   { false },
	allow_trailing_dot  { false },
	expect_dot          { false };
};

struct [[gnu::visibility("internal")]]
ircd::lex::from_long_double_policy
:spirit::karma::real_policies<long double>
{

};

decltype(ircd::lex::to_long_double)
ircd::lex::to_long_double
{
	spirit::qi::real_parser<long double, to_long_double_policy>{}
	,"long double floating point precision"
};

decltype(ircd::lex::from_long_double)
ircd::lex::from_long_double
{
	spirit::karma::real_generator<long double, from_long_double_policy>{}
	,"long double floating point precision"
};

template<> ircd::string_view
ircd::lex_cast(long double i,
               const mutable_buffer &buf)
{
	return lex::cast<long double, lex::from_long_double>(buf, i);
}

template<> long double
ircd::lex_cast(const string_view &s)
{
	return lex::cast<long double, lex::to_long_double>(s);
}

template<> bool
ircd::lex_castable<long double>(const string_view &s)
noexcept
{
	return lex::test<long double, lex::to_long_double>(s);
}

//
// seconds
//

template<> ircd::string_view
ircd::lex_cast(seconds i,
               const mutable_buffer &buf)
{
	return lex::cast<time_t, lex::from_long>(buf, i.count());
}

template<> ircd::seconds
ircd::lex_cast(const string_view &s)
{
	return std::chrono::duration<time_t, std::ratio<1L, 1L>>
	(
		lex::cast<time_t, lex::to_long>(s)
	);
}

template<> bool
ircd::lex_castable<ircd::seconds>(const string_view &s)
noexcept
{
	return lex::test<time_t, lex::to_long>(s);
}

//
// milliseconds
//

template<> ircd::string_view
ircd::lex_cast(milliseconds i,
               const mutable_buffer &buf)
{
	return lex::cast<time_t, lex::from_long>(buf, i.count());
}

template<> ircd::milliseconds
ircd::lex_cast(const string_view &s)
{
	return std::chrono::duration<time_t, std::ratio<1L, 1000L>>
	(
		lex::cast<time_t, lex::to_long>(s)
	);
}

template<> bool
ircd::lex_castable<ircd::milliseconds>(const string_view &s)
noexcept
{
	return lex::test<time_t, lex::to_long>(s);
}

//
// microseconds
//

template<> ircd::string_view
ircd::lex_cast(microseconds i,
               const mutable_buffer &buf)
{
	return lex::cast<time_t, lex::from_long>(buf, i.count());
}

template<> ircd::microseconds
ircd::lex_cast(const string_view &s)
{
	return std::chrono::duration<time_t, std::ratio<1L, 1000000L>>
	(
		lex::cast<time_t, lex::to_long>(s)
	);
}

template<> bool
ircd::lex_castable<ircd::microseconds>(const string_view &s)
noexcept
{
	return lex::test<time_t, lex::to_long>(s);
}

//
// nanoseconds
//

template<> ircd::string_view
ircd::lex_cast(nanoseconds i,
               const mutable_buffer &buf)
{
	return lex::cast<time_t, lex::from_long>(buf, i.count());
}

template<> ircd::nanoseconds
ircd::lex_cast(const string_view &s)
{
	return std::chrono::duration<time_t, std::ratio<1L, 1000000000L>>
	(
		lex::cast<time_t, lex::to_long>(s)
	);
}

template<> bool
ircd::lex_castable<ircd::nanoseconds>(const string_view &s)
noexcept
{
	return lex::test<time_t, lex::to_long>(s);
}
