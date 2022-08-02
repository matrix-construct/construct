// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::string_view
ircd::m::room::message::body()
const noexcept
{
	const auto reply_to_body
	{
		this->reply_to_body()
	};

	string_view body
	{
		json::get<"body"_>(*this)
	};

	if(reply_to_body)
	{
		body = string_view
		{
			reply_to_body.end(), body.end()
		};

		body = lstrip(body, "\\n", 2);
	}

	return body;
}

ircd::string_view
ircd::m::room::message::reply_to_body()
const noexcept
{
	const auto reply_to_user
	{
		this->reply_to_user()
	};

	if(likely(!reply_to_user))
		return {};

	string_view body
	{
		json::get<"body"_>(*this)
	};

	body = string_view
	{
		reply_to_user.end(), body.end()
	};

	body = lstrip(body, '>', 1);
	body = lstrip(body, ' ', 1);
	body = string_view
	{
		body.begin(), token(body, "\\n", 0).end()
	};

	tokens(body, "\\n", [&body]
	(const string_view &line) noexcept
	{
		if(!startswith(line, '>'))
			return false;

		body = string_view
		{
			body.begin(), line.end()
		};

		return true;
	});

	return body;
}

ircd::m::user::id
ircd::m::room::message::reply_to_user()
const noexcept
{
	string_view body
	{
		json::get<"body"_>(*this)
	};

	if(!startswith(body, '>'))
		return {};

	body = lstrip(body, '>', 1);
	body = lstrip(body, ' ', 1);
	body = lstrip(body, '*', 1);
	body = lstrip(body, ' ', 1);
	const string_view ret
	{
		between(body, '<', '>')
	};

	if(!valid(m::id::USER, ret))
		return {};

	return ret;
}

ircd::m::event::id
ircd::m::room::message::reply_to_event()
const noexcept try
{
	const json::object &m_relates_to
	{
		json::get<"m.relates_to"_>(*this)
	};

	if(!m_relates_to || !json::type(m_relates_to, json::OBJECT))
		return {};

	const json::object &m_in_reply_to
	{
		m_relates_to["m.in_reply_to"]
	};

	const auto &event_id
	{
		json::type(m_in_reply_to, json::OBJECT)?
			m_in_reply_to.get<json::string>("event_id"):
			m_relates_to.get<json::string>("event_id")
	};

	if(!event_id || !valid(m::id::EVENT, event_id))
		return {};

	return event_id;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Failed to extract m.relates_to m.in_reply_to event_id :%s",
		e.what(),
	};

	return {};
}
