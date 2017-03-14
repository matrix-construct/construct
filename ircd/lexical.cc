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

#include <RB_INC_BOOST_TOKENIZER_HPP
#include <RB_INC_BOOST_LEXICAL_CAST_HPP

ircd::string_view
ircd::token_first(const string_view &str,
                  const char *const &sep)
{
	return token(str, sep, 0);
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

	auto ret(it);
	while(it != end(view))
		ret = it++;

	return *ret;
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
ircd::tokens_count(const string_view &str,
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
             const char *const &sep,
             char *const &buf,
             const size_t &max,
             const token_view &closure)
{
	char *ptr(buf);
	char *const stop(buf + max);
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

	return std::distance(buf, ptr);
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

namespace ircd {

const size_t LEX_CAST_BUFSIZE {64};
thread_local char lex_cast_buf[LEX_CAST_BUFS][LEX_CAST_BUFSIZE];

template<size_t N,
         class T>
static string_view
_lex_cast(const T &i,
          char *buf,
          size_t max)
{
	using array = std::array<char, N>;

	if(!buf)
	{
		static thread_local uint cur;
		buf = lex_cast_buf[cur++];
		max = LEX_CAST_BUFSIZE;
		cur %= LEX_CAST_BUFS;
	}

	assert(max >= N);
	auto &a(*reinterpret_cast<array *>(buf));
	a = boost::lexical_cast<array>(i);
	return { buf, strnlen(buf, max) };
}

template<class T>
static T
_lex_cast(const string_view &s)
{
	return boost::lexical_cast<T>(s);
}

} // namespace ircd

template<> ircd::string_view
ircd::lex_cast(bool i,
               char *const &buf,
               const size_t &max)
{
	static const size_t MAX(8);
	return _lex_cast<MAX>(i, buf, max);
}

template<> ircd::string_view
ircd::lex_cast(int8_t i,
               char *const &buf,
               const size_t &max)
{
	static const size_t MAX(8);
	return _lex_cast<MAX>(i, buf, max);
}

template<> ircd::string_view
ircd::lex_cast(uint8_t i,
               char *const &buf,
               const size_t &max)
{
	static const size_t MAX(8);
	return _lex_cast<MAX>(i, buf, max);
}

template<> ircd::string_view
ircd::lex_cast(short i,
               char *const &buf,
               const size_t &max)
{
	static const size_t MAX(8);
	return _lex_cast<MAX>(i, buf, max);
}

template<> ircd::string_view
ircd::lex_cast(ushort i,
               char *const &buf,
               const size_t &max)
{
	static const size_t MAX(8);
	return _lex_cast<MAX>(i, buf, max);
}

template<> ircd::string_view
ircd::lex_cast(int i,
               char *const &buf,
               const size_t &max)
{
	static const size_t MAX(16);
	return _lex_cast<MAX>(i, buf, max);
}

template<> ircd::string_view
ircd::lex_cast(uint i,
               char *const &buf,
               const size_t &max)
{
	static const size_t MAX(16);
	return _lex_cast<MAX>(i, buf, max);
}

template<> ircd::string_view
ircd::lex_cast(long i,
               char *const &buf,
               const size_t &max)
{
	static const size_t MAX(32);
	return _lex_cast<MAX>(i, buf, max);
}

template<> ircd::string_view
ircd::lex_cast(ulong i,
               char *const &buf,
               const size_t &max)
{
	static const size_t MAX(32);
	return _lex_cast<MAX>(i, buf, max);
}

template<> ircd::string_view
ircd::lex_cast(double i,
               char *const &buf,
               const size_t &max)
{
	static const size_t MAX(64);
	return _lex_cast<MAX>(i, buf, max);
}

template<> ircd::string_view
ircd::lex_cast(long double i,
               char *const &buf,
               const size_t &max)
{
	static const size_t MAX(64);
	return _lex_cast<MAX>(i, buf, max);
}

template<> bool
ircd::lex_cast(const string_view &s)
{
	return _lex_cast<bool>(s);
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
