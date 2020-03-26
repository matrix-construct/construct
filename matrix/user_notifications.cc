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

ircd::m::user::notifications::opts
ircd::m::user::notifications::unmake_type(const string_view &type)
{
	const string_view &room_id
	{
		type.substr(type.find('!'))
	};

	const string_view &only
	{
		lstrip(split(type, '!').first, type_prefix)
	};

	opts ret;
	ret.only = lstrip(only, '.');
	ret.room_id = room_id? room::id{room_id} : room::id{};
	return ret;
}

/// valid result examples:
///
/// ircd.push.note.highlight!AAAANTUiY1fBZ230:zemos.net
/// ircd.push.note.highlight
/// ircd.push.note!AAAANTUiY1fBZ230:zemos.net
/// ircd.push.note
ircd::string_view
ircd::m::user::notifications::make_type(const mutable_buffer &buf,
                                        const opts &opts)
{
	return fmt::sprintf
	{
		buf, "%s%s%s%s",
		type_prefix,
		opts.only? "."_sv : string_view{},
		opts.only,
		string_view{opts.room_id},
	};
}

bool
ircd::m::user::notifications::empty(const opts &opts)
const
{
	return !for_each(opts, closure_meta{[&]
	(const auto &, const auto &)
	{
		return false;
	}});
}

size_t
ircd::m::user::notifications::count(const opts &opts)
const
{
	size_t ret(0);
	for_each(opts, closure_meta{[&ret]
	(const auto &, const auto &)
	{
		++ret;
		return true;
	}});

	return ret;
}

bool
ircd::m::user::notifications::for_each(const opts &opts,
                                       const closure &closure)
const
{
	return for_each(opts, closure_meta{[&closure]
	(const auto &type, const auto &event_idx)
	{
		return m::query(std::nothrow, event_idx, "content", [&closure, &event_idx]
		(const json::object &content)
		{
			return closure(event_idx, content);
		});
	}});
}

bool
ircd::m::user::notifications::for_each(const opts &opts,
                                       const closure_meta &closure)
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
		user_room, type, {-1UL, -1L}, true
	};

	if(!opts.sorted || opts.room_id)
		return events.for_each([&opts, &closure]
		(const auto &type, const auto &depth, const auto &event_idx)
		{
			if(opts.from && event_idx > opts.from) //TODO: XXX
				return true;

			if(opts.to && event_idx <= opts.to) //TODO: XXX
				return false;

			return closure(type, event_idx);
		});

	std::vector<event::idx> idxs;
	idxs.reserve(events.count());
	events.for_each([&opts, &idxs]
	(const auto &type, const auto &depth, const auto &event_idx)
	{
		if(opts.from && event_idx > opts.from)
			return true;

		if(opts.to && event_idx <= opts.to)
			return true;

		idxs.emplace_back(event_idx);
		return true;
	});

	std::sort(rbegin(idxs), rend(idxs));
	for(const auto &idx : idxs)
		if(!closure(string_view{}, idx)) //NOTE: no type string to closure
			return false;

	return true;
}
