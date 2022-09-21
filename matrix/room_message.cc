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
ircd::m::room::message::replace_body()
const noexcept
{
	const auto replace_event
	{
		this->replace_event()
	};

	if(!replace_event)
		return {};

	const json::object m_new_content
	{
		this->source["m.new_content"]
	};

	if(!json::type(m_new_content, json::OBJECT))
		return {};

	const json::string body
	{
		m_new_content["body"]
	};

	return body;
}

ircd::m::event::id
ircd::m::room::message::replace_event()
const noexcept try
{
	const m::relates_to &m_relates_to
	{
		json::get<"m.relates_to"_>(*this)
	};

	if(json::get<"rel_type"_>(m_relates_to) != "m.replace")
		return {};

	const auto &event_id
	{
		json::get<"event_id"_>(m_relates_to)
	};

	if(!event_id || !valid(m::id::EVENT, event_id))
		return {};

	return event_id;
}
catch(const json::error &e)
{
	log::derror
	{
		log, "Failed to extract m.relates_to m.replace event_id :%s",
		e.what(),
	};

	return {};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Failed to extract m.relates_to m.replace event_id :%s",
		e.what(),
	};

	return {};
}

ircd::string_view
ircd::m::room::message::reply_to_body()
const noexcept
{
	const auto reply_to_name
	{
		this->reply_to_name()
	};

	if(likely(!reply_to_name))
		return {};

	string_view body
	{
		json::get<"body"_>(*this)
	};

	body = string_view
	{
		reply_to_name.end(), body.end()
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
	const auto reply_to_name
	{
		this->reply_to_name()
	};

	if(!valid(m::id::USER, reply_to_name))
		return {};

	return reply_to_name;
}

ircd::string_view
ircd::m::room::message::reply_to_name()
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

	return ret;
}

ircd::m::event::id
ircd::m::room::message::reply_to_event()
const noexcept try
{
	const m::relates_to &m_relates_to
	{
		json::get<"m.relates_to"_>(*this)
	};

	const json::object &m_in_reply_to
	{
		json::get<"m.in_reply_to"_>(m_relates_to)
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
catch(const json::error &e)
{
	log::derror
	{
		log, "Failed to extract m.relates_to m.in_reply_to event_id :%s",
		e.what(),
	};

	return {};
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
