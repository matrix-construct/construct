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
	IRCD_OVERLOAD(synchronous)

	struct dispatch;
	struct defer;
	struct post;
}

namespace ircd
{
	using ios::dispatch;
	using ios::defer;
	using ios::post;
}

struct ircd::ios::dispatch
{
	dispatch(descriptor &, std::function<void ()>);
	dispatch(descriptor &, synchronous_t, const std::function<void ()> &);
	dispatch(descriptor &, synchronous_t);
	dispatch(std::function<void ()>);
	dispatch(synchronous_t, const std::function<void ()> &);
};

struct ircd::ios::defer
{
	defer(descriptor &, std::function<void ()>);
	defer(descriptor &, synchronous_t, const std::function<void ()> &);
	defer(descriptor &, synchronous_t);
	defer(std::function<void ()>);
	defer(synchronous_t, const std::function<void ()> &);
};

struct ircd::ios::post
{
	post(descriptor &, std::function<void ()>);
	post(descriptor &, synchronous_t, const std::function<void ()> &);
	post(descriptor &, synchronous_t);
	post(std::function<void ()>);
	post(synchronous_t, const std::function<void ()> &);
};
