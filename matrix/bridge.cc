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
	static thread_local char tmp[2][512];
}

decltype(ircd::m::bridge::log)
ircd::m::bridge::log
{
	"m.bridge"
};

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

bool
ircd::m::bridge::exists(const string_view &id)
{
	return config::idx(std::nothrow, id);
}

//
// query
//

decltype(ircd::m::bridge::query::timeout)
ircd::m::bridge::query::timeout
{
	{ "name",      "ircd.m.bridge.query.timeout"  },
	{ "default",   5L                             },
};

ircd::m::bridge::query::query(const config &config,
                              const m::room::alias &alias)
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
	make_uri(buf, config, fmt::sprintf
	{
		tmp[0], "rooms/%s", string_view{alias}
	})
}
,wb
{
	buf + size(uri)
}
,hypertext
{
	wb,
	base_url.remote,
	"GET",
	uri,
}
,request
{
	net::hostport  { base_url.remote                },
	server::out    { wb.completed(),  {}            },
	server::in     { wb.remains(),    wb.remains()  },
}
,code
{
	request.get(seconds(timeout))
}
{
}

ircd::m::bridge::query::query(const config &config,
                              const m::user::id &user_id)
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
	make_uri(buf, config, fmt::sprintf
	{
		tmp[0], "users/%s", string_view{user_id}
	})
}
,wb
{
	buf + size(uri)
}
,hypertext
{
	wb,
	base_url.remote,
	"GET",
	uri,
}
,request
{
	net::hostport  { base_url.remote                },
	server::out    { wb.completed(),  {}            },
	server::in     { wb.remains(),    wb.remains()  },
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
ircd::m::bridge::config::for_each(const closure_bool &closure)
{
	const m::room::id::buf bridge_room_id
	{
		"bridge", my_host()
	};

	const m::room::state state
	{
		bridge_room_id
	};

	return state.for_each("ircd.bridge", [&closure]
	(const string_view &type, const string_view &state_key, const event::idx &event_idx)
	{
		bool ret{true};
		m::get(std::nothrow, event_idx, "content", [&event_idx, &closure, &ret]
		(const json::object &content)
		{
			ret = closure(event_idx, content);
		});

		return ret;
	});
}

void
ircd::m::bridge::config::get(const string_view &id,
                             const closure &closure)
{
	if(!get(std::nothrow, id, closure))
		throw m::NOT_FOUND
		{
			"Configuration for appservice '%s' not found.",
			id
		};
}

bool
ircd::m::bridge::config::get(std::nothrow_t,
                             const string_view &id,
                             const closure &closure)
{
	const auto event_idx
	{
		idx(std::nothrow, id)
	};

	return m::get(std::nothrow, event_idx, "content", [&event_idx, &closure]
	(const json::object &content)
	{
		closure(event_idx, content);
	});
}

ircd::m::event::idx
ircd::m::bridge::config::idx(const string_view &id)
{
	const auto ret
	{
		idx(std::nothrow, id)
	};

	if(!ret)
		throw m::NOT_FOUND
		{
			"Configuration for appservice '%s' not found.",
			id
		};

	return ret;
}

ircd::m::event::idx
ircd::m::bridge::config::idx(std::nothrow_t,
                             const string_view &id)
{
	const m::room::id::buf bridge_room_id
	{
		"bridge", my_host()
	};

	const m::room::state state
	{
		bridge_room_id
	};

	return state.get("ircd.bridge", id);
}
