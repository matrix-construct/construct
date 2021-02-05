// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::bridge
{
	static thread_local char tmp[4][512];
}

decltype(ircd::m::bridge::log)
ircd::m::bridge::log
{
	"m.bridge"
};

ircd::json::object
ircd::m::bridge::protocol(const mutable_buffer &buf,
                          const config &config,
                          const string_view &name)
{
	const string_view &uri
	{
		make_uri(tmp[0], config, fmt::sprintf
		{
			tmp[1], "thirdparty/protocol/%s",
			url::encode(tmp[2], name),
		})
	};

	switch(const query query{config, uri, buf}; query.code)
	{
		case http::OK:
			return json::object
			{
				query.request.in.content
			};

		default:
			throw http::error
			{
				query.code
			};
	}
}

bool
ircd::m::bridge::exists(const config &config,
                        const m::user::id &user_id)
{
	const string_view &uri
	{
		make_uri(tmp[0], config, fmt::sprintf
		{
			tmp[1], "users/%s",
			url::encode(tmp[2], user_id),
		})
	};

	switch(const query query{config, uri}; query.code)
	{
		case http::OK:          return true;
		case http::NOT_FOUND:   return false;
		default:
			throw http::error
			{
				query.code
			};
	}
}

bool
ircd::m::bridge::exists(const config &config,
                        const m::room::alias &room_alias)
{
	const string_view &uri
	{
		make_uri(tmp[0], config, fmt::sprintf
		{
			tmp[1], "rooms/%s",
			url::encode(tmp[2], room_alias),
		})
	};

	switch(const query query{config, uri}; query.code)
	{
		case http::OK:          return true;
		case http::NOT_FOUND:   return false;
		default:
			throw http::error
			{
				query.code
			};
	}
}

ircd::string_view
ircd::m::bridge::make_uri(const mutable_buffer &buf,
                          const config &config,
                          const string_view &path)
{
	const rfc3986::uri base_url
	{
		at<"url"_>(config)
	};

	char hs_token[256];
	return fmt::sprintf
	{
		buf, "%s/_matrix/app/v1/%s?access_token=%s",
		base_url.path,
		path,
		url::encode(hs_token, at<"hs_token"_>(config)),
	};
}

//
// query
//

decltype(ircd::m::bridge::query::timeout)
ircd::m::bridge::query::timeout
{
	{ "name",      "ircd.m.bridge.query.timeout"  },
	{ "default",   10L                            },
};

ircd::m::bridge::query::query(const config &config,
                              const string_view &uri,
                              const mutable_buffer &in_body)
:base_url
{
	at<"url"_>(config)
}
,buf
{
	8_KiB
}
,uri
{
	uri
}
,wb
{
	buf
}
,hypertext
{
	wb,
	base_url.remote,
	"GET",
	uri,
}
,sopts
{
	false, // http_exceptions
}
,request
{
	net::hostport  { base_url.remote                                          },
	server::out    { wb.completed(),  {}                                      },
	server::in     { wb.remains(),    !empty(in_body)? in_body: wb.remains()  },
	&sopts,
}
,code
{
	request.get(seconds(timeout))
}
{
}

//
// config
//

bool
ircd::m::bridge::config::exists(const string_view &id)
{
	return get(std::nothrow, id, []
	(const auto &, const auto &, const auto &)
	{
	});
}

void
ircd::m::bridge::config::get(const string_view &id,
                             const closure &closure)
{
	if(unlikely(!get(std::nothrow, id, closure)))
		throw m::NOT_FOUND
		{
			"No bridge config found for '%s'",
			id,
		};
}

bool
ircd::m::bridge::config::get(std::nothrow_t,
                             const string_view &id,
                             const closure &closure)
{
	return !config::for_each([&id, &closure]
	(const auto &event_idx, const auto &event, const config &config)
	{
		if(json::get<"id"_>(config) != id)
			return true;

		closure(event_idx, event, config);
		return false;
	});
}

bool
ircd::m::bridge::config::for_each(const closure_bool &closure)
{
	return events::type::for_each_in("ircd.bridge", [&closure]
	(const string_view &, const event::idx &event_idx)
	{
		const m::event::fetch event
		{
			std::nothrow, event_idx
		};

		if(unlikely(!event.valid))
			return true;

		if(!my(event))
			return true;

		if(!defined(json::get<"state_key"_>(event)))
			return true;

		const bridge::config config
		{
			json::get<"content"_>(event)
		};

		if(!json::get<"id"_>(config))
			return true;

		// the state_key has to match the id for now
		if(json::get<"id"_>(config) != json::get<"state_key"_>(event))
			return true;

		// filter replaced state
		if(room::state::next(event_idx))
			return true;

		// filter redacted state
		if(redacted(event_idx))
			return true;

		const user::id sender
		{
			json::get<"sender"_>(event)
		};

		if(!is_oper(sender))
			return true;

		if(likely(closure(event_idx, event, config)))
			return true;

		return false;
	});
}
