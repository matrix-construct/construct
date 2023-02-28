// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_UTIL_BLACKWHITE_H

namespace ircd { inline namespace util
{
	namespace blackwhite
	{
		struct list;
	}
}}

class ircd::util::blackwhite::list
{
	string_view black;
	string_view white;
	char delim;

	bool match(const string_view &) const;

  public:
	bool operator()(const string_view &) const;

	list(const char delim          = ' ',
	     const string_view &black  = {},
	     const string_view &white  = {});
};

inline
ircd::util::blackwhite::list::list(const char delim,
	                               const string_view &black,
	                               const string_view &white)
:black{black}
,white{white}
,delim{delim}
{}

inline bool
ircd::util::blackwhite::list::operator()(const string_view &s)
const
{
	if(!this->black && !this->white)
		return true;

	return match(s);
}
