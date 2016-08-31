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
