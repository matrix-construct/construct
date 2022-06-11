// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_SPIRIT_STRING_H

#if defined(__clang__)
namespace boost::spirit::qi::detail
{
	template<class iterator,
	         class attribute>
	[[gnu::always_inline]]
	inline bool
	string_parse(const char *str,
	             iterator &first,
	             const iterator &last,
	             attribute &attr)
	{
		char ch;
		iterator it(first);
		#pragma clang loop unroll(disable) vectorize(enable)
		for(ch = *str; likely(ch != 0); ch = *str)
		{
			if(likely(it == last))
				return false;

			if(unlikely(ch != *it))
				return false;

			++it;
			++str;
		}

		spirit::traits::assign_to(first, it, attr);
		first = it;
		return true;
	}
}
#endif
