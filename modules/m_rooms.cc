// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::mapi::header
IRCD_MODULE
{
	"Matrix rooms interface; modular components"
};

decltype(ircd::m::rooms::opts_default)
ircd::m::rooms::opts_default;

bool
IRCD_MODULE_EXPORT
ircd::m::rooms::has(const opts &opts)
{
	return !for_each(opts, []
	(const m::room::id &)
	{
		// false to break; for_each() returns false
		return false;
	});
}

size_t
IRCD_MODULE_EXPORT
ircd::m::rooms::count(const opts &opts)
{
	size_t ret{0};
	for_each(opts, [&ret]
	(const m::room::id &)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
IRCD_MODULE_EXPORT
ircd::m::rooms::for_each(const room::id::closure_bool &closure)
{
	return for_each(opts_default, closure);
}

bool
IRCD_MODULE_EXPORT
ircd::m::rooms::for_each(const opts &opts,
                         const room::id::closure_bool &closure)
{
	bool ret{true};
	const auto proffer{[&opts, &closure, &ret]
	(const m::room::id &room_id)
	{
		if(opts.room_id && !opts.lower_bound)
		{
			ret = false;
			return;
		}

		if(opts.room_id)
			if(room_id < opts.room_id)
				return;

		if(opts.server && !opts.summary)
			if(opts.server != room_id.host())
				return;

		if(opts.summary)
			if(!summary::has(room_id))
				return;

		if(opts.server && opts.summary)
			if(!room::aliases(room_id).count(opts.server))
				return;

		if(opts.join_rule)
			if(!room(room_id).join_rule(opts.join_rule))
				return;

		ret = closure(room_id);
	}};

	// branch for optimized public rooms searches.
	if(opts.summary)
	{
		const room::id::buf public_room_id
		{
			"!public", my_host()
		};

		const room::state state{public_room_id};
		return state.for_each("ircd.rooms", opts.server, [&proffer, &ret]
		(const string_view &type, const string_view &state_key, const event::idx &event_idx)
		{
			room::id::buf buf;
			proffer(room::id::unswap(state_key, buf));
			return ret;
		});
	}

	return events::for_each_in_type("m.room.create", [&proffer, &ret]
	(const string_view &type, const event::idx &event_idx)
	{
		assert(type == "m.room.create");
		m::get(std::nothrow, event_idx, "room_id", proffer);
		return ret;
	});
}
