// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::users
{
	static bool for_each_host(const opts &, const user::closure_bool &);
	static bool for_each_in_host(const opts &, const user::closure_bool &);
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix users interface"
};

decltype(ircd::m::users::opts_default)
ircd::m::users::opts_default;

bool
IRCD_MODULE_EXPORT
ircd::m::users::exists(const opts &opts)
{
	return !for_each(opts, []
	(const auto &)
	{
		// return false to break and have for_each() returns false
		return false;
	});
}

size_t
IRCD_MODULE_EXPORT
ircd::m::users::count(const opts &opts)
{
	size_t ret(0);
	for_each(opts, [&ret](const auto &)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
IRCD_MODULE_EXPORT
ircd::m::users::for_each(const user::closure_bool &closure)
{
	return for_each(opts_default, closure);
}

bool
IRCD_MODULE_EXPORT
ircd::m::users::for_each(const opts &opts,
                         const user::closure_bool &closure)
{
	// Note: if opts.hostpart is given then for_each_host() will close over
	// that host, so no branch is needed here.
	return for_each_host(opts, closure);
}

bool
ircd::m::users::for_each_host(const opts &opts,
                              const user::closure_bool &closure)
{
	bool ret{true};
	events::for_each_origin(opts.hostpart, [&ret, &opts, &closure]
	(const string_view &origin)
	{
		// The events:: iteration interface works with prefixes; if our
		// user wants to iterate users on exactly a single server, we test
		// for an exact match here and can break from the loop if no match.
		if(opts.hostpart && !opts.hostpart_prefix)
			if(origin != opts.hostpart)
				return false;

		auto _opts(opts);
		_opts.hostpart = origin;
		ret = for_each_in_host(_opts, closure);
		return ret;
	});

	return ret;
}

bool
ircd::m::users::for_each_in_host(const opts &opts,
                                 const user::closure_bool &closure)
{
	assert(opts.hostpart);

	bool ret{true};
	events::for_each_sender(opts.hostpart, [&opts, &ret, &closure]
	(const id::user &sender)
	{
		// The events:: iteration interface only tests if the sender's
		// hostpart startswith our queried hostpart; we want an exact
		// match here, otherwise our loop can finish.
		if(sender.host() != opts.hostpart)
			return false;

		// Skip this entry if the user wants a prefix match on the localpart
		// and this mxid doesn't match.
		if(opts.localpart && opts.localpart_prefix)
			if(!startswith(sender.local(), opts.localpart))
				return true;

		// Skip this entry if the user wants an exact match on the localpart
		// and this mxid doesn't match.
		if(opts.localpart && !opts.localpart_prefix)
			if(sender.local() != opts.localpart)
				return true;

		// Call the user with match.
		ret = closure(sender);
		return ret;
	});

	return ret;
}

IRCD_MODULE_EXPORT
ircd::m::users::opts::opts(const string_view &query)
{
	if(startswith(query, '@') && has(query, ':'))
	{
		localpart = split(query, ':').first;
		hostpart = split(query, ':').second;
		return;
	}

	if(startswith(query, '@'))
	{
		localpart = query;
		localpart_prefix = true;
		return;
	}

	if(startswith(query, ':'))
	{
		hostpart = lstrip(query, ':');
		return;
	}

	hostpart = query;
	hostpart_prefix = true;
}
