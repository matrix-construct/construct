/*
 *  charybdis: an advanced ircd.
 *  inline/stringops.h: inlined string operations used in a few places
 *
 *  Copyright (C) 2005-2016 Charybdis Development Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 */

/// !!! NOTE !!!
///
/// Many functions implemented here need to be replaced with karma generators
/// similar to ircd::fmt. Both the boost and std lexical conversions are an
/// order of magnitude slower than the formal generators. Some tokenizations
/// can also be replaced.
///

#include <RB_INC_BOOST_TOKENIZER_HPP
#include <RB_INC_BOOST_LEXICAL_CAST_HPP
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>

///////////////////////////////////////////////////////////////////////////////
//
// ircd/tokens.h
//

ircd::string_view
ircd::tokens_after(const string_view &str,
                   const char &sep,
                   const size_t &i)
{
	const char ssep[2] { sep, '\0' };
	return tokens_after(str, ssep, i);
}

ircd::string_view
ircd::tokens_after(const string_view &str,
                   const char *const &sep,
                   const size_t &i)
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = boost::char_separator<char>;

	const delim d(sep);
	const boost::tokenizer<delim, iter, type> view(str, d);

	auto it(begin(view));
	for(size_t j(0); it != end(view); ++it, j++)
		if(j > i)
			return string_view{it->data(), str.data() + str.size()};

	return {};
}

ircd::string_view
ircd::token_first(const string_view &str,
                  const char &sep)
{
	const char ssep[2] { sep, '\0' };
	return token(str, ssep, 0);
}

ircd::string_view
ircd::token_first(const string_view &str,
                  const char *const &sep)
{
	return token(str, sep, 0);
}

ircd::string_view
ircd::token_last(const string_view &str,
                 const char &sep)
{
	const char ssep[2] { sep, '\0' };
	return token_last(str, ssep);
}

ircd::string_view
ircd::token_last(const string_view &str,
                 const char *const &sep)
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = boost::char_separator<char>;

	const delim d(sep);
	const boost::tokenizer<delim, iter, type> view(str, d);

	auto it(begin(view));
	if(it == end(view))
		return str.empty()? str : throw std::out_of_range("token out of range");

	string_view ret(*it);
	for(++it; it != end(view); ++it)
		ret = *it;

	return ret;
}

ircd::string_view
ircd::token(const string_view &str,
            const char &sep,
            const size_t &i)
{
	const char ssep[2] { sep, '\0' };
	return token(str, ssep, i);
}

ircd::string_view
ircd::token(const string_view &str,
            const char *const &sep,
            const size_t &i)
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = boost::char_separator<char>;

	const delim d(sep);
	const boost::tokenizer<delim, iter, type> view(str, d);
	const auto it(at(begin(view), end(view), i));
	return *it;
}

size_t
ircd::token_count(const string_view &str,
                  const char &sep)
{
	const char ssep[2] { sep, '\0' };
	return token_count(str, ssep);
}

size_t
ircd::token_count(const string_view &str,
                  const char *const &sep)
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = boost::char_separator<char>;

	const delim d(sep);
	const boost::tokenizer<delim, iter, type> view(str, d);
	return std::distance(begin(view), end(view));
}

size_t
ircd::tokens(const string_view &str,
             const char &sep,
             const mutable_buffer &buf,
             const token_view &closure)
{
	const char ssep[2] { sep, '\0' };
	return tokens(str, ssep, buf, closure);
}

size_t
ircd::tokens(const string_view &str,
             const char *const &sep,
             const mutable_buffer &buf,
             const token_view &closure)
{
	char *ptr(data(buf));
	char *const stop(data(buf) + size(buf));
	tokens(str, sep, [&closure, &ptr, &stop]
	(const string_view &token)
	{
		const size_t terminated_size(token.size() + 1);
		const size_t remaining(std::distance(ptr, stop));
		if(remaining < terminated_size)
			return;

		char *const dest(ptr);
		ptr += strlcpy(dest, token.data(), terminated_size);
		closure(string_view(dest, token.size()));
	});

	return std::distance(data(buf), ptr);
}

size_t
ircd::tokens(const string_view &str,
             const char &sep,
             const size_t &limit,
             const token_view &closure)
{
	const char ssep[2] { sep, '\0' };
	return tokens(str, ssep, limit, closure);
}

size_t
ircd::tokens(const string_view &str,
             const char *const &sep,
             const size_t &limit,
             const token_view &closure)
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = boost::char_separator<char>;

	const delim d(sep);
	const boost::tokenizer<delim, iter, type> view(str, d);

	size_t i(0);
	for(auto it(begin(view)); i < limit && it != end(view); ++it, i++)
		closure(*it);

	return i;
}

void
ircd::tokens(const string_view &str,
             const char &sep,
             const token_view &closure)
{
	const char ssep[2] { sep, '\0' };
	tokens(str, ssep, closure);
}

void
ircd::tokens(const string_view &str,
             const char *const &sep,
             const token_view &closure)
{
	using type = string_view;
	using iter = typename type::const_iterator;
	using delim = boost::char_separator<char>;

	const delim d(sep);
	const boost::tokenizer<delim, iter, type> view(str, d);
	std::for_each(begin(view), end(view), closure);
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/lex_cast.h
//

namespace ircd
{
	/// The static lex_cast ring buffers are each LEX_CAST_BUFSIZE bytes;
	/// Consider increasing if some lex_cast<T>(str) has more characters.
	const size_t LEX_CAST_BUFSIZE {64};

	/// This is a static "ring buffer" to simplify a majority of lex_cast uses.
	/// If the lex_cast has binary input and string output, and no user buffer
	/// is supplied, the next buffer here will be used instead. The returned
	/// string_view of data from this buffer is only valid for several more
	/// calls to lex_cast before it is overwritten.
	thread_local char lex_cast_buf[LEX_CAST_BUFS][LEX_CAST_BUFSIZE];
	thread_local uint lex_cast_cur;

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
	throw ircd::bad_lex_cast("%s", e.what());
}

/// Internal template providing conversions from a string to a number;
/// the native object is returned directly; no ring buffer is consumed.
template<class T>
T
ircd::_lex_cast(const string_view &s)
try
{
	return boost::lexical_cast<T>(s);
}
catch(const boost::bad_lexical_cast &e)
{
	throw ircd::bad_lex_cast("%s", e.what());
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
	const auto occurs
	{
		std::count(begin(s), end(s), before)
	};

	const size_t size
	{
		occurs? s.size() + (occurs * after.size()):
		        s.size() - occurs
	};

	std::string ret(size, char{});
	auto *p{const_cast<char *>(ret.data())};
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

	//assert(ret.size() == size_t(std::distance(ret.data(), const_cast<const char *>(p))));
	ret.resize(std::distance(ret.data(), const_cast<const char *>(p)));
	return ret;
}

namespace ircd
{
	const char _b64_pad_
	{
		'='
	};

	using _b64_encoder = std::function<string_view (const mutable_buffer &, const const_raw_buffer &)>;
	static std::string _b64encode(const const_raw_buffer &in, const _b64_encoder &);
}

/// Allocate and return a string without padding from the encoding of in
std::string
ircd::b64encode_unpadded(const const_raw_buffer &in)
{
	return _b64encode(in, [](const auto &out, const auto &in)
	{
		return b64encode_unpadded(out, in);
	});
}

/// Allocate and return a string from the encoding of in
std::string
ircd::b64encode(const const_raw_buffer &in)
{
	return _b64encode(in, [](const auto &out, const auto &in)
	{
		return b64encode(out, in);
	});
}

/// Internal; dedupes encoding functions that create and return a string
static std::string
ircd::_b64encode(const const_raw_buffer &in,
                 const _b64_encoder &encoder)
{
	// Allocate a buffer 1.33 times larger than input with pessimistic
	// extra space for any padding and nulling.
	const auto max
	{
		ceil(size(in) * (4.0 / 3.0)) + 4
	};

	std::string ret(max, char{});
	const mutable_buffer buf
	{
		const_cast<char *>(ret.data()), ret.size()
	};

	const string_view encoded
	{
		encoder(buf, in)
	};

	assert(size(encoded) <= ret.size());
	ret.resize(size(encoded));
	return ret;
}

/// Encoding in to base64 at out. Out must be 1.33+ larger than in
/// padding is not present in the returned view.
ircd::string_view
ircd::b64encode(const mutable_buffer &out,
                const const_raw_buffer &in)
{
	const auto pads
	{
		(3 - size(in) % 3) % 3
	};

	const auto encoded
	{
		b64encode_unpadded(out, in)
	};

	assert(size(encoded) + pads <= size(out));
	memset(data(out) + size(encoded), _b64_pad_, pads);

	const auto len
	{
		size(encoded) + pads
	};

	return { data(out), len };
}

/// Encoding in to base64 at out. Out must be 1.33+ larger than in.
ircd::string_view
ircd::b64encode_unpadded(const mutable_buffer &out,
                         const const_raw_buffer &in)
{
	namespace iterators = boost::archive::iterators;
	using transform = iterators::transform_width<unsigned char *, 6, 8>;
	using b64fb = iterators::base64_from_binary<transform>;

	const auto cpsz
	{
		std::min(size(in), size_t(size(out) * (3.0 / 4.0)))
	};

	const auto end
	{
		std::copy(b64fb(data(in)), b64fb(data(in) + cpsz), begin(out))
	};

	const auto len
	{
		size_t(std::distance(begin(out), end))
	};

	assert(len <= size(out));
	return { data(out), len };
}

std::string
ircd::b64decode(const string_view &in)
{
	// Allocate a buffer 75% than input with pessimistic extra space
	const auto max
	{
		ceil(size(in) * 0.75) + 4
	};

	std::string ret(max, char{});
	const mutable_raw_buffer buf
	{
		reinterpret_cast<unsigned char *>(const_cast<char *>(ret.data())), ret.size()
	};

	const auto decoded
	{
		b64decode(buf, in)
	};

	assert(size(decoded) <= ret.size());
	ret.resize(size(decoded));
	return ret;
}

/// Decode base64 from in to the buffer at out; out can be 75% of the size
/// of in.
ircd::const_raw_buffer
ircd::b64decode(const mutable_raw_buffer &out,
                const string_view &in)
{
	namespace iterators = boost::archive::iterators;
	using b64bf = iterators::binary_from_base64<const char *>;
	using transform = iterators::transform_width<b64bf, 8, 6>;

	const auto pads
	{
		endswith_count(in, _b64_pad_)
	};

	const auto e
	{
		std::copy(transform(begin(in)), transform(begin(in) + size(in) - pads), begin(out))
	};

	const auto len
	{
		std::distance(begin(out), e)
	};

	assert(size_t(len) <= size(out));
	return { data(out), size_t(len) };
}

ircd::const_raw_buffer
ircd::a2u(const mutable_raw_buffer &out,
          const const_buffer &in)
{
	const size_t len{size(in) / 2};
	for(size_t i(0); i < len; ++i)
	{
		const char gl[3]
		{
			in[i * 2],
			in[i * 2 + 1],
			'\0'
		};

		out[i] = strtol(gl, nullptr, 16);
	}

	return { data(out), len };
}

ircd::string_view
ircd::u2a(const mutable_buffer &out,
          const const_raw_buffer &in)
{
	char *p(data(out));
	for(size_t i(0); i < size(in); ++i)
		p += snprintf(p, size(out) - (p - data(out)), "%02x", in[i]);

	return { data(out), size_t(p - data(out)) };
}

/*
 * strip_colour - remove colour codes from a string
 * -asuffield (?)
 */
char *
ircd::strip_colour(char *string)
{
	char *c = string;
	char *c2 = string;
	char *last_non_space = NULL;

	/* c is source, c2 is target */
	for(; c && *c; c++)
		switch (*c)
		{
		case 3:
			if(rfc1459::is_digit(c[1]))
			{
				c++;
				if(rfc1459::is_digit(c[1]))
					c++;
				if(c[1] == ',' && rfc1459::is_digit(c[2]))
				{
					c += 2;
					if(rfc1459::is_digit(c[1]))
						c++;
				}
			}
			break;
		case 2:
		case 4:
		case 6:
		case 7:
		case 15:
		case 22:
		case 23:
		case 27:
		case 29:
		case 31:
			break;
		case 32:
			*c2++ = *c;
			break;
		default:
			*c2++ = *c;
			last_non_space = c2;
			break;
		}

	*c2 = '\0';

	if(last_non_space)
		*last_non_space = '\0';

	return string;
}

char *
ircd::strip_unprintable(char *string)
{
	char *c = string;
	char *c2 = string;
	char *last_non_space = NULL;

	/* c is source, c2 is target */
	for(; c && *c; c++)
		switch (*c)
		{
		case 3:
			if(rfc1459::is_digit(c[1]))
			{
				c++;
				if(rfc1459::is_digit(c[1]))
					c++;
				if(c[1] == ',' && rfc1459::is_digit(c[2]))
				{
					c += 2;
					if(rfc1459::is_digit(c[1]))
						c++;
				}
			}
			break;
		case 32:
			*c2++ = *c;
			break;
		default:
			if (*c < 32)
				break;
			*c2++ = *c;
			last_non_space = c2;
			break;
		}

	*c2 = '\0';

	if(last_non_space)
		*last_non_space = '\0';

	return string;
}

// This is from the old parse.c and does not ensure the trailing :is added
// NOTE: Deprecated. Use formal grammar.
char *
ircd::reconstruct_parv(int parc, const char *parv[])
{
	static char tmpbuf[BUFSIZE];

	strlcpy(tmpbuf, parv[0], BUFSIZE);
	for (int i = 1; i < parc; i++)
	{
		strlcat(tmpbuf, " ", BUFSIZE);
		strlcat(tmpbuf, parv[i], BUFSIZE);
	}

	return tmpbuf;
}
