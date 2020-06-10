// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
//
// event/prev.h
//

bool
ircd::m::event::prev::prev_exist()
const
{
	for(size_t i(0); i < prev_events_count(); ++i)
		if(prev_event_exists(i))
			return true;

	return false;
}

bool
ircd::m::event::prev::auth_exist()
const
{
	for(size_t i(0); i < auth_events_count(); ++i)
		if(auth_event_exists(i))
			return true;

	return false;
}

size_t
ircd::m::event::prev::prev_events_exist()
const
{
	// the spec max is really 20 but we accept a little more in this
	// subroutine for whatever forward reason...
	static const auto max
	{
		32UL
	};

	const auto num
	{
		std::min(prev_events_count(), max)
	};

	size_t i(0);
	event::id ids[num];
	std::generate(ids, ids + num, [this, &i]
	{
		return prev_event(i++);
	});

	const auto mask
	{
		m::exists({ids, i})
	};

	const auto ret
	{
		__builtin_popcountl(mask)
	};

	assert(size_t(ret) <= max && size_t(ret) <= prev_events_count());
	return ret;
}

size_t
ircd::m::event::prev::auth_events_exist()
const
{
	// the de facto max is really 4 but we accept a little more in this
	// subroutine for whatever forward reason...
	static const auto max
	{
		8UL
	};

	const auto num
	{
		std::min(auth_events_count(), max)
	};

	size_t i(0);
	event::id ids[num];
	std::generate(ids, ids + num, [this, &i]
	{
		return auth_event(i++);
	});

	const auto mask
	{
		m::exists({ids, i})
	};

	const auto ret
	{
		__builtin_popcountl(mask)
	};

	assert(size_t(ret) <= max && size_t(ret) <= auth_events_count());
	return ret;
}

bool
ircd::m::event::prev::prev_event_exists(const size_t &idx)
const
{
	return m::exists(prev_event(idx));
}

bool
ircd::m::event::prev::auth_event_exists(const size_t &idx)
const
{
	return m::exists(auth_event(idx));
}

bool
ircd::m::event::prev::prev_events_has(const event::id &event_id)
const
{
	for(size_t i(0); i < prev_events_count(); ++i)
		if(prev_event(i) == event_id)
			return true;

	return false;
}

bool
ircd::m::event::prev::auth_events_has(const event::id &event_id)
const
{
	for(size_t i(0); i < auth_events_count(); ++i)
		if(auth_event(i) == event_id)
			return true;

	return false;
}

size_t
ircd::m::event::prev::prev_events_count()
const
{
	return json::get<"prev_events"_>(*this).count();
}

size_t
ircd::m::event::prev::auth_events_count()
const
{
	return json::get<"auth_events"_>(*this).count();
}

ircd::m::event::id
ircd::m::event::prev::auth_event(const size_t &idx)
const
{
	return std::get<0>(auth_events(idx));
}

ircd::m::event::id
ircd::m::event::prev::prev_event(const size_t &idx)
const
{
	return std::get<0>(prev_events(idx));
}

std::tuple<ircd::m::event::id, ircd::json::object>
ircd::m::event::prev::auth_events(const size_t &idx)
const
{
	const string_view &prev_
	{
		json::at<"auth_events"_>(*this).at(idx)
	};

	switch(json::type(prev_))
	{
		// v1 event format
		case json::ARRAY:
		{
			const json::array &prev(prev_);
			const json::string &prev_id(prev.at(0));
			return {prev_id, prev[1]};
		}

		// v3/v4 event format
		case json::STRING:
		{
			const json::string &prev_id(prev_);
			return {prev_id, string_view{}};
		}

		default: throw m::INVALID_MXID
		{
			"auth_events[%zu] is invalid", idx
		};
	}
}

std::tuple<ircd::m::event::id, ircd::json::object>
ircd::m::event::prev::prev_events(const size_t &idx)
const
{
	const string_view &prev_
	{
		json::at<"prev_events"_>(*this).at(idx)
	};

	switch(json::type(prev_))
	{
		// v1 event format
		case json::ARRAY:
		{
			const json::array &prev(prev_);
			const json::string &prev_id(prev.at(0));
			return {prev_id, prev[1]};
		}

		// v3/v4 event format
		case json::STRING:
		{
			const json::string &prev_id(prev_);
			return {prev_id, string_view{}};
		}

		default: throw m::INVALID_MXID
		{
			"prev_events[%zu] is invalid", idx
		};
	}
}

bool
ircd::m::for_each(const event::prev &prev,
                  const event::id::closure_bool &closure)
{
	return json::until(prev, [&closure]
	(const auto &key, const json::array &prevs)
	{
		for(const string_view &prev_ : prevs)
		{
			switch(json::type(prev_))
			{
				// v1 event format
				case json::ARRAY:
				{
					const json::array &prev(prev_);
					const json::string &prev_id(prev.at(0));
					if(!closure(prev_id))
						return false;

					continue;
				}

				// v3/v4 event format
				case json::STRING:
				{
					const json::string &prev(prev_);
					if(!closure(prev))
						return false;

					continue;
				}

				default:
					continue;
			}
		}

		return true;
	});
}
