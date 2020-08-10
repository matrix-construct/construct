// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::string_view
ircd::m::user::filter::set(const mutable_buffer &buf,
                           const json::object &val)
const
{
	return set(buf, user, val);
}

ircd::string_view
ircd::m::user::filter::set(const mutable_buffer &idbuf,
                           const m::user &user,
                           const json::object &filter)
{
	const user::room user_room
	{
		user
	};

	const sha256::buf hash
	{
		sha256{filter}
	};

	const string_view filter_id
	{
		b64::encode_unpadded<b64::urlsafe>(idbuf, hash)
	};

	//TODO: ABA
	if(user_room.has("ircd.filter", filter_id))
		return filter_id;

	//TODO: ABA
	send(user_room, user.user_id, "ircd.filter", filter_id, filter);
	return filter_id;
}

std::string
ircd::m::user::filter::get(const string_view &id)
const
{
	std::string ret;
	get(std::nothrow, id, [&ret]
	(const string_view &id, const json::object &val)
	{
		ret = val;
	});

	return ret;
}

ircd::json::object
ircd::m::user::filter::get(const mutable_buffer &out,
                           const string_view &id)
const
{
	json::object ret;
	get(std::nothrow, id, [&out, &ret]
	(const string_view &id, const json::object &val)
	{
		ret = string_view { data(out), copy(out, val) };
	});

	return ret;
}

void
ircd::m::user::filter::get(const string_view &id,
                           const closure &closure)
const
{
	if(!get(std::nothrow, user, id, closure))
		throw m::NOT_FOUND
		{
			"filter id '%s' for user %s not found",
			id,
			string_view{user.user_id}
		};
}

bool
ircd::m::user::filter::get(std::nothrow_t,
                           const string_view &id,
                           const closure &closure)
const
{
	return get(std::nothrow, user, id, closure);
}

bool
ircd::m::user::filter::for_each(const closure_bool &closure)
const
{
	return for_each(user, closure);
}

bool
ircd::m::user::filter::get(std::nothrow_t,
                           const m::user &user,
                           const string_view &filter_id,
                           const closure &closure)
{
	static const event::fetch::opts fopts
	{
		event::keys::include {"content"}
	};

	const user::room user_room
	{
		user, nullptr, &fopts
	};

	return user_room.get(std::nothrow, "ircd.filter", filter_id, [&filter_id, &closure]
	(const m::event &event)
	{
		const json::object &content
		{
			at<"content"_>(event)
		};

		closure(filter_id, content);
	});
}

bool
ircd::m::user::filter::for_each(const m::user &user,
                                const closure_bool &closure)
{
	const user::room user_room
	{
		user
	};

	const room::state state
	{
		user_room
	};

	return state.for_each("ircd.filter", event::closure_idx_bool{[&closure]
	(const m::event::idx &event_idx)
	{
		static const event::fetch::opts fopts
		{
			event::keys::include {"state_key", "content"}
		};

		const event::fetch event
		{
			std::nothrow, event_idx, fopts
		};

		if(!event.valid)
			return true;

		const auto &filter_id
		{
			at<"state_key"_>(event)
		};

		const json::object &content
		{
			at<"content"_>(event)
		};

		return closure(filter_id, content);
	}});
}
