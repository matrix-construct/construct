// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	static resource::response
	get_notifications(client &,
	                  const resource::request &);

	extern const event::keys notification_event_keys;
	extern conf::item<size_t> notifications_limit_default;
	extern resource::method notifications_method_get;
	extern resource notifications_resource;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client r0.6.0-13.13.1.3.1 :Listing Notifications"
};

decltype(ircd::m::notifications_resource)
ircd::m::notifications_resource
{
	"/_matrix/client/r0/notifications",
	{
		"(r0.6.0-13.13.1.3.1) Listing Notifications"
	}
};

decltype(ircd::m::notifications_method_get)
ircd::m::notifications_method_get
{
	notifications_resource, "GET", get_notifications,
	{
		notifications_method_get.REQUIRES_AUTH
	}
};

decltype(ircd::m::notifications_limit_default)
ircd::m::notifications_limit_default
{
	{ "name",    "ircd.client.notifications.limit.default" },
	{ "default",  12                                       }
};

decltype(ircd::m::notification_event_keys)
ircd::m::notification_event_keys
{
	event::keys::include
	{
		"event_id",
		"content",
		"origin_server_ts",
		"sender",
		"state_key",
		"type",
	}
};

ircd::m::resource::response
ircd::m::get_notifications(client &client,
                           const resource::request &request)
{
	const string_view &from
	{
		request.query["from"]
	};

	const string_view &only
	{
		request.query["only"]
	};

	const ushort limit
	{
		request.query.get<ushort>("limit", size_t(notifications_limit_default))
	};

	const m::user::notifications::opts opts
	{
		.from = from? lex_cast<event::idx>(from): 0,
		.to = 0,
		.only = only,
	};

	const m::user::notifications notifications
	{
		request.user_id
	};

	resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher()
	};

	json::stack::object top
	{
		out
	};

	size_t count(0);
	bool finished(true);
	event::idx next_token(0);
	{
		json::stack::array array
		{
			top, "notifications"
		};

		finished = notifications.for_each(opts, [&]
		(const event::idx &note_idx, const json::object &note)
		{
			assert(note_idx);
			next_token = note_idx;
			if(count >= limit)
				return false;

			const event::idx &event_idx
			{
				note.get<event::idx>("event_idx")
			};

			const event::idx &rule_idx
			{
				note.get<event::idx>("rule_idx")
			};

			//if(m::redacted(event_idx))
			//	return true;

			const m::event::fetch event
			{
				std::nothrow, event_idx
			};

			if(unlikely(!event.valid))
				return true;

			json::stack::object object
			{
				array
			};

			json::stack::member
			{
				object, "room_id", json::get<"room_id"_>(event)
			};

			json::stack::member
			{
				object, "ts", json::value
				{
					m::get<time_t>(note_idx, "origin_server_ts")
				}
			};

			m::event::id::buf last_read;
			m::receipt::get(last_read, json::get<"room_id"_>(event), request.user_id);
			json::stack::member
			{
				object, "read", json::value
				{
					last_read && index(std::nothrow, last_read) >= event_idx
				}
			};

			//TODO: get from rule_idx
			json::stack::member
			{
				object, "actions", json::value
				{
					opts.only == "highlight"?
						R"(["notify",{"set_tweak":"highlight","value":true}])":
						R"(["notify"])"
				}
			};

			//TODO: get from notification source
			json::stack::member
			{
				object, "profile_tag", json::value
				{
					nullptr
				}
			};

			json::stack::object event_object
			{
				object, "event"
			};

			m::event::append
			{
				event_object, event,
				{
					.event_idx = event_idx,
					.keys = &notification_event_keys,
					//.query_txnid = false,
					//.query_prev_state = false,
					.query_redacted = false,
				},
			};

			++count;
			return true;
		});
	}

	if(!finished)
		json::stack::member
		{
			top, "next_token", json::value
			{
				lex_cast(next_token), json::STRING
			}
		};

	top.~object();
	return response;
}
