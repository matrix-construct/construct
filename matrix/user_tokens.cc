// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

size_t
ircd::m::user::tokens::del(const string_view &reason)
const
{
	size_t ret(0);
	for_each([this, &ret, &reason]
	(const event::idx &event_idx, const string_view &token)
	{
		ret += del(token, reason);
		return true;
	});

	return ret;
}

bool
ircd::m::user::tokens::del(const string_view &token,
                           const string_view &reason)
const
{
	const m::room::id::buf tokens_room_id
	{
		"tokens", origin(my())
	};

	const m::room tokens
	{
		tokens_room_id
	};

	const auto event_idx
	{
		tokens.get(std::nothrow, "ircd.access_token", token)
	};

	const auto match
	{
		[this](const string_view &sender)
		{
			return sender == this->user.user_id;
		}
	};

	if(unlikely(!m::query(std::nothrow, event_idx, "sender", match)))
		return false;

	const auto event_id
	{
		m::event_id(std::nothrow, event_idx)
	};

	if(unlikely(!event_id))
		return false;

	const auto redact_id
	{
		m::redact(tokens, user.user_id, event_id, reason)
	};

	return true;
}

bool
ircd::m::user::tokens::check(const string_view &token)
const
{
	const m::room::id::buf tokens_room_id
	{
		"tokens", origin(my())
	};

	const m::room tokens
	{
		tokens_room_id
	};

	const auto event_idx
	{
		tokens.get(std::nothrow, "ircd.access_token", token)
	};

	return event_idx && m::query(std::nothrow, event_idx, "sender", [this]
	(const string_view &sender)
	{
		return sender == this->user.user_id;
	});
}

bool
ircd::m::user::tokens::for_each(const closure_bool &closure)
const
{
	const m::room::id::buf tokens_room_id
	{
		"tokens", origin(my())
	};

	const m::room tokens
	{
		tokens_room_id
	};

	const m::room::state state
	{
		tokens
	};

	return state.for_each("ircd.access_token", [this, &closure]
	(const auto &type, const auto &state_key, const auto &event_idx)
	{
		const auto match
		{
			[this](const string_view &sender)
			{
				return sender == this->user.user_id;
			}
		};

		if(!m::query(std::nothrow, event_idx, "sender", match))
			return true;

		if(!closure(event_idx, state_key))
			return false;

		return true;
	});
}

ircd::m::user::id::buf
ircd::m::user::tokens::get(const string_view &token)
{
	const auto ret
	{
		get(std::nothrow, token)
	};

	if(!ret)
		throw m::error
		{
			http::UNAUTHORIZED, "M_UNKNOWN_TOKEN",
			"Credentials for this method are required but invalid."
		};

	return ret;
}

ircd::m::user::id::buf
ircd::m::user::tokens::get(std::nothrow_t,
                           const string_view &token)
{
	const m::room::id::buf tokens_room_id
	{
		"tokens", origin(my())
	};

	const m::room tokens
	{
		tokens_room_id
	};

	const event::idx event_idx
	{
		tokens.get(std::nothrow, "ircd.access_token", token)
	};

	m::user::id::buf ret;
	m::get(std::nothrow, event_idx, "sender", [&ret]
	(const string_view &sender)
	{
		ret = sender;
	});

	return ret;
}

ircd::m::device::id::buf
ircd::m::user::tokens::device(const string_view &token)
{
	const auto ret
	{
		device(std::nothrow, token)
	};

	if(unlikely(!ret))
		throw m::NOT_FOUND
		{
			"No device for this access_token"
		};

	return ret;
}

ircd::m::device::id::buf
ircd::m::user::tokens::device(std::nothrow_t,
                              const string_view &token)
{
	const m::room::id::buf tokens_room_id
	{
		"tokens", origin(my())
	};

	const m::room tokens
	{
		tokens_room_id
	};

	const event::idx event_idx
	{
		tokens.get(std::nothrow, "ircd.access_token", token)
	};

	device::id::buf ret;
	m::get(std::nothrow, event_idx, "content", [&ret]
	(const json::object &content)
	{
		const json::string &device_id
		{
			content.at("device_id")
		};

		ret = device_id;
	});

	return ret;
}

ircd::string_view
ircd::m::user::tokens::generate(const mutable_buffer &buf)
{
	static const size_t token_max
	{
		32
	};

	static const auto &token_dict
	{
		rand::dict::alpha
	};

	const mutable_buffer out
	{
		buf, token_max
	};

	return rand::string(out, token_dict);
}
