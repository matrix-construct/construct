// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_IOS_DISPATCH_H

namespace ircd::ios
{
	struct dispatch;

	IRCD_OVERLOAD(defer)
	IRCD_OVERLOAD(yield)
}

namespace ircd
{
	using ios::dispatch;
}

struct ircd::ios::dispatch
{
	dispatch(descriptor &, std::function<void ()>);

	dispatch(descriptor &, yield_t, const std::function<void ()> &);

	dispatch(descriptor &, defer_t, std::function<void ()>);

	dispatch(descriptor &, defer_t, yield_t, const std::function<void ()> &);

	dispatch(descriptor &, defer_t, yield_t);
};
