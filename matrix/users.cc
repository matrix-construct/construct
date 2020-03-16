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

decltype(ircd::m::users::opts_default)
ircd::m::users::opts_default;

bool
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
ircd::m::users::for_each(const user::closure_bool &closure)
{
	return for_each(opts_default, closure);
}

bool
ircd::m::users::for_each(const opts &opts,
                         const user::closure_bool &closure)
{
	// Branch to a better query for hosts when there's no localpart given.
	if(opts.hostpart && (!opts.localpart || opts.localpart == "@"))
		return for_each_host(opts, closure);

	bool ret{true};
	events::sender::for_each(opts.localpart, [&opts, &ret, &closure]
	(const id::user &sender)
	{
		if(opts.localpart && !opts.localpart_prefix)
			if(sender.local() != opts.localpart)
				return false;

		if(opts.localpart && opts.localpart_prefix)
			if(!startswith(sender.local(), opts.localpart))
				return false;

		if(opts.hostpart && !opts.hostpart_prefix)
			if(sender.host() != opts.hostpart)
				return true;

		if(opts.hostpart && opts.hostpart_prefix)
			if(!startswith(sender.host(), opts.hostpart))
				return true;

		// Call the user with match.a
		ret = closure(sender);
		return ret;
	});

	return ret;
}

bool
ircd::m::users::for_each_host(const opts &opts,
                              const user::closure_bool &closure)
{
	bool ret{true};
	events::origin::for_each(opts.hostpart, [&ret, &opts, &closure]
	(const string_view &origin)
	{
		if(opts.hostpart && !opts.hostpart_prefix)
			if(origin != opts.hostpart)
				return false;

		if(opts.hostpart && opts.hostpart_prefix)
			if(!startswith(origin, opts.hostpart))
				return false;

		auto _opts(opts);
		_opts.hostpart = origin;
		_opts.hostpart_prefix = false;
		ret = for_each_in_host(_opts, closure);
		return ret;
	});

	return ret;
}

bool
ircd::m::users::for_each_in_host(const opts &opts,
                                 const user::closure_bool &closure)
{
	bool ret{true};
	m::user::id::buf last;
	events::origin::for_each_in(opts.hostpart, [&opts, &ret, &closure, &last]
	(const id::user &sender, const auto &event_idx)
	{
		if(sender == last)
			return true;

		if(opts.hostpart && !opts.hostpart_prefix)
			if(sender.host() != opts.hostpart)
				return false;

		if(opts.hostpart && opts.hostpart_prefix)
			if(!startswith(sender.host(), opts.hostpart))
				return false;

		if(opts.localpart && opts.localpart_prefix)
			if(!startswith(sender.local(), opts.localpart))
				return true;

		if(opts.localpart && !opts.localpart_prefix)
			if(sender.local() != opts.localpart)
				return true;

		// Call the user with match.
		ret = closure(sender);
		last = sender;
		return ret;
	});

	return ret;
}

ircd::m::users::opts::opts(const string_view &query)
{
	if(startswith(query, '@') && has(query, ':'))
	{
		localpart = split(query, ':').first;
		hostpart = split(query, ':').second;
		hostpart_prefix = !has(hostpart, '.');
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
		hostpart_prefix = !has(hostpart, '.');
		return;
	}

	localpart = query;
	localpart_prefix = true;
}
