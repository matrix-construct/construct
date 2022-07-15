// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

static m::resource::response
get__keys_changes(client &client,
                  const m::resource::request &request);

mapi::header
IRCD_MODULE
{
	"Client 14.11.5.2 :Key management API"
};

ircd::m::resource
changes_resource
{
	"/_matrix/client/r0/keys/changes",
	{
		"(14.11.5.2.4) Keys changes",
	}
};

m::resource::method
method_get
{
	changes_resource, "GET", get__keys_changes,
	{
		method_get.REQUIRES_AUTH
	}
};

m::resource::response
get__keys_changes(client &client,
                  const m::resource::request &request)
{
	char buf[2][128];
	const string_view range[2]
	{
		url::decode(buf[0], request.query["from"]),
		url::decode(buf[1], request.query["to"])
	};

	const auto from
	{
		m::sync::sequence(m::sync::make_since(range[0]))
	};

	const auto to
	{
		m::sync::sequence(m::sync::make_since(range[1]))
	};

	const m::user::mitsein mitsein
	{
		request.user_id
	};

	m::resource::response::chunked::json response
	{
		client, http::OK
	};

	if(likely(request.query.get("changed", true)))
	{
		json::stack::array out
		{
			response, "changed"
		};

		m::events::type::for_each_in("ircd.device.keys", [&from, &to, &out, &mitsein]
		(const string_view &type, const m::event::idx &event_idx)
		{
			if(event_idx < from || event_idx >= to)
				return true;

			const bool room_id_prefetched
			{
				m::prefetch(event_idx, "room_id")
			};

			m::get(std::nothrow, event_idx, "sender", [&out, &mitsein, &event_idx]
			(const m::user::id &user_id)
			{
				m::get(std::nothrow, event_idx, "room_id", [&out, &mitsein, &user_id]
				(const m::room::id &room_id)
				{
					if(!m::user::room::is(room_id, user_id))
						return;

					if(!mitsein.has(user_id))
						return;

					out.append(user_id);
				});
			});

			return true;
		});
	}

	if(likely(request.query.get("left", true)))
	{
		json::stack::array out
		{
			response, "left"
		};
	}

	return response;
}
