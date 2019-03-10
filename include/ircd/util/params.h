// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_UTIL_PARAMS_H

// This file is not part of the standard include stack. It is included
// manually as needed.

namespace ircd::util
{
	struct params;
}

struct ircd::util::params
{
	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, missing)
	IRCD_EXCEPTION(error, invalid)

	string_view in;
	const char *sep;
	std::vector<const char *> names;

	const char *name(const size_t &i) const;
	size_t name(const string_view &) const;

  public:
	size_t count() const;

	// Get positional argument by position index
	string_view operator[](const size_t &i) const;            // returns empty
	template<class T> T at(const size_t &i, const T &def) const;    // throws invalid
	template<class T> T at(const size_t &i) const;                  // throws missing or invalid
	string_view at(const size_t &i) const;                    // throws missing

	// Get positional argument by name->index convenience.
	string_view operator[](const string_view &) const;
	template<class T> T at(const string_view &, const T &def) const;
	template<class T> T at(const string_view &) const;
	string_view at(const string_view &) const;

	params(const string_view &in,
	       const char *const &sep,
	       const std::initializer_list<const char *> &names = {});
};

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
ircd::util::params::at(const string_view &name,
                       const T &def)
const
{
	return at<T>(this->name(name), def);
}

template<class T>
T
ircd::util::params::at(const string_view &name)
const
{
	return at<T>(this->name(name));
}

inline ircd::string_view
ircd::util::params::at(const string_view &name)
const
{
	return at(this->name(name));
}

inline ircd::string_view
ircd::util::params::operator[](const string_view &name)
const
{
	return operator[](this->name(name));
}

template<class T>
T
ircd::util::params::at(const size_t &i,
                       const T &def)
const try
{
	return count() > i? at<T>(i) : def;
}
catch(const bad_lex_cast &e)
{
	throw invalid
	{
		"parameter #%zu <%s>", i, name(i)
	};
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
	throw invalid
	{
		"parameter #%zu <%s>", i, name(i)
	};
}

inline ircd::string_view
ircd::util::params::at(const size_t &i)
const try
{
	return token(in, sep, i);
}
catch(const std::out_of_range &e)
{
	throw missing
	{
		"required parameter #%zu <%s>", i, name(i)
	};
}

inline ircd::string_view
ircd::util::params::operator[](const size_t &i)
const
{
	return count() > i? token(in, sep, i) : string_view{};
}

inline size_t
ircd::util::params::count()
const
{
	return token_count(in, sep);
}

inline size_t
ircd::util::params::name(const string_view &name)
const
{
	return util::index(begin(names), end(names), name);
}

inline const char *
ircd::util::params::name(const size_t &i)
const
{
	return names.size() > i? *std::next(begin(names), i) : "<unnamed>";
}
