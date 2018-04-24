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
#define HAVE_IRCD_UTIL_POINTERS_H

//
// Transform to pointer utils
//

namespace ircd {
namespace util {

/// Transform input sequence values to pointers in the output sequence
/// using two input iterators [begin, end] and one output iterator [begin]
template<class input_begin,
         class input_end,
         class output_begin>
auto
pointers(input_begin&& ib,
         const input_end &ie,
         output_begin&& ob)
{
	return std::transform(ib, ie, ob, []
	(auto&& value)
	{
		return std::addressof(value);
	});
}

template<class input_container,
         class output_container>
auto
pointers(input_container&& ic,
         output_container &oc)
{
	return pointers(begin(ic), end(ic), begin(oc));
}

} // namespace util
} // namespace ircd
