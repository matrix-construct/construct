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

mapi::header
IRCD_MODULE
{
	"Client 7.5 :Public Rooms"
};

const ircd::m::room::id::buf
public_room_id
{
    "public", ircd::my_host()
};

m::room public_
{
	public_room_id
};

resource
publicrooms_resource
{
	"/_matrix/client/r0/publicRooms",
	{
		"(7.5) Lists the public rooms on the server. "
	}
};

conf::item<size_t>
flush_hiwat
{
	{ "name",     "ircd.client.publicrooms.flush.hiwat" },
	{ "default",  16384L                                },
};

static resource::response
post__publicrooms_since(client &client,
                        const resource::request &request,
                        const string_view &since);

static resource::response
post__publicrooms_remote(client &client,
                         const resource::request &request,
                         const string_view &server,
                         const string_view &search,
                         const size_t &limit);

resource::response
post__publicrooms(client &client,
                  const resource::request &request)
{
	const string_view &since
	{
		unquote(request["since"])
	};

	const auto &server
	{
		request.query["server"]
	};

	const json::object &filter
	{
		request["filter"]
	};

	const string_view &search_term
	{
		unquote(filter["generic_search_term"])
	};

	const uint8_t limit
	{
		uint8_t(request.get<ushort>("limit", 16U))
	};

	const bool include_all_networks
	{
		request.get<bool>("include_all_networks", false)
	};

	if(server && server != my_host())
		return post__publicrooms_remote(client, request, server, search_term, limit);

	resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher(), size_t(flush_hiwat)
	};

	size_t count{0};
	m::room::id::buf prev_batch_buf;
	m::room::id::buf next_batch_buf;
	const m::room::state publix
	{
		public_
	};

	json::stack::object top{out};
	{
		json::stack::member chunk_m{top, "chunk"};
		json::stack::array chunk{chunk_m};
		publix.for_each("ircd.room", since, [&](const m::event &event)
		{
			const m::room::id room_id{at<"state_key"_>(event)};
			const m::room::state state{room_id};
			json::stack::object obj{chunk};

			if(!count && !empty(since))
				prev_batch_buf = room_id; //TODO: ???

			// Aliases array
			{
				json::stack::member aliases_m{obj, "aliases"};
				json::stack::array array{aliases_m};
				state.for_each("m.room.aliases", [&array]
				(const m::event &event)
				{
					const json::array aliases
					{
						json::get<"content"_>(event).get("aliases")
					};

					for(const string_view &alias : aliases)
						array.append(unquote(alias));
				});
			}

			state.get(std::nothrow, "m.room.avatar_url", "", [&obj]
			(const m::event &event)
			{
				json::stack::member
				{
					obj, "avatar_url", unquote(json::get<"content"_>(event).get("url"))
				};
			});

			state.get(std::nothrow, "m.room.canonical_alias", "", [&obj]
			(const m::event &event)
			{
				json::stack::member
				{
					obj, "canonical_alias", unquote(json::get<"content"_>(event).get("alias"))
				};
			});

			state.get(std::nothrow, "m.room.guest_access", "", [&obj]
			(const m::event &event)
			{
				json::stack::member
				{
					obj, "guest_can_join", json::value
					{
						unquote(json::get<"content"_>(event).get("guest_access")) == "can_join"
					}
				};
			});

			state.get(std::nothrow, "m.room.name", "", [&obj]
			(const m::event &event)
			{
				json::stack::member
				{
					obj, "name", json::value
					{
						unquote(json::get<"content"_>(event).get("name"))
					}
				};
			});

			// num_join_members
			{
				const m::room::members members{room_id};
				json::stack::member
				{
					obj, "num_joined_members", json::value
					{
						long(members.count("join"))
					}
				};
			}

			json::stack::member
			{
				obj, "room_id", room_id
			};

			state.get(std::nothrow, "m.room.topic", "", [&obj]
			(const m::event &event)
			{
				json::stack::member
				{
					obj, "topic", unquote(json::get<"content"_>(event).get("topic"))
				};
			});

			state.get(std::nothrow, "m.room.history_visibility", "", [&obj]
			(const m::event &event)
			{
				json::stack::member
				{
					obj, "world_readable", json::value
					{
						unquote(json::get<"content"_>(event).get("history_visibility")) == "world_readable"
					}
				};
			});

			next_batch_buf = room_id;
			return count < limit;
		});
	}

	json::stack::member
	{
		top, "total_room_count_estimate", json::value
		{
			long(publix.count("ircd.room"))
		}
	};

	if(prev_batch_buf)
		json::stack::member
		{
			top, "prev_batch", prev_batch_buf
		};

	if(count >= limit)
		json::stack::member
		{
			top, "next_batch", next_batch_buf
		};

	return {};
}

resource::method
post_method
{
	publicrooms_resource, "POST", post__publicrooms
};

resource::response
get__publicrooms(client &client,
                 const resource::request &request)
{
	const auto &server
	{
		request.query["server"]
	};

	const auto &since
	{
		request.query["since"]
	};

	const uint8_t limit
	{
		request.query["limit"]?
			uint8_t(lex_cast<ushort>(request.query["limit"])):
			uint8_t(16U)
	};

	std::vector<json::value> chunk;
	const string_view next_batch;
	const string_view prev_batch;
	const int64_t total_room_count_estimate{0};

	return resource::response
	{
		client, json::members
		{
			{ "chunk",                      { chunk.data(), chunk.size() } },
			{ "next_batch",                 next_batch                     },
			{ "prev_batch",                 prev_batch                     },
			{ "total_room_count_estimate",  total_room_count_estimate      },
		}
	};
}

resource::method
get_method
{
	publicrooms_resource, "GET", get__publicrooms
};

static resource::response
post__publicrooms_remote(client &client,
                         const resource::request &request,
                         const string_view &server,
                         const string_view &search,
                         const size_t &limit)
{
	std::vector<json::value> chunk;
	int64_t total_room_count_estimate{0};
	const string_view next_batch;
	const string_view prev_batch;

	return resource::response
	{
		client, json::members
		{
			{ "chunk",                      { chunk.data(), chunk.size() } },
			{ "next_batch",                 next_batch                     },
			{ "prev_batch",                 prev_batch                     },
			{ "total_room_count_estimate",  total_room_count_estimate      },
		}
	};
}

static void
_create_public_room(const m::event &)
{
	m::create(public_room_id, m::me.user_id);
}

/// Create the public rooms room at the appropriate time on startup.
/// The startup event chosen here is when @ircd joins the !ircd room,
/// which is a fundamental notification toward the end of init.
const m::hookfn<>
_create_public_hook
{
	_create_public_room,
	{
		{ "_site",       "vm.notify"      },
		{ "room_id",     "!ircd"          },
		{ "type",        "m.room.create"  },
	}
};
