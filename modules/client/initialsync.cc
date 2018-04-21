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
	"Client 6.2.3 :initialSync"
};

const string_view
initialsync_description
{R"(

6.2.3

This returns the full state for this user, with an optional limit on the number
of messages per room to return.

This endpoint was deprecated in r0 of this specification. Clients should instead
call the /sync API with no since parameter.

*** developer note:
We reuse the routines of this module for the initial sync portion of the /sync
API, and branch for their spec differences when applicable. This way the /sync
module focuses specifically on the increment aspect.

)"};

//
// sync resource
//

resource
initialsync_resource
{
	"/_matrix/client/r0/initialSync",
	{
		initialsync_description
	}
};

extern "C" resource::response
initialsync(client &client,
            const resource::request &request);

resource::method
get_initialsync
{
	initialsync_resource, "GET", initialsync,
	{
		get_initialsync.REQUIRES_AUTH,
		-1s // No timer for this method.
	}
};

conf::item<size_t>
initialsync_limit_default
{
	{ "name",     "ircd.client.initialsync.limit.default" },
	{ "default",  16L                                     },
};

conf::item<size_t>
initialsync_limit_max
{
	{ "name",     "ircd.client.initialsync.limit.max"     },
	{ "default",  64L                                     },
};

static void
_initialsync(client &client,
             const resource::request &request,
             json::stack::object &out);

resource::response
initialsync(client &client,
            const resource::request &request)
try
{
	const auto &filter_id
	{
		request.query["filter"]
	};

	const auto &set_presence
	{
		request.query.get("set_presence", "online"_sv)
	};

	const auto &limit{[&request]
	{
		auto ret(request.query.get("limit", size_t(initialsync_limit_default)));
		ret = std::min(ret, size_t(initialsync_limit_max));
		return ret;
	}()};

	// Due to the way json::stack works the chunk buffer must be at least
	// the size of an appended input (for ex. a json::tuple). In our case this
	// buffer must hold a 64_KiB worst-case event and then a little extra.
	const unique_buffer<mutable_buffer> buf
	{
		96_KiB
	};

	resource::response::chunked response
	{
		client, http::OK, "application/json; charset=utf-8"
	};

	json::stack out
	{
		buf, [&response](const const_buffer &buf)
		{
			response.write(buf);
			return buf;
		}
	};

	json::stack::object object{out};
	_initialsync(client, request, object);
	return {};
}
catch(const std::exception &e)
{
	log::error
	{
		"Initial sync for %s from %s failed because :%s",
		request.user_id,
		string(remote(client)),
		e.what()
	};

	throw;
}

static void
initialsync_rooms(client &client,
                  const resource::request &request,
                  json::stack::object &out,
                  const m::user::room &);

static void
initialsync_presence(client &client,
                     const resource::request &request,
                     json::stack::object &out,
                     const m::user &user);

static void
initialsync_account_data(client &client,
                         const resource::request &request,
                         json::stack::object &out,
                         const m::user::room &);

void
_initialsync(client &client,
             const resource::request &request,
             json::stack::object &out)
{
	const m::user user{request.user_id};
	const m::user::room user_room{user};

	// rooms
	{
		json::stack::member member{out, "rooms"};
		json::stack::object object{member};
		initialsync_rooms(client, request, object, user_room);
	}

	// presence
	{
		json::stack::member member{out, "presence"};
		json::stack::object object{member};
		initialsync_presence(client, request, object, user);
	}

	// account_data
	{
		json::stack::member member{out, "account_data"};
		json::stack::object object{member};
		initialsync_account_data(client, request, object, user_room);
	}

	// next_batch
	{
		//TODO: XXX
		const auto next_batch
		{
			int64_t(m::vm::current_sequence)
		};

		json::stack::member member
		{
			out, "next_batch", json::value{next_batch}
		};

		const m::user::room user_room
		{
			request.user_id
		};

		m::send(user_room, request.user_id, "ircd.tape.head", request.access_token,
		{
			{ "sequence", next_batch }
		});
	}
}

void
initialsync_presence(client &client,
                     const resource::request &request,
                     json::stack::object &out,
                     const m::user &user)
{
	json::stack::member member{out, "events"};
	json::stack::array array{member};

	const m::user::mitsein mitsein{user};
	mitsein.for_each("join", [&array]
	(const m::user &user)
	{
		m::presence::get(std::nothrow, user, [&array]
		(const json::object &event)
		{
			json::stack::object object{array};

			// sender
			{
				json::stack::member member
				{
					object, "sender", unquote(event.get("user_id"))
				};
			}

			// type
			{
				json::stack::member member
				{
					object, "type", json::value{"m.presence"}
				};
			}

			// content
			{
				json::stack::member member
				{
					object, "content", event
				};
			}
		});
	});
}

void
initialsync_account_data(client &client,
                         const resource::request &request,
                         json::stack::object &out,
                         const m::user::room &user_room)
{
	json::stack::member member{out, "events"};
	json::stack::array array{member};

	const m::room::state state{user_room};
	state.for_each("ircd.account_data", [&array]
	(const m::event &event)
	{
		json::stack::object object{array};

		// type
		{
			json::stack::member member
			{
				object, "type", at<"state_key"_>(event)
			};
		}

		// content
		{
			json::stack::member member
			{
				object, "content", at<"content"_>(event)
			};
		}
	});
}

static void
initialsync_rooms_join(client &client,
                       const resource::request &request,
                       json::stack::object &out,
                       const m::user::room &user_room);

static void
initialsync_rooms_leave(client &client,
                        const resource::request &request,
                        json::stack::object &out,
                        const m::user::room &user_room);

static void
initialsync_rooms_invite(client &client,
                         const resource::request &request,
                         json::stack::object &out,
                         const m::user::room &user_room);

void
initialsync_rooms(client &client,
                  const resource::request &request,
                  json::stack::object &out,
                  const m::user::room &user_room)
{
	// join
	{
		json::stack::member member{out, "join"};
		json::stack::object object{member};
		initialsync_rooms_join(client, request, object, user_room);
	}

	// leave
	{
		json::stack::member member{out, "leave"};
		json::stack::object object{member};
		initialsync_rooms_leave(client, request, object, user_room);
	}

	// invite
	{
		json::stack::member member{out, "invite"};
		json::stack::object object{member};
		initialsync_rooms_invite(client, request, object, user_room);
	}
}

void
initialsync_rooms__membership(client &client,
                              const resource::request &request,
                              json::stack::object &out,
                              const m::user::room &user_room,
                              const string_view &membership);
void
initialsync_rooms_join(client &client,
                       const resource::request &request,
                       json::stack::object &out,
                       const m::user::room &user_room)
{
	initialsync_rooms__membership(client, request, out, user_room, "join");
}

void
initialsync_rooms_leave(client &client,
                        const resource::request &request,
                        json::stack::object &out,
                        const m::user::room &user_room)
{
	initialsync_rooms__membership(client, request, out, user_room, "leave");
}

void
initialsync_rooms_invite(client &client,
                         const resource::request &request,
                         json::stack::object &out,
                         const m::user::room &user_room)
{
	initialsync_rooms__membership(client, request, out, user_room, "invite");
}

static void
initialsync_room(client &client,
                 const resource::request &request,
                 json::stack::object &out,
                 const m::user::room &user_room,
                 const m::room &room,
                 const string_view &membership);

void
initialsync_rooms__membership(client &client,
                              const resource::request &request,
                              json::stack::object &out,
                              const m::user::room &user_room,
                              const string_view &membership)
{
	const m::user::rooms rooms
	{
		user_room.user
	};

	rooms.for_each(membership, [&client, &request, &out, &user_room, &membership]
	(const m::room &room, const string_view &)
	{
		const m::room::id &room_id{room.room_id};
		json::stack::member member{out, room_id};
		json::stack::object object{member};
		initialsync_room(client, request, object, user_room, room_id, membership);
	});
}

static void
initialsync_room_state(client &client,
                       const resource::request &request,
                       json::stack::object &out,
                       const m::user::room &user_room,
                       const m::room &room);

static void
initialsync_room_timeline(client &client,
                          const resource::request &request,
                          json::stack::object &out,
                          const m::user::room &user_room,
                          const m::room &room);

static void
initialsync_room_ephemeral(client &client,
                           const resource::request &request,
                           json::stack::object &out,
                           const m::user::room &user_room,
                           const m::room &room);

static void
initialsync_room_account_data(client &client,
                              const resource::request &request,
                              json::stack::object &out,
                              const m::user::room &user_room,
                              const m::room &room);

static void
initialsync_room_unread_notifications(client &client,
                                      const resource::request &request,
                                      json::stack::object &out,
                                      const m::user::room &user_room,
                                      const m::room &room);

void
initialsync_room(client &client,
                 const resource::request &request,
                 json::stack::object &out,
                 const m::user::room &user_room,
                 const m::room &room,
                 const string_view &membership)
{
	// state
	{
		json::stack::member member
		{
			out, membership != "invite"?
				"state":
				"invite_state"
		};

		json::stack::object object{member};
		initialsync_room_state(client, request, object, user_room, room);
	}

	// timeline
	{
		json::stack::member member{out, "timeline"};
		json::stack::object object{member};
		initialsync_room_timeline(client, request, object, user_room, room);
	}

	// ephemeral
	{
		json::stack::member member{out, "ephemeral"};
		json::stack::object object{member};
		initialsync_room_ephemeral(client, request, object, user_room, room);
	}

	// account_data
	{
		json::stack::member member{out, "account_data"};
		json::stack::object object{member};
		initialsync_room_ephemeral(client, request, object, user_room, room);
	}

	// unread_notifications
	{
		json::stack::member member{out, "unread_notifications"};
		json::stack::object object{member};
		initialsync_room_unread_notifications(client, request, object, user_room, room);
	}
}

void
initialsync_room_state(client &client,
                       const resource::request &request,
                       json::stack::object &out,
                       const m::user::room &user_room,
                       const m::room &room)
{
	json::stack::member member
	{
		out, "events"
	};

	json::stack::array array
	{
		member
	};

	const m::room::state state
	{
		room
	};

	state.for_each([&array](const m::event &event)
	{
		array.append(event);
	});
}

static m::event::id::buf
initialsync_room_timeline_events(client &client,
                                 const resource::request &request,
                                 json::stack::array &out,
                                 const m::user::room &user_room,
                                 const m::room &room);

void
initialsync_room_timeline(client &client,
                          const resource::request &request,
                          json::stack::object &out,
                          const m::user::room &user_room,
                          const m::room &room)
{
	// events
	m::event::id::buf prev;
	{
		json::stack::member member{out, "events"};
		json::stack::array array{member};
		prev = initialsync_room_timeline_events(client, request, array, user_room, room);
	}

	// prev_batch
	{
		json::stack::member member
		{
			out, "prev_batch", string_view{prev}
		};
	}

	// limited
	{
		json::stack::member member
		{
			out, "limited", json::value{false}
		};
	}
}

m::event::id::buf
initialsync_room_timeline_events(client &client,
                                 const resource::request &request,
                                 json::stack::array &out,
                                 const m::user::room &user_room,
                                 const m::room &room)
{
	// messages seeks to the newest event, but the client wants the oldest
	// event first so we seek down first and then iterate back up. Due to
	// an issue with rocksdb's prefix-iteration this iterator becomes
	// toxic as soon as it becomes invalid. As a result we have to copy the
	// event_id on the way down in case of renewing the iterator for the
	// way back. This is not a big deal but rocksdb should fix their shit.
	ssize_t i(0);
	m::event::id::buf event_id;
	m::room::messages it{room};
	for(; it && i < 10; --it, ++i)
		event_id = it.event_id();

	if(i > 0 && !it)
		it.seek(event_id);

	if(i > 0)
		for(; it && i > -1; ++it, --i)
			out.append(*it);

	return event_id;
}

static void
initialsync_room_ephemeral_events(client &client,
                                  const resource::request &request,
                                  json::stack::array &out,
                                  const m::room &room);

void
initialsync_room_ephemeral(client &client,
                           const resource::request &request,
                           json::stack::object &out,
                           const m::user::room &user_room,
                           const m::room &room)
{
	{
		json::stack::member member{out, "events"};
		json::stack::array array{member};
		initialsync_room_ephemeral_events(client, request, array, room);
	}
}

void
initialsync_room_ephemeral_events(client &client,
                                  const resource::request &request,
                                  json::stack::array &events,
                                  const m::room &room)
{
	const m::room::members members{room};
	//TODO: We're skipping receipts from members who left so we enjoy the
	//TODO: joined members optimizations. Need to figure out if anyone
	//TODO: left in the synced timeline and include them manually.
	members.for_each("join", m::room::members::closure{[&events, &room]
	(const m::user &user)
	{
		const m::user::room user_room{user};
		user_room.get(std::nothrow, "ircd.read", room.room_id, [&events, &room, &user]
		(const m::event &event)
		{
			//TODO: skip if receipt is not for event we're actually syncing
			//TODO: in the related messages timeline array.

			json::stack::object object{events};

			// type
			{
				json::stack::member member
				{
					object, "type", "m.receipt"
				};
			}

			// content
			{
				const json::object data
				{
					at<"content"_>(event)
				};

				thread_local char buf[1024];
				const json::members reformat
				{
					{ unquote(data.at("event_id")),
					{
						{ "m.read",
						{
							{ at<"sender"_>(event),
							{
								{ "ts", data.at("ts") }
							}}
						}}
					}}
				};

				json::stack::member member
				{
					object, "content", json::stringify(mutable_buffer{buf}, reformat)
				};
			}
		});
	}});
}

void
initialsync_room_account_data(client &client,
                              const resource::request &request,
                              json::stack::object &out,
                              const m::user::room &user_room,
                              const m::room &room)
{

}

void
initialsync_room_unread_notifications(client &client,
                                      const resource::request &request,
                                      json::stack::object &out,
                                      const m::user::room &user_room,
                                      const m::room &room)
{
	// highlight_count
	{
		json::stack::member member
		{
			out, "highlight_count", json::value{0L}
		};
	}

	// notification_count
	{
		json::stack::member member
		{
			out, "notification_count", json::value{0L}
		};
	}
}
