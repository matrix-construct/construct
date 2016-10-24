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

	rb_strlcpy(tmpbuf, parv[0], BUFSIZE);
	for (int i = 1; i < parc; i++)
	{
		rb_strlcat(tmpbuf, " ", BUFSIZE);
		rb_strlcat(tmpbuf, parv[i], BUFSIZE);
	}

	return tmpbuf;
}

std::string
ircd::token(const std::string &str,
            const char *const &sep,
            const size_t &at)
{
	using delim = boost::char_separator<char>;

	const delim d(sep);
	const boost::tokenizer<delim> view(str, d);

	size_t i(0);
	auto it(begin(view));
	for(; it != end(view); ++it, i++)
		if(i == at)
			return *it;

	if(str.empty())
		return str;

	// Moving the iterator may cause a parse, so there is no pre-check on the bounds;
	// the valid token is returned in the loop, and exiting the loop is an error.
	throw std::out_of_range("token out of range");
}

std::string
ircd::token_last(const std::string &str,
                 const char *const &sep)
{
	using delim = boost::char_separator<char>;

	const delim d(sep);
	const boost::tokenizer<delim> view(str, d);

	auto it(begin(view));
	if(it == end(view))
		return str.empty()? str : throw std::out_of_range("token out of range");

	auto ret(it);
	while(it != end(view))
		ret = it++;

	return *ret;
}

size_t
ircd::token_count(const std::string &str,
                  const char *const &sep)
{
	using delim = boost::char_separator<char>;

	const delim d(sep);
	const boost::tokenizer<delim> view(str, d);
	return std::distance(begin(view), end(view));
}

std::vector<std::string>
ircd::tokens(const std::string &str,
             const char *const &sep,
             const size_t &reserve)
{
	using delim = boost::char_separator<char>;

	std::vector<std::string> ret;
	ret.reserve(reserve);

	const delim d(sep);
	const boost::tokenizer<delim> view(str, d);
	std::copy(begin(view), end(view), std::back_inserter(ret));

	return ret;
}

void
ircd::tokens(const std::string &str,
             const char *const &sep,
             const token_closure_string &closure)
{
	using delim = boost::char_separator<char>;

	const delim d(sep);
	const boost::tokenizer<delim> view(str, d);
	std::for_each(begin(view), end(view), closure);
}

std::vector<char *>
ircd::tokens(const char *const &str,
             const char *const &sep,
             char *const &buf,
             const size_t &max,
             const size_t &reserve)
{
	std::vector<char *> ret;
	ret.reserve(reserve);
	rb_strlcpy(buf, str, max);
	tokens(buf, sep, [&ret]
	(char *const &token)
	{
		ret.emplace_back(token);
	});

	return ret;
}

void
ircd::tokensa(const char *const &str,
              const char *const &sep,
              const token_closure_cstr &closure)
{
	custom_ptr<char> cpy(strdup(str), std::free);
	tokens(cpy.get(), sep, closure);
}

void
ircd::tokens(const char *const &str,
             const char *const &sep,
             const token_closure_cstr &closure)
{
	const auto len(strlen(str));
	char buf[strlen(str) + 1];
	tokens(str, sep, buf, sizeof(buf), closure);
}

void
ircd::tokens(const char *const &str,
             const char *const &sep,
             char *const &buf,
             const size_t &max,
             const token_closure_cstr &closure)
{
	rb_strlcpy(buf, str, max);
	tokens(buf, sep, closure);
}

void
ircd::tokens(char *const &str,
             const char *const &sep,
             const token_closure_cstr &closure)
{
	char *ctx;
	char *tok(strtok_r(str, sep, &ctx)); do
	{
		closure(tok);
	}
	while((tok = strtok_r(NULL, sep, &ctx)) != NULL);
}
