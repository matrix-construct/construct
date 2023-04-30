// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::relates::latest_column)
ircd::m::relates::latest_column
{
	{ "name",     "ircd.m.relates.latest_column" },
	{ "default",  "origin_server_ts"             },
};

bool
ircd::m::relates::prefetch(const string_view &type)
const
{
	// If the prefetch was launched, bail here without blocking below.
	if(refs.prefetch(dbs::ref::M_RELATES))
		return true;

	// The iteration is cached so we can prefetch the content now
	bool ret{false};
	refs.for_each(dbs::ref::M_RELATES, [this, &ret]
	(const auto &event_idx, const auto &)
	{
		if(this->prefetch_latest)
			ret |= m::prefetch(event_idx, string_view{latest_column});

		if(this->prefetch_sender || this->match_sender)
			ret |= m::prefetch(event_idx, "sender");

		ret |= m::prefetch(event_idx, "content");
		return true;
	});

	return ret;
}

size_t
ircd::m::relates::count(const string_view &type)
const
{
	size_t ret(0);
	for_each(type, [&ret]
	(const event::idx &, const json::object &, const m::relates_to &)
	noexcept
	{
		++ret;
		return true;
	});

	return ret;
}

bool
ircd::m::relates::has(const event::idx &idx)
const
{
	return has(string_view{}, idx);
}

bool
ircd::m::relates::has(const string_view &type)
const
{
	return !for_each(type, []
	(const event::idx &, const json::object &, const m::relates_to &)
	noexcept
	{
		return false;
	});
}

bool
ircd::m::relates::has(const string_view &type,
                      const event::idx &idx)
const
{
	return !for_each(type, [&idx]
	(const event::idx &ref_idx, const json::object &, const m::relates_to &)
	noexcept
	{
		return ref_idx != idx; // true to continue, false to break
	});
}

ircd::m::event::idx
ircd::m::relates::latest(const string_view &type,
                         uint *const at)
const
{
	const string_view &column
	{
		latest_column
	};

	if(at)
		*at = -1;

	uint i{0};
	int64_t best{0};
	event::idx ret{0};
	for_each(type, [&]
	(const event::idx &event_idx, const json::object &, const m::relates_to &)
	noexcept
	{
		int64_t val{0};
		if((val = m::get(std::nothrow, event_idx, column, val)) > best)
		{
			best = val;
			ret = event_idx;

			if(at)
				*at = i;
		}

		++i;
		return true;
	});

	return ret;
}

ircd::m::event::idx
ircd::m::relates::get(const string_view &type,
                      const uint at)
const
{
	uint ctr{0};
	event::idx ret{0};
	for_each(type, [&at, &ctr, &ret]
	(const event::idx &event_idx, const json::object &, const m::relates_to &)
	noexcept
	{
		if(ctr++ < at)
			return true;

		ret = event_idx;
		return false;
	});

	return ret;
}

bool
ircd::m::relates::_each(const string_view &rel_type,
                        const closure &closure,
                        const event::idx &event_idx)
const
{
	thread_local char buf[event::MAX_SIZE];
	const json::object content
	{
		m::get(std::nothrow, event_idx, "content", buf)
	};

	if(!content)
		return true;

	m::relates_to relates
	{
		content["m.relates_to"]
	};

	if(!json::get<"rel_type"_>(relates))
		if(json::get<"m.in_reply_to"_>(relates))
			json::get<"rel_type"_>(relates) = "m.in_reply_to";

	if(rel_type)
		if(json::get<"rel_type"_>(relates) != rel_type)
			return true;

	if(match_sender)
	{
		const pair<event::idx> idx
		{
			refs.idx, event_idx
		};

		const std::equal_to<string_view> equal_to;
		if(!m::query(std::nothrow, idx, "sender", equal_to))
			return true;
	}

	return closure(event_idx, content, relates);
}
