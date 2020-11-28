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

namespace ircd
{
	inline namespace util
	{
		struct params;
	}
}

struct ircd::util::params
{
	static const size_t MAX
	{
		12
	};

	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, missing)
	IRCD_EXCEPTION(error, invalid)

	string_view in;
	string_view prefix;
	const char *sep {" "};
	std::array<string_view, MAX> names;

	bool for_each_pararg(const token_view &) const;
	bool for_each_posarg(const token_view &) const;
	string_view name(const size_t &i) const;
	size_t name(const string_view &) const;

  public:
	size_t count_pararg() const;
	size_t count() const;

	bool has(const string_view &param) const;
	string_view get(const string_view &param) const;

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
	       const std::array<string_view, MAX> &names = {});

	params(const string_view &in,
	       const std::pair<const char *, const char *> &sep,
	       const std::array<string_view, MAX> &names = {});
};

inline
ircd::util::params::params(const string_view &in,
                           const char *const &sep,
	                       const std::array<string_view, MAX> &names)
:in{in}
,sep{sep}
,names{names}
{
}

inline
ircd::util::params::params(const string_view &in,
	                       const std::pair<const char *, const char *> &sep,
	                       const std::array<string_view, MAX> &names)
:in{in}
,prefix{sep.second}
,sep{sep.first}
,names{names}
{
}

template<class T>
T
ircd::util::params::at(const string_view &name,
                       const T &def)
const try
{
	const auto &val(get(name));
	return val? lex_cast<T>(val): def;
}
catch(const bad_lex_cast &)
{
	return def;
}

template<class T>
T
ircd::util::params::at(const string_view &name)
const try
{
	return lex_cast<T>(get(name));
}
catch(const bad_lex_cast &e)
{
	throw invalid
	{
		"parameter <%s> :%s",
		name,
		e.what()
	};
}

inline ircd::string_view
ircd::util::params::at(const string_view &name)
const
{
	const string_view ret
	{
		get(name)
	};

	if(unlikely(!ret))
		throw missing
		{
			"required parameter <%s>",
			name,
		};

	return ret;
}

inline ircd::string_view
ircd::util::params::operator[](const string_view &name)
const
{
	return get(name);
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
	return def;
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
const
{
	const string_view ret
	{
		this->operator[](i)
	};

	if(unlikely(!ret))
		throw missing
		{
			"required parameter #%zu <%s>",
			i,
			name(i)
		};

	return ret;
}

inline ircd::string_view
ircd::util::params::operator[](const size_t &i)
const
{
	size_t j{i};
	string_view ret;
	for_each_posarg([&ret, &j]
	(const string_view &token)
	{
		if(j-- == 0)
		{
			ret = token;
			return false;
		}
		else return true;
	});

	return ret;
}

inline ircd::string_view
ircd::util::params::get(const string_view &arg)
const
{
	if(prefix && startswith(arg, prefix))
	{
		string_view ret;
		for_each_pararg([&ret, &arg]
		(const string_view &token)
		{
			if(startswith(token, arg))
			{
				ret = token;
				return false;
			}
			else return true;
		});

		return ret;
	}
	else return operator[](this->name(arg));
}

inline bool
ircd::util::params::has(const string_view &arg)
const
{
	if(prefix && startswith(arg, prefix))
		return !for_each_pararg([&arg]
		(const string_view &token)
		{
			return !startswith(token, arg);
		});

	return count() > this->name(arg);
}

inline size_t
ircd::util::params::count()
const
{
	size_t ret{0};
	for_each_posarg([&ret](const string_view &)
	{
		++ret;
		return true;
	});

	return ret;
}

inline size_t
ircd::util::params::count_pararg()
const
{
	size_t ret{0};
	for_each_pararg([&ret](const string_view &)
	{
		++ret;
		return true;
	});

	return ret;
}

inline size_t
ircd::util::params::name(const string_view &name)
const
{
	return util::index(begin(names), end(names), name);
}

inline ircd::string_view
ircd::util::params::name(const size_t &i)
const
{
	return names.size() > i?
		*std::next(begin(names), i):
		"<unnamed>";
}

inline bool
ircd::util::params::for_each_posarg(const token_view &closure)
const
{
	return tokens(in, sep, [this, &closure]
	(const string_view &token)
	{
		return !prefix || !startswith(token, prefix)?
			closure(token):
			true;
	});
}

inline bool
ircd::util::params::for_each_pararg(const token_view &closure)
const
{
	return tokens(in, sep, [this, &closure]
	(const string_view &token)
	{
		return prefix && startswith(token, prefix)?
			closure(token):
			true;
	});
}
