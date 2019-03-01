// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

/// !!! NOTE !!!
///
/// Many functions implemented here need to be replaced with karma generators
/// similar to ircd::fmt. Both the boost and std lexical conversions are an
/// order of magnitude slower than the formal generators. Some tokenizations
/// can also be replaced.
///

#include <boost/lexical_cast.hpp>

decltype(ircd::LEX_CAST_BUFS)
ircd::LEX_CAST_BUFS
{
	64 // plenty
};

decltype(ircd::LEX_CAST_BUFSIZE)
ircd::LEX_CAST_BUFSIZE
{
	64
};

namespace ircd
{
	/// This is a static "ring buffer" to simplify a majority of lex_cast uses.
	/// If the lex_cast has binary input and string output, and no user buffer
	/// is supplied, the next buffer here will be used instead. The returned
	/// string_view of data from this buffer is only valid for several more
	/// calls to lex_cast before it is overwritten.
	thread_local char lex_cast_buf[LEX_CAST_BUFS][LEX_CAST_BUFSIZE];
	thread_local uint lex_cast_cur;

	[[noreturn]] static void throw_bad_lex_cast(const boost::bad_lexical_cast &, const std::type_info &);
	template<size_t N, class T> static string_view _lex_cast(const T &i, mutable_buffer buf);
	template<class T> static T _lex_cast(const string_view &s);
}

/// Internal template providing conversions from a number to a string;
/// potentially using the ring buffer if no user buffer is supplied.
template<size_t N,
         class T>
ircd::string_view
ircd::_lex_cast(const T &i,
                mutable_buffer buf)
try
{
	using array = std::array<char, N>;

	if(!buf)
	{
		buf = lex_cast_buf[lex_cast_cur++];
		lex_cast_cur %= LEX_CAST_BUFS;
	}

	assert(size(buf) >= N);
	auto &a(*reinterpret_cast<array *>(data(buf)));
	a = boost::lexical_cast<array>(i);
	return { data(buf), strnlen(data(buf), size(buf)) };
}
catch(const boost::bad_lexical_cast &e)
{
	throw_bad_lex_cast(e, typeid(T));
}

/// Internal template providing conversions from a string to a number;
/// the native object is returned directly; no ring buffer is consumed.
template<class T>
T
ircd::_lex_cast(const string_view &s)
try
{
	return boost::lexical_cast<T>(data(s), size(s));
}
catch(const boost::bad_lexical_cast &e)
{
	throw_bad_lex_cast(e, typeid(T));
}

void
ircd::throw_bad_lex_cast(const boost::bad_lexical_cast &e,
                         const std::type_info &t)
{
	throw ircd::bad_lex_cast
	{
		"%s: %s", e.what(), demangle(t.name())
	};
}

template<> ircd::string_view
ircd::lex_cast(bool i,
               const mutable_buffer &buf)
{
	static const size_t MAX(8);
	return _lex_cast<MAX>(i, buf);
}

template<> ircd::string_view
ircd::lex_cast(int8_t i,
               const mutable_buffer &buf)
{
	static const size_t MAX(8);
	return _lex_cast<MAX>(i, buf);
}

template<> ircd::string_view
ircd::lex_cast(uint8_t i,
               const mutable_buffer &buf)
{
	static const size_t MAX(8);
	return _lex_cast<MAX>(i, buf);
}

template<> ircd::string_view
ircd::lex_cast(short i,
               const mutable_buffer &buf)
{
	static const size_t MAX(8);
	return _lex_cast<MAX>(i, buf);
}

template<> ircd::string_view
ircd::lex_cast(ushort i,
               const mutable_buffer &buf)
{
	static const size_t MAX(8);
	return _lex_cast<MAX>(i, buf);
}

template<> ircd::string_view
ircd::lex_cast(int i,
               const mutable_buffer &buf)
{
	static const size_t MAX(16);
	return _lex_cast<MAX>(i, buf);
}

template<> ircd::string_view
ircd::lex_cast(uint i,
               const mutable_buffer &buf)
{
	static const size_t MAX(16);
	return _lex_cast<MAX>(i, buf);
}

template<> ircd::string_view
ircd::lex_cast(long i,
               const mutable_buffer &buf)
{
	static const size_t MAX(32);
	return _lex_cast<MAX>(i, buf);
}

template<> ircd::string_view
ircd::lex_cast(ulong i,
               const mutable_buffer &buf)
{
	static const size_t MAX(32);
	return _lex_cast<MAX>(i, buf);
}

template<> ircd::string_view
ircd::lex_cast(double i,
               const mutable_buffer &buf)
{
	static const size_t MAX(64);
	return _lex_cast<MAX>(i, buf);
}

template<> ircd::string_view
ircd::lex_cast(long double i,
               const mutable_buffer &buf)
{
	static const size_t MAX(64);
	return _lex_cast<MAX>(i, buf);
}

template<> ircd::string_view
ircd::lex_cast(nanoseconds i,
               const mutable_buffer &buf)
{
	static const size_t MAX(64);
	return _lex_cast<MAX>(i.count(), buf);
}

template<> ircd::string_view
ircd::lex_cast(microseconds i,
               const mutable_buffer &buf)
{
	static const size_t MAX(64);
	return _lex_cast<MAX>(i.count(), buf);
}

template<> ircd::string_view
ircd::lex_cast(milliseconds i,
               const mutable_buffer &buf)
{
	static const size_t MAX(64);
	return _lex_cast<MAX>(i.count(), buf);
}

template<> ircd::string_view
ircd::lex_cast(seconds i,
               const mutable_buffer &buf)
{
	static const size_t MAX(64);
	return _lex_cast<MAX>(i.count(), buf);
}

template<> bool
ircd::lex_cast(const string_view &s)
{
	return s == "true"? true:
	       s == "false"? false:
	       _lex_cast<bool>(s);
}

template<> int8_t
ircd::lex_cast(const string_view &s)
{
	return _lex_cast<char>(s);
}

template<> uint8_t
ircd::lex_cast(const string_view &s)
{
	return _lex_cast<unsigned char>(s);
}

template<> short
ircd::lex_cast(const string_view &s)
{
	return _lex_cast<short>(s);
}

template<> unsigned short
ircd::lex_cast(const string_view &s)
{
	return _lex_cast<unsigned short>(s);
}

template<> int
ircd::lex_cast(const string_view &s)
{
	return _lex_cast<int>(s);
}

template<> unsigned int
ircd::lex_cast(const string_view &s)
{
	return _lex_cast<unsigned int>(s);
}

template<> long
ircd::lex_cast(const string_view &s)
{
	return _lex_cast<long>(s);
}

template<> unsigned long
ircd::lex_cast(const string_view &s)
{
	return _lex_cast<unsigned long>(s);
}

template<> double
ircd::lex_cast(const string_view &s)
{
	return _lex_cast<double>(s);
}

template<> long double
ircd::lex_cast(const string_view &s)
{
	return _lex_cast<long double>(s);
}

template<> ircd::nanoseconds
ircd::lex_cast(const string_view &s)
{
	return std::chrono::duration<time_t, std::ratio<1L, 1000000000L>>(_lex_cast<time_t>(s));
}

template<> ircd::microseconds
ircd::lex_cast(const string_view &s)
{
	return std::chrono::duration<time_t, std::ratio<1L, 1000000L>>(_lex_cast<time_t>(s));
}

template<> ircd::milliseconds
ircd::lex_cast(const string_view &s)
{
	return std::chrono::duration<time_t, std::ratio<1L, 1000L>>(_lex_cast<time_t>(s));
}

template<> ircd::seconds
ircd::lex_cast(const string_view &s)
{
	return std::chrono::duration<time_t, std::ratio<1L, 1L>>(_lex_cast<time_t>(s));
}

template<> bool
ircd::try_lex_cast<bool>(const string_view &s)
{
	bool i;
	return boost::conversion::try_lexical_convert(s, i);
}

template<> bool
ircd::try_lex_cast<int8_t>(const string_view &s)
{
	int8_t i;
	return boost::conversion::try_lexical_convert(s, i);
}

template<> bool
ircd::try_lex_cast<uint8_t>(const string_view &s)
{
	uint8_t i;
	return boost::conversion::try_lexical_convert(s, i);
}

template<> bool
ircd::try_lex_cast<short>(const string_view &s)
{
	short i;
	return boost::conversion::try_lexical_convert(s, i);
}

template<> bool
ircd::try_lex_cast<ushort>(const string_view &s)
{
	ushort i;
	return boost::conversion::try_lexical_convert(s, i);
}

template<> bool
ircd::try_lex_cast<int>(const string_view &s)
{
	int i;
	return boost::conversion::try_lexical_convert(s, i);
}

template<> bool
ircd::try_lex_cast<unsigned int>(const string_view &s)
{
	unsigned int i;
	return boost::conversion::try_lexical_convert(s, i);
}

template<> bool
ircd::try_lex_cast<long>(const string_view &s)
{
	long i;
	return boost::conversion::try_lexical_convert(s, i);
}

template<> bool
ircd::try_lex_cast<unsigned long>(const string_view &s)
{
	unsigned long i;
	return boost::conversion::try_lexical_convert(s, i);
}

template<> bool
ircd::try_lex_cast<double>(const string_view &s)
{
	double i;
	return boost::conversion::try_lexical_convert(s, i);
}

template<> bool
ircd::try_lex_cast<long double>(const string_view &s)
{
	long double i;
	return boost::conversion::try_lexical_convert(s, i);
}

template<> bool
ircd::try_lex_cast<ircd::nanoseconds>(const string_view &s)
{
	time_t i; //TODO: XXX
	return boost::conversion::try_lexical_convert(s, i);
}

template<> bool
ircd::try_lex_cast<ircd::microseconds>(const string_view &s)
{
	time_t i; //TODO: XXX
	return boost::conversion::try_lexical_convert(s, i);
}

template<> bool
ircd::try_lex_cast<ircd::milliseconds>(const string_view &s)
{
	time_t i; //TODO: XXX
	return boost::conversion::try_lexical_convert(s, i);
}

template<> bool
ircd::try_lex_cast<ircd::seconds>(const string_view &s)
{
	time_t i; //TODO: XXX
	return boost::conversion::try_lexical_convert(s, i);
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/stringops.h
//

std::string
ircd::replace(const string_view &s,
              const char &before,
              const string_view &after)
{
	const uint32_t occurs
	(
		std::count(begin(s), end(s), before)
	);

	const size_t size
	{
		occurs? s.size() + (occurs * after.size()):
		        s.size() - occurs
	};

	return string(size, [&s, &before, &after]
	(const mutable_buffer &buf)
	{
		char *p{begin(buf)};
		std::for_each(begin(s), end(s), [&before, &after, &p]
		(const char &c)
		{
			if(c == before)
			{
				memcpy(p, after.data(), after.size());
				p += after.size();
			}
			else *p++ = c;
		});

		return std::distance(begin(buf), p);
	});
}
