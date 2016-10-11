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

#pragma once
#define HAVE_IRCD_STRINGOPS_H

#ifdef __cplusplus
namespace ircd {

size_t token_count(const std::string &str, const char *const &sep);
const std::string &token(const std::string &str, const char *const &sep, const size_t &at);
const std::string &token_last(const std::string &str, const char *const &sep);

using token_closure_string = std::function<void (const std::string &)>;
void tokens(const std::string &str, const char *const &sep, const token_closure_string &);

std::string chomp(const std::string &str, const std::string &c = " "s);
std::pair<std::string, std::string> split(const std::string &str, const std::string &delim = " "s);
std::string between(const std::string &str, const std::string &a = "("s, const std::string &b = ")"s);
bool endswith(const std::string &str, const char *const &val);
bool endswith(const std::string &str, const std::string &val);
bool endswith(const std::string &str, const char &val);
template<class It> bool endswith_any(const std::string &str, const It &begin, const It &end);
bool startswith(const std::string &str, const char *const &val);
bool startswith(const std::string &str, const std::string &val);
bool startswith(const std::string &str, const char &val);

char *strip_colour(char *string);
char *strip_unprintable(char *string);
char *reconstruct_parv(int parc, const char **parv);

} // namespace ircd

inline bool
ircd::startswith(const std::string &str,
                 const char &val)
{
	return !str.empty() && str[0] == val;
}

inline bool
ircd::startswith(const std::string &str,
                 const std::string &val)
{
	const auto pos(str.find(val, 0));
	return pos == 0;
}

inline bool
ircd::startswith(const std::string &str,
                 const char *const &val)
{
	const auto pos(str.find(val, 0, strlen(val)));
	return pos == 0;
}

template<class It>
bool
ircd::endswith_any(const std::string &str,
                   const It &begin,
                   const It &end)
{
	return std::any_of(begin, end, [&str](const auto &val)
	{
		return endswith(str, val);
	});
}

inline bool
ircd::endswith(const std::string &str,
               const char &val)
{
	return !str.empty() && str[str.size()-1] == val;
}

inline bool
ircd::endswith(const std::string &str,
               const std::string &val)
{
	const auto vlen(std::min(str.size(), val.size()));
	const auto pos(str.find(val, vlen));
	return pos == str.size() - vlen;
}

inline bool
ircd::endswith(const std::string &str,
               const char *const &val)
{
	const auto vlen(std::min(str.size(), strlen(val)));
	const auto pos(str.find(val, str.size() - vlen));
	return pos == str.size() - vlen;
}

inline std::string
ircd::between(const std::string &str,
              const std::string &a,
              const std::string &b)
{
	return split(split(str, a).second, b).first;
}


inline std::pair<std::string, std::string>
ircd::split(const std::string &str,
            const std::string &delim)
{
	const auto pos(str.find(delim));
	return pos == std::string::npos? std::make_pair(str, std::string{}):
	                                 std::make_pair(str.substr(0, pos), str.substr(pos+delim.size()));
}

inline std::string
ircd::chomp(const std::string &str,
            const std::string &c)
{
	const auto pos(str.find_last_not_of(c));
	return pos == std::string::npos? str : str.substr(0, pos+1);
}

#endif // __cplusplus
