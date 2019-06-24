// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

static void
get__initialsync_remote(client &client,
                        const resource::request &request,
                        const m::room &room);

static void
get__initialsync_local(client &client,
                       const resource::request &request,
                       const m::room &room,
                       const m::user &user,
                       json::stack::object &out);

static conf::item<size_t>
initial_backfill
{
	{ "name",      "ircd.client.rooms.initialsync.backfill" },
	{ "default",   20L                                      },
};

static conf::item<size_t>
buffer_size
{
	{ "name",     "ircd.client.rooms.initialsync.buffer_size" },
	{ "default",  long(96_KiB)                                },
};

static conf::item<size_t>
flush_hiwat
{
	{ "name",     "ircd.client.rooms.initialsync.flush.hiwat" },
	{ "default",  long(32_KiB)                                },
};

resource::response
get__initialsync(client &client,
                 const resource::request &request,
                 const m::room::id &room_id)
{
	const m::room room
	{
		room_id
	};

	if(!exists(room))
	{
		if(!my(room))
			get__initialsync_remote(client, request, room);
		else
			throw m::NOT_FOUND
			{
				"room_id '%s' does not exist.", string_view{room_id}
			};
	}

	resource::response::chunked response
	{
		client, http::OK, buffer_size
	};

	json::stack out
	{
		response.buf, response.flusher(), size_t(flush_hiwat)
	};

	json::stack::object top
	{
		out
	};

	get__initialsync_local(client, request, room, request.user_id, top);
	return std::move(response);
}

void
get__initialsync_local(client &client,
                       const resource::request &request,
                       const m::room &room,
                       const m::user &user,
                       json::stack::object &out)
{
	char membership_buf[m::MEMBERSHIP_MAX_SIZE];
	json::stack::member
	{
		out, "membership", room.membership(membership_buf, request.user_id)
	};

	json::stack::member
	{
		out, "visibility", m::rooms::is_public(room)? "public"_sv: "private"_sv
	};

	const m::user::room_account_data room_account_data
	{
		user, room
	};

	json::stack::array account_data
	{
		out, "account_data"
	};

	room_account_data.for_each([&account_data]
	(const string_view &type, const json::object &content)
	{
		json::stack::object object
		{
			account_data
		};

		json::stack::member
		{
			object, "type", type
		};

		json::stack::member
		{
			object, "content", content
		};

		return true;
	});

	const m::user::room_tags room_tags
	{
		user, room
	};

	json::stack::object tag
	{
		account_data
	};

	json::stack::member
	{
		tag, "type", "m.tag"
	};

	json::stack::object tag_content
	{
		tag, "content"
	};

	json::stack::object tags
	{
		tag_content, "tags"
	};

	room_tags.for_each([&tags]
	(const string_view &type, const json::object &content)
	{
		json::stack::member
		{
			tags, type, content
		};

		return true;
	});

	tags.~object();
	tag_content.~object();
	tag.~object();
	account_data.~array();

	json::stack::array state
	{
		out, "state"
	};

	const m::room::state room_state{room};
	room_state.for_each(m::event::id::closure_bool{[&state, &room, &user]
	(const m::event::id &event_id)
	{
		if(!visible(event_id, user.user_id))
			return true;

		const m::event::fetch event
		{
			event_id, std::nothrow
		};

		if(!event.valid)
			return true;

		m::event_append_opts opts;
		opts.event_idx = &event.event_idx;
		m::append(state, event, opts);
		return true;
	}});
	state.~array();

	m::room::messages it{room};
	json::stack::object messages
	{
		out, "messages"
	};

	if(it)
		json::stack::member
		{
			messages, "start", it.event_id()
		};

	// seek down first to give events in chronological order.
	for(size_t i(0); it && i <= size_t(initial_backfill); --it, ++i);
	if(it)
		json::stack::member
		{
			messages, "end", it.event_id()
		};

	json::stack::array chunk
	{
		messages, "chunk"
	};

	for(; it; ++it)
	{
		const auto &event_id(it.event_id());
		if(!visible(event_id, user.user_id))
			continue;

		const m::event &event(*it);
		const auto &event_idx(it.event_idx());

		m::event_append_opts opts;
		opts.event_idx = &event_idx;
		m::append(chunk, event, opts);
	}
}

void
get__initialsync_remote(client &client,
                        const resource::request &request,
                        const m::room &room)
{
	const auto head
	{
		m::v1::fetch_head(room, room.room_id.host(), request.user_id)
	};

	m::room room_{room};
	room_.event_id = head;

	const net::hostport remote
	{
		my_host(room_.event_id.host())?
			room_.room_id.host():
			room_.event_id.host()
	};

	m::fetch::state_ids(room_);
}
