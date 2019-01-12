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
#define HAVE_IRCD_JSON_PATH_H

namespace ircd::json
{
	/// Higher order type beyond a string to cleanly delimit multiple keys.
	using path = std::initializer_list<string_view>;
	std::ostream &operator<<(std::ostream &, const path &);
}

inline std::ostream &
ircd::json::operator<<(std::ostream &s, const path &p)
{
	auto it(std::begin(p));
	if(it != std::end(p))
	{
		s << *it;
		++it;
	}

	for(; it != std::end(p); ++it)
		s << '.' << *it;

	return s;
}
