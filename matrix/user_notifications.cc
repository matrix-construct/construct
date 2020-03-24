// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::user::notifications::type_prefix)
ircd::m::user::notifications::type_prefix
{
	"ircd.push.note"
};

ircd::string_view
ircd::m::user::notifications::make_type(const mutable_buffer &buf,
                                        const opts &opts)
{
	return fmt::sprintf
	{
		buf, "%s%s%s",
		type_prefix,
		opts.only? "."_sv : string_view{},
		opts.only,
	};
}

bool
ircd::m::user::notifications::empty(const opts &opts)
const
{
	return !for_each(opts, [&]
	(const auto &, const auto &)
	{
		return false;
	});
}

size_t
ircd::m::user::notifications::count(const opts &opts)
const
{
	size_t ret(0);
	for_each(opts, [&ret]
	(const auto &, const auto &)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
ircd::m::user::notifications::for_each(const opts &opts,
                                       const closure_bool &closure)
const
{
	const user::room user_room
	{
		user
	};

	char type_buf[event::TYPE_MAX_SIZE];
	const auto type
	{
		make_type(type_buf, opts)
	};

	const room::type events
	{
		user_room, type
	};

	return events.for_each([&opts, &closure]
	(const auto &type, const auto &depth, const auto &event_idx)
	{
		if(opts.from && event_idx > opts.from) //TODO: XXX
			return true;

		if(opts.to && event_idx <= opts.to) //TODO: XXX
			return false;

		bool ret{true};
		m::get(std::nothrow, event_idx, "content", [&ret, &closure, &event_idx]
		(const json::object &content)
		{
			ret = closure(event_idx, content);
		});

		return ret;
	});
}
