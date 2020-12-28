// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
//
// m/filter.h
//

//TODO: globular expression
//TODO: tribool for contains_url; we currently ignore the false value.
bool
ircd::m::match(const room_event_filter &filter,
               const event &event)
noexcept
{
	if(json::get<"contains_url"_>(filter) == true)
		if(!json::get<"content"_>(event).has("url"))
			return false;

	for(const json::string room_id : json::get<"not_rooms"_>(filter))
		if(json::get<"room_id"_>(event) == room_id)
			return false;

	if(empty(json::get<"rooms"_>(filter)))
		return match(event_filter{filter}, event);

	for(const json::string room_id : json::get<"rooms"_>(filter))
		if(json::get<"room_id"_>(event) == room_id)
			return match(event_filter{filter}, event);

	return false;
}

//TODO: globular expression
bool
ircd::m::match(const event_filter &filter,
               const event &event)
noexcept
{
	for(const json::string type : json::get<"not_types"_>(filter))
		if(json::get<"type"_>(event) == type)
			return false;

	for(const json::string sender : json::get<"not_senders"_>(filter))
		if(json::get<"sender"_>(event) == sender)
			return false;

	if(empty(json::get<"senders"_>(filter)) && empty(json::get<"types"_>(filter)))
		return true;

	if(empty(json::get<"senders"_>(filter)))
	{
		for(const json::string type : json::get<"types"_>(filter))
			if(json::get<"type"_>(event) == type)
				return true;

		return false;
	}

	if(empty(json::get<"types"_>(filter)))
	{
		for(const json::string sender : json::get<"senders"_>(filter))
			if(json::get<"sender"_>(event) == sender)
				return true;

		return false;
	}

	return true;
}

//
// filter
//

/// Convenience interface for filters out of common `?filter=` query string
/// arguments. This function expects a raw urlencoded value of the filter
/// query parameter. It detects if the value is an "inline" filter by being
/// a valid JSON object; otherwise it considers the value an ID and fetches
/// the filter stored previously by the user.
std::string
ircd::m::filter::get(const string_view &val,
                     const m::user &user)
{
	if(!val)
		return {};

	const bool is_inline
	{
		startswith(val, "{") || startswith(val, "%7B")
	};

	if(is_inline)
		return util::string(val.size(), [&val]
		(const mutable_buffer &buf)
		{
			return url::decode(buf, val);
		});

	if(!user.user_id)
		return {};

	char idbuf[m::event::STATE_KEY_MAX_SIZE];
	const string_view &id
	{
		url::decode(idbuf, val)
	};

	const m::user::filter filter
	{
		user
	};

	return filter.get(id);
}

//
// filter::filter
//

ircd::m::filter::filter(const user &user,
                        const string_view &filter_id,
                        const mutable_buffer &buf)
{
	const json::object &obj
	{
		user::filter(user).get(buf, filter_id)
	};

	new (this) m::filter
	{
		obj
	};
}

//
// room_filter
//

ircd::m::room_filter::room_filter(const mutable_buffer &buf,
                                  const json::members &members)
:super_type::tuple
{
	json::stringify(mutable_buffer{buf}, members)
}
{
}

//
// state_filter
//

ircd::m::state_filter::state_filter(const mutable_buffer &buf,
                                    const json::members &members)
:super_type::tuple
{
	json::stringify(mutable_buffer{buf}, members)
}
{
}

//
// room_event_filter
//

ircd::m::room_event_filter::room_event_filter(const mutable_buffer &buf,
                                              const json::members &members)
:super_type::tuple
{
	json::stringify(mutable_buffer{buf}, members)
}
{
}

//
// event_filter
//

ircd::m::event_filter::event_filter(const mutable_buffer &buf,
                                    const json::members &members)
:super_type::tuple
{
	json::stringify(mutable_buffer{buf}, members)
}
{
}
