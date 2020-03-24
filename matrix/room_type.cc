// The Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

bool
ircd::m::room::type::prefetch(const room::id &room_id,
                              const string_view &type)
{
	return prefetch(room_id, type, -1L);
}

bool
ircd::m::room::type::prefetch(const room::id &room_id,
                              const string_view &type,
                              const int64_t &depth)
{
	char buf[dbs::ROOM_TYPE_KEY_MAX_SIZE];
	const string_view &key
	{
		dbs::room_type_key(buf, room_id, type, depth, -1L)
	};

	return db::prefetch(dbs::room_type, key);
}

bool
ircd::m::room::type::empty()
const
{
	return for_each([]
	(const auto &type, const auto &depth, const auto &event_idx)
	{
		return false;
	});
}

size_t
ircd::m::room::type::count()
const
{
	size_t ret(0);
	for_each([&ret]
	(const auto &type, const auto &depth, const auto &event_idx)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
ircd::m::room::type::for_each(const closure &closure)
const
{
	char buf[dbs::ROOM_TYPE_KEY_MAX_SIZE];
	const string_view &key
	{
		dbs::room_type_key(buf, room.room_id, _type, range.first, -1UL)
	};

	auto it
	{
		dbs::room_type.begin(key)
	};

	for(; it; ++it)
	{
		const auto &[_type, depth, event_idx]
		{
			dbs::room_type_key(it->first)
		};

		if(int64_t(depth) <= range.second)
			break;

		if(this->_type && !prefixing && this->_type != _type)
			break;

		if(this->_type && prefixing && !startswith(_type, this->_type))
			break;

		if(!closure(_type, depth, event_idx))
			return false;
	}

	return true;
}
