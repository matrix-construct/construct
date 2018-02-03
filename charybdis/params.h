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
#define HAVE_CONSTRUCT_PARAMS_H

class params
{
	ircd::string_view in;
	const char *sep;
	std::vector<const char *> names;

	const char *name(const size_t &i) const;

  public:
	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, missing)
	IRCD_EXCEPTION(error, invalid)

	size_t count() const;
	ircd::string_view operator[](const size_t &i) const;            // returns empty
	template<class T> T at(const size_t &i, const T &def) const;    // throws invalid
	template<class T> T at(const size_t &i) const;                  // throws missing or invalid
	ircd::string_view at(const size_t &i) const;                    // throws missing

	params(const ircd::string_view &in,
	       const char *const &sep,
	       const std::initializer_list<const char *> &names = {});
};

inline
params::params(const ircd::string_view &in,
               const char *const &sep,
	           const std::initializer_list<const char *> &names)
:in{in}
,sep{sep}
,names{names}
{
}

template<class T>
T
params::at(const size_t &i,
                        const T &def)
const try
{
	return count() > i? at<T>(i) : def;
}
catch(const ircd::bad_lex_cast &e)
{
	throw invalid("parameter #%zu <%s>", i, name(i));
}

template<class T>
T
params::at(const size_t &i)
const try
{
	return ircd::lex_cast<T>(at(i));
}
catch(const ircd::bad_lex_cast &e)
{
	throw invalid("parameter #%zu <%s>", i, name(i));
}

inline ircd::string_view
params::at(const size_t &i)
const try
{
	return token(in, sep, i);
}
catch(const std::out_of_range &e)
{
	throw missing("required parameter #%zu <%s>", i, name(i));
}

inline ircd::string_view
params::operator[](const size_t &i)
const
{
	return count() > i? token(in, sep, i) : ircd::string_view{};
}

inline size_t
params::count()
const
{
	return token_count(in, sep);
}

inline const char *
params::name(const size_t &i)
const
{
	return names.size() > i? *std::next(begin(names), i) : "<unnamed>";
}
