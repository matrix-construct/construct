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
#define HAVE_IRCD_PARAMS_H

namespace ircd {
inline namespace util {

class params
{
	string_view in;
	const char *sep;
	std::vector<const char *> names;

	const char *name(const size_t &i) const;

  public:
	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, missing)
	IRCD_EXCEPTION(error, invalid)

	string_view operator[](const size_t &i) const;                  // returns empty
	template<class T> T at(const size_t &i, const T &def) const;    // throws invalid
	template<class T> T at(const size_t &i) const;                  // throws missing or invalid
	string_view at(const size_t &i) const;                          // throws missing

	params(const string_view &in,
	       const char *const &sep,
	       const std::initializer_list<const char *> &names = {});
};

} // namespace util
} // namespace ircd

inline
ircd::util::params::params(const string_view &in,
                           const char *const &sep,
	                       const std::initializer_list<const char *> &names)
:in{in}
,sep{sep}
,names{names}
{
}

template<class T>
T
ircd::util::params::at(const size_t &i,
                        const T &def)
const try
{
	return tokens_count(in, sep) > i? at<T>(i) : def;
}
catch(const bad_lex_cast &e)
{
	throw invalid("parameter #%zu <%s>", i, name(i));
}

template<class T>
T
ircd::util::params::at(const size_t &i)
const try
{
	return lex_cast<T>(at(i));
}
catch(const bad_lex_cast &e)
{
	throw invalid("parameter #%zu <%s>", i, name(i));
}

inline ircd::string_view
ircd::util::params::at(const size_t &i)
const try
{
	return token(in, sep, i);
}
catch(const std::out_of_range &e)
{
	throw missing("required parameter #%zu <%s>", i, name(i));
}

inline ircd::string_view
ircd::util::params::operator[](const size_t &i)
const
{
	return tokens_count(in, sep) > i? token(in, sep, i) : string_view{};
}

inline const char *
ircd::util::params::name(const size_t &i)
const
{
	return names.size() > i? *std::next(begin(names), i) : "<unnamed>";
}
