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
ircd::m::cached(const id::event &event_id)
{
	return cached(event_id, event::fetch::default_opts);
}

bool
ircd::m::cached(const id::event &event_id,
                const event::fetch::opts &opts)
{
	if(!db::cached(dbs::event_idx, event_id, opts.gopts))
		return false;

	const event::idx event_idx
	{
		index(std::nothrow, event_id)
	};

	return event_idx?
		cached(event_idx, opts.gopts):
		false;
}

bool
ircd::m::cached(const event::idx &event_idx)
{
	return cached(event_idx, event::fetch::default_opts);
}

bool
ircd::m::cached(const event::idx &event_idx,
                const event::fetch::opts &opts)
{
	const byte_view<string_view> &key
	{
		event_idx
	};

	if(event::fetch::should_seek_json(opts))
		return db::cached(dbs::event_json, key, opts.gopts);

	const auto &selection
	{
		opts.keys
	};

	const auto result
	{
		cached_keys(event_idx, opts)
	};

	for(size_t i(0); i < selection.size(); ++i)
	{
		auto &column
		{
			dbs::event_column.at(i)
		};

		if(column && selection.test(i) && !result.test(i))
		{
			if(!db::has(column, key, opts.gopts))
				continue;

			return false;
		}
	}

	return true;
}

ircd::m::event::keys::selection
ircd::m::cached_keys(const event::idx &event_idx,
                     const event::fetch::opts &opts)
{
	const byte_view<string_view> &key
	{
		event_idx
	};

	event::keys::selection ret(0);
	const event::keys::selection &selection(opts.keys);
	for(size_t i(0); i < selection.size(); ++i)
	{
		auto &column
		{
			dbs::event_column.at(i)
		};

		if(column && db::cached(column, key, opts.gopts))
			ret.set(i);
	}

	return ret;
}
