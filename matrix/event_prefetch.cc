// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

bool
ircd::m::prefetch(const event::id &event_id,
                  const event::fetch::opts &opts)
{
	if(prefetch(event_id, "_event_idx"))
		return true;

	if(prefetch(index(std::nothrow, event_id), opts))
		return true;

	return false;
}

bool
ircd::m::prefetch(const event::id &event_id,
                  const string_view &key)
{
	if(db::prefetch(dbs::event_idx, event_id))
		return true;

	if(key == "_event_idx")
		return false;

	if(prefetch(index(std::nothrow, event_id), key))
		return true;

	return false;
}

bool
ircd::m::prefetch(const event::idx &event_idx,
                  const event::fetch::opts &opts)
{
	if(event::fetch::should_seek_json(opts))
	{
		if(!event_idx)
			return false;

		return db::prefetch(dbs::event_json, byte_view<string_view>{event_idx});
	}

	const event::keys keys
	{
		opts.keys
	};

	const vector_view<const string_view> cols
	{
		keys
	};

	bool ret{false};
	for(const auto &col : cols)
		if(col)
			ret |= prefetch(event_idx, col);

	return ret;
}

bool
ircd::m::prefetch(const event::idx &event_idx,
                  const string_view &key)
{
	const auto &column_idx
	{
		json::indexof<event>(key)
	};

	if(column_idx >= dbs::event_column.size())
		return prefetch(event_idx, event::fetch::default_opts);

	auto &column
	{
		dbs::event_column.at(column_idx)
	};

	if(!event_idx)
		return false;

	return db::prefetch(column, byte_view<string_view>{event_idx});
}
