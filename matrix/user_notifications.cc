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

//TODO: XXX optimize
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

	const auto type_match{[&type](const auto &_type)
	{
		return _type == type;
	}};

	m::room::events it
	{
		user_room
	};

	if(opts.from)
		it.seek_idx(opts.from);

	bool ret(true);
	for(; it && ret && it.event_idx() > opts.to; --it) //TODO: XXX optimize
	{
		if(!m::query(std::nothrow, it.event_idx(), "type", type_match))
			continue;

		m::get(std::nothrow, it.event_idx(), "content", [&ret, &it, &closure]
		(const json::object &content)
		{
			ret = closure(it.event_idx(), content);
		});
	}

	return ret;
}
