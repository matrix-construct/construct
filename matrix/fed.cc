// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
//
// fed/groups.h
//

ircd::m::fed::groups::publicised::publicised(const string_view &node,
                                             const vector_view<const id::user> &user_ids,
                                             const mutable_buffer &buf_,
                                             opts opts)
:server::request{[&]
{
	if(!opts.remote)
		opts.remote = node;

	if(!defined(json::get<"origin"_>(opts.request)))
		json::get<"origin"_>(opts.request) = my_host();

	if(!defined(json::get<"destination"_>(opts.request)))
		json::get<"destination"_>(opts.request) = node;

	if(!defined(json::get<"uri"_>(opts.request)))
		json::get<"uri"_>(opts.request) = "/_matrix/federation/v1/get_groups_publicised";

	json::get<"method"_>(opts.request) = "POST";

	mutable_buffer buf{buf_};
	const string_view user_ids_
	{
		json::stringify(buf, user_ids.data(), user_ids.data() + user_ids.size())
	};

	assert(!defined(json::get<"content"_>(opts.request)));
	json::get<"content"_>(opts.request) = stringify(buf, json::members
	{
		{ "user_ids", user_ids_ }
	});

	// (front of buf was advanced by stringify)
	opts.out.content = json::get<"content"_>(opts.request);
	opts.out.head = opts.request(buf);

	if(!size(opts.in))
	{
		consume(buf, size(opts.out.head));
		opts.in.head = buf;
		opts.in.content = opts.dynamic?
			mutable_buffer{}:  // server::request will allocate new mem
			opts.in.head;      // server::request will auto partition
	}

	return server::request
	{
		matrix_service(opts.remote),
		std::move(opts.out),
		std::move(opts.in),
		opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/send.h
//

void
ircd::m::fed::send::response::for_each_pdu(const pdus_closure &closure)
const
{
	const json::object &pdus
	{
		this->get("pdus")
	};

	for(const auto &member : pdus)
	{
		const id::event &event_id{member.first};
		const json::object &error{member.second};
		closure(event_id, error);
	}
}

ircd::m::fed::send::send(const string_view &txnid,
                         const const_buffer &content,
                         const mutable_buffer &buf,
                         opts opts)
:server::request{[&]
{
	assert(!!opts.remote);

	assert(!size(opts.out.content));
	opts.out.content = content;

	assert(!defined(json::get<"content"_>(opts.request)));
	json::get<"content"_>(opts.request) = json::object{opts.out.content};

	if(!defined(json::get<"origin"_>(opts.request)))
		json::get<"origin"_>(opts.request) = my_host();

	if(!defined(json::get<"destination"_>(opts.request)))
		json::get<"destination"_>(opts.request) = host(opts.remote);

	if(!defined(json::get<"uri"_>(opts.request)))
	{
		thread_local char urlbuf[1024], txnidbuf[512];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			urlbuf, "/_matrix/federation/v1/send/%s/",
			url::encode(txnidbuf, txnid),
		};
	}

	json::get<"method"_>(opts.request) = "PUT";
	opts.out.head = opts.request(buf);

	if(!size(opts.in))
	{
		opts.in.head = buf + size(opts.out.head);
		opts.in.content = opts.dynamic?
			mutable_buffer{}:  // server::request will allocate new mem
			opts.in.head;      // server::request will auto partition
	}

	return server::request
	{
		matrix_service(opts.remote),
		std::move(opts.out),
		std::move(opts.in),
		opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/public_rooms.h
//

ircd::m::fed::public_rooms::public_rooms(const net::hostport &remote,
                                         const mutable_buffer &buf,
                                         opts opts)
:server::request{[&]
{
	if(!opts.remote)
		opts.remote = remote;

	if(!defined(json::get<"origin"_>(opts.request)))
		json::get<"origin"_>(opts.request) = my_host();

	if(!defined(json::get<"destination"_>(opts.request)))
		json::get<"destination"_>(opts.request) = host(opts.remote);

	if(defined(json::get<"content"_>(opts.request)))
		opts.out.content = json::get<"content"_>(opts.request);

	if(!defined(json::get<"content"_>(opts.request)))
		json::get<"content"_>(opts.request) = json::object{opts.out.content};

	if(!defined(json::get<"uri"_>(opts.request)))
	{
		thread_local char urlbuf[3072], query[2048], since[1024], tpid[1024];
		std::stringstream qss;
		pubsetbuf(qss, query);

		if(opts.since)
			qss << "&since="
			    << url::encode(since, opts.since);

		if(opts.third_party_instance_id)
			qss << "&third_party_instance_id="
			    << url::encode(tpid, opts.third_party_instance_id);

		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			urlbuf, "/_matrix/federation/v1/publicRooms?limit=%zu%s%s",
			opts.limit,
			opts.include_all_networks? "&include_all_networks=true" : "",
			view(qss, query)
		};
	}

	json::get<"method"_>(opts.request) = "GET";
	opts.out.head = opts.request(buf);

	if(!size(opts.in))
	{
		opts.in.head = buf + size(opts.out.head);
		opts.in.content = opts.dynamic?
			mutable_buffer{}:  // server::request will allocate new mem
			opts.in.head;      // server::request will auto partition
	}

	return server::request
	{
		matrix_service(opts.remote),
		std::move(opts.out),
		std::move(opts.in),
		opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/frontfill.h
//

ircd::m::fed::frontfill::frontfill(const room::id &room_id,
                                   const span &span,
                                   const mutable_buffer &buf,
                                   opts opts)
:frontfill
{
	room_id,
	ranges { vector(&span.first, 1), vector(&span.second, 1) },
	buf,
	std::move(opts)
}
{
}

ircd::m::fed::frontfill::frontfill(const room::id &room_id,
                                   const ranges &pair,
                                   const mutable_buffer &buf_,
                                   opts opts)
:server::request{[&]
{
	assert(!!opts.remote);

	if(!defined(json::get<"origin"_>(opts.request)))
		json::get<"origin"_>(opts.request) = my_host();

	if(!defined(json::get<"destination"_>(opts.request)))
		json::get<"destination"_>(opts.request) = host(opts.remote);

	if(defined(json::get<"content"_>(opts.request)))
		opts.out.content = json::get<"content"_>(opts.request);

	if(!defined(json::get<"uri"_>(opts.request)))
	{
		thread_local char urlbuf[1024], ridbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			urlbuf, "/_matrix/federation/v1/get_missing_events/%s/",
			url::encode(ridbuf, room_id)
		};
	}

	window_buffer buf{buf_};
	if(!defined(json::get<"content"_>(opts.request)))
	{
		buf([&pair, &opts](const mutable_buffer &buf)
		{
			return make_content(buf, pair, opts);
		});

		json::get<"content"_>(opts.request) = json::object{buf.completed()};
		opts.out.content = json::get<"content"_>(opts.request);
	}

	json::get<"method"_>(opts.request) = "POST";
	opts.out.head = opts.request(buf);

	if(!size(opts.in))
	{
		opts.in.head = buf + size(opts.out.head);
		opts.in.content = opts.dynamic?
			mutable_buffer{}:  // server::request will allocate new mem
			opts.in.head;      // server::request will auto partition
	}

	return server::request
	{
		matrix_service(opts.remote),
		std::move(opts.out),
		std::move(opts.in),
		opts.sopts
	};
}()}
{
}

ircd::const_buffer
ircd::m::fed::frontfill::make_content(const mutable_buffer &buf,
	                                  const ranges &pair,
	                                  const opts &opts)
{
	json::stack out{buf};
	{
		// note: This object must be in abc order
		json::stack::object top{out};
		{
			json::stack::member earliest{top, "earliest_events"};
			json::stack::array array{earliest};
			for(const auto &id : pair.first)
				array.append(id);
		}
		{
			json::stack::member latest{top, "latest_events"};
			json::stack::array array{latest};
			for(const auto &id : pair.second)
				array.append(id);
		}
		json::stack::member{top, "limit", json::value(int64_t(opts.limit))};
		json::stack::member{top, "min_depth", json::value(int64_t(opts.min_depth))};
	}

	return out.completed();
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/backfill.h
//

ircd::m::fed::backfill::backfill(const room::id &room_id,
                                 const mutable_buffer &buf,
                                 opts opts)
:server::request{[&]
{
	if(!opts.remote)
		opts.remote = room_id.host();

	m::event::id::buf event_id_buf;
	if(!opts.event_id)
	{
		event_id_buf = fetch_head(room_id, opts.remote);
		opts.event_id = event_id_buf;
	}

	if(!defined(json::get<"origin"_>(opts.request)))
		json::get<"origin"_>(opts.request) = my_host();

	if(!defined(json::get<"destination"_>(opts.request)))
		json::get<"destination"_>(opts.request) = host(opts.remote);

	if(defined(json::get<"content"_>(opts.request)))
		opts.out.content = json::get<"content"_>(opts.request);

	if(!defined(json::get<"content"_>(opts.request)))
		json::get<"content"_>(opts.request) = json::object{opts.out.content};

	if(!defined(json::get<"uri"_>(opts.request)))
	{
		thread_local char urlbuf[2048], ridbuf[768], eidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			urlbuf, "/_matrix/federation/v1/backfill/%s/?limit=%zu&v=%s",
			url::encode(ridbuf, room_id),
			opts.limit,
			url::encode(eidbuf, opts.event_id),
		};
	}

	json::get<"method"_>(opts.request) = "GET";
	opts.out.head = opts.request(buf);

	if(!size(opts.in))
	{
		opts.in.head = buf + size(opts.out.head);
		opts.in.content = opts.dynamic?
			mutable_buffer{}:  // server::request will allocate new mem
			opts.in.head;      // server::request will auto partition
	}

	return server::request
	{
		matrix_service(opts.remote),
		std::move(opts.out),
		std::move(opts.in),
		opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/state.h
//

ircd::m::fed::state::state(const room::id &room_id,
                           const mutable_buffer &buf,
                           opts opts)
:server::request{[&]
{
	if(!opts.remote)
		opts.remote = room_id.host();

	m::event::id::buf event_id_buf;
	if(!opts.event_id)
	{
		event_id_buf = fetch_head(room_id, opts.remote);
		opts.event_id = event_id_buf;
	}

	if(!defined(json::get<"origin"_>(opts.request)))
		json::get<"origin"_>(opts.request) = my_host();

	if(!defined(json::get<"destination"_>(opts.request)))
		json::get<"destination"_>(opts.request) = host(opts.remote);

	if(defined(json::get<"content"_>(opts.request)))
		opts.out.content = json::get<"content"_>(opts.request);

	if(!defined(json::get<"content"_>(opts.request)))
		json::get<"content"_>(opts.request) = json::object{opts.out.content};

	if(!defined(json::get<"uri"_>(opts.request)))
	{
		thread_local char urlbuf[2048], ridbuf[768], eidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			urlbuf, "/_matrix/federation/v1/%s/%s/?event_id=%s%s",
			opts.ids_only? "state_ids" : "state",
			url::encode(ridbuf, room_id),
			url::encode(eidbuf, opts.event_id),
			opts.ids_only? "&auth_chain_ids=0"_sv : ""_sv,
		};
	}

	json::get<"method"_>(opts.request) = "GET";
	opts.out.head = opts.request(buf);

	if(!size(opts.in))
	{
		opts.in.head = buf + size(opts.out.head);
		opts.in.content = opts.dynamic?
			mutable_buffer{}:  // server::request will allocate new mem
			opts.in.head;      // server::request will auto partition
	}

	return server::request
	{
		matrix_service(opts.remote),
		std::move(opts.out),
		std::move(opts.in),
		opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/query_auth.h
//

ircd::m::fed::query_auth::query_auth(const m::room::id &room_id,
                                     const m::event::id &event_id,
                                     const json::object &content,
                                     const mutable_buffer &buf,
                                     opts opts)
:server::request{[&]
{
	if(!opts.remote)
		opts.remote = event_id.host();

	if(!defined(json::get<"origin"_>(opts.request)))
		json::get<"origin"_>(opts.request) = my_host();

	if(!defined(json::get<"destination"_>(opts.request)))
		json::get<"destination"_>(opts.request) = host(opts.remote);

	if(!defined(json::get<"content"_>(opts.request)))
		json::get<"content"_>(opts.request) = content;

	if(defined(json::get<"content"_>(opts.request)))
		opts.out.content = json::get<"content"_>(opts.request);

	if(!defined(json::get<"content"_>(opts.request)))
		json::get<"content"_>(opts.request) = json::object{opts.out.content};

	if(!defined(json::get<"uri"_>(opts.request)))
	{
		thread_local char urlbuf[2048], ridbuf[768], eidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			urlbuf, "/_matrix/federation/v1/query_auth/%s/%s",
			url::encode(ridbuf, room_id),
			url::encode(eidbuf, event_id),
		};
	}

	json::get<"method"_>(opts.request) = "POST";
	opts.out.head = opts.request(buf);

	if(!size(opts.in))
	{
		opts.in.head = buf + size(opts.out.head);
		opts.in.content = opts.dynamic?
			mutable_buffer{}:  // server::request will allocate new mem
			opts.in.head;      // server::request will auto partition
	}

	return server::request
	{
		matrix_service(opts.remote),
		std::move(opts.out),
		std::move(opts.in),
		opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/event_auth.h
//

ircd::m::fed::event_auth::event_auth(const m::room::id &room_id,
                                     const m::event::id &event_id,
                                     const mutable_buffer &buf,
                                     opts opts)
:server::request{[&]
{
	if(!opts.remote)
		opts.remote = event_id.host();

	if(!defined(json::get<"origin"_>(opts.request)))
		json::get<"origin"_>(opts.request) = my_host();

	if(!defined(json::get<"destination"_>(opts.request)))
		json::get<"destination"_>(opts.request) = host(opts.remote);

	if(defined(json::get<"content"_>(opts.request)))
		opts.out.content = json::get<"content"_>(opts.request);

	if(!defined(json::get<"content"_>(opts.request)))
		json::get<"content"_>(opts.request) = json::object{opts.out.content};

	if(!defined(json::get<"uri"_>(opts.request)))
	{
		thread_local char urlbuf[2048], ridbuf[768], eidbuf[768];

		if(opts.ids_only)
			json::get<"uri"_>(opts.request) = fmt::sprintf
			{
				urlbuf, "/_matrix/federation/v1/state_ids/%s/?event_id=%s&pdu_ids=0",
				url::encode(ridbuf, room_id),
				url::encode(eidbuf, event_id),
			};
		else
			json::get<"uri"_>(opts.request) = fmt::sprintf
			{
				urlbuf, "/_matrix/federation/v1/event_auth/%s/%s",
				url::encode(ridbuf, room_id),
				url::encode(eidbuf, event_id),
			};
	}

	json::get<"method"_>(opts.request) = "GET";
	opts.out.head = opts.request(buf);

	if(!size(opts.in))
	{
		opts.in.head = buf + size(opts.out.head);
		opts.in.content = opts.dynamic?
			mutable_buffer{}:  // server::request will allocate new mem
			opts.in.head;      // server::request will auto partition
	}

	return server::request
	{
		matrix_service(opts.remote),
		std::move(opts.out),
		std::move(opts.in),
		opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/event.h
//

ircd::m::fed::event::event(const m::event::id &event_id,
                           const mutable_buffer &buf,
                           opts opts)
:server::request{[&]
{
	if(!opts.remote)
		opts.remote = event_id.host();

	if(!defined(json::get<"origin"_>(opts.request)))
		json::get<"origin"_>(opts.request) = my_host();

	if(!defined(json::get<"destination"_>(opts.request)))
		json::get<"destination"_>(opts.request) = host(opts.remote);

	if(defined(json::get<"content"_>(opts.request)))
		opts.out.content = json::get<"content"_>(opts.request);

	if(!defined(json::get<"content"_>(opts.request)))
		json::get<"content"_>(opts.request) = json::object{opts.out.content};

	if(!defined(json::get<"uri"_>(opts.request)))
	{
		thread_local char urlbuf[1024], eidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			urlbuf, "/_matrix/federation/v1/event/%s/",
			url::encode(eidbuf, event_id),
		};
	}

	json::get<"method"_>(opts.request) = "GET";
	opts.out.head = opts.request(buf);

	if(!size(opts.in))
	{
		opts.in.head = buf + size(opts.out.head);
		opts.in.content = opts.dynamic?
			mutable_buffer{}:  // server::request will allocate new mem
			opts.in.head;      // server::request will auto partition
	}

	return server::request
	{
		matrix_service(opts.remote),
		std::move(opts.out),
		std::move(opts.in),
		opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/invite.h
//

ircd::m::fed::invite::invite(const room::id &room_id,
                             const id::event &event_id,
                             const json::object &content,
                             const mutable_buffer &buf,
                             opts opts)
:server::request{[&]
{
	assert(!!opts.remote);

	assert(!size(opts.out.content));
	opts.out.content = content;

	assert(!defined(json::get<"content"_>(opts.request)));
	json::get<"content"_>(opts.request) = json::object{opts.out.content};

	if(!defined(json::get<"origin"_>(opts.request)))
		json::get<"origin"_>(opts.request) = my_host();

	if(!defined(json::get<"destination"_>(opts.request)))
		json::get<"destination"_>(opts.request) = host(opts.remote);

	if(!defined(json::get<"uri"_>(opts.request)))
	{
		thread_local char urlbuf[2048], ridbuf[768], eidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			urlbuf, "/_matrix/federation/v1/invite/%s/%s",
			url::encode(ridbuf, room_id),
			url::encode(eidbuf, event_id)
		};
	}

	json::get<"method"_>(opts.request) = "PUT";
	opts.out.head = opts.request(buf);

	if(!size(opts.in))
	{
		opts.in.head = buf + size(opts.out.head);
		opts.in.content = opts.dynamic?
			mutable_buffer{}:  // server::request will allocate new mem
			opts.in.head;      // server::request will auto partition
	}

	return server::request
	{
		matrix_service(opts.remote),
		std::move(opts.out),
		std::move(opts.in),
		opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/invite2.h
//

ircd::m::fed::invite2::invite2(const room::id &room_id,
                               const id::event &event_id,
                               const json::object &content,
                               const mutable_buffer &buf,
                               opts opts)
:server::request{[&]
{
	assert(!!opts.remote);

	assert(!size(opts.out.content));
	opts.out.content = content;

	assert(!defined(json::get<"content"_>(opts.request)));
	json::get<"content"_>(opts.request) = json::object{opts.out.content};

	if(!defined(json::get<"origin"_>(opts.request)))
		json::get<"origin"_>(opts.request) = my_host();

	if(!defined(json::get<"destination"_>(opts.request)))
		json::get<"destination"_>(opts.request) = host(opts.remote);

	if(!defined(json::get<"uri"_>(opts.request)))
	{
		thread_local char urlbuf[2048], ridbuf[768], eidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			urlbuf, "/_matrix/federation/v2/invite/%s/%s",
			url::encode(ridbuf, room_id),
			url::encode(eidbuf, event_id)
		};
	}

	json::get<"method"_>(opts.request) = "PUT";
	opts.out.head = opts.request(buf);

	if(!size(opts.in))
	{
		opts.in.head = buf + size(opts.out.head);
		opts.in.content = opts.dynamic?
			mutable_buffer{}:  // server::request will allocate new mem
			opts.in.head;      // server::request will auto partition
	}

	return server::request
	{
		matrix_service(opts.remote),
		std::move(opts.out),
		std::move(opts.in),
		opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/send_join.h
//

ircd::m::fed::send_join::send_join(const room::id &room_id,
                                   const id::event &event_id,
                                   const const_buffer &content,
                                   const mutable_buffer &buf,
                                   opts opts)
:server::request{[&]
{
	assert(!!opts.remote);

	assert(!size(opts.out.content));
	opts.out.content = content;

	assert(!defined(json::get<"content"_>(opts.request)));
	json::get<"content"_>(opts.request) = json::object{opts.out.content};

	if(!defined(json::get<"origin"_>(opts.request)))
		json::get<"origin"_>(opts.request) = my_host();

	if(!defined(json::get<"destination"_>(opts.request)))
		json::get<"destination"_>(opts.request) = host(opts.remote);

	if(!defined(json::get<"uri"_>(opts.request)))
	{
		thread_local char urlbuf[2048], ridbuf[768], uidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			urlbuf, "/_matrix/federation/v1/send_join/%s/%s",
			url::encode(ridbuf, room_id),
			url::encode(uidbuf, event_id)
		};
	}

	json::get<"method"_>(opts.request) = "PUT";
	opts.out.head = opts.request(buf);

	if(!size(opts.in))
	{
		opts.in.head = buf + size(opts.out.head);
		opts.in.content = opts.dynamic?
			mutable_buffer{}:  // server::request will allocate new mem
			opts.in.head;      // server::request will auto partition
	}

	return server::request
	{
		matrix_service(opts.remote),
		std::move(opts.out),
		std::move(opts.in),
		opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/make_join.h
//

ircd::m::fed::make_join::make_join(const room::id &room_id,
                                   const id::user &user_id_,
                                   const mutable_buffer &buf,
                                   opts opts)
:server::request{[&]
{
	if(!opts.remote)
		opts.remote = room_id.host();

	if(!defined(json::get<"origin"_>(opts.request)))
		json::get<"origin"_>(opts.request) = my_host();

	if(!defined(json::get<"destination"_>(opts.request)))
		json::get<"destination"_>(opts.request) = host(opts.remote);

	if(defined(json::get<"content"_>(opts.request)))
		opts.out.content = json::get<"content"_>(opts.request);

	if(!defined(json::get<"content"_>(opts.request)))
		json::get<"content"_>(opts.request) = json::object{opts.out.content};

	id::user::buf user_id_buf;
	const id::user &user_id
	{
		user_id_?: id::user
		{
			user_id_buf, id::generate, json::get<"origin"_>(opts.request)
		}
	};

	if(!defined(json::get<"uri"_>(opts.request)))
	{
		thread_local char urlbuf[2048], ridbuf[768], uidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			urlbuf, "/_matrix/federation/v1/make_join/%s/%s"
			"?ver=1&ver=2&ver=3&ver=4&ver=5&ver=6&ver=7&ver=8",
			url::encode(ridbuf, room_id),
			url::encode(uidbuf, user_id)
		};
	}

	json::get<"method"_>(opts.request) = "GET";
	opts.out.head = opts.request(buf);

	if(!size(opts.in))
	{
		opts.in.head = buf + size(opts.out.head);
		opts.in.content = opts.dynamic?
			mutable_buffer{}:  // server::request will allocate new mem
			opts.in.head;      // server::request will auto partition
	}

	return server::request
	{
		matrix_service(opts.remote),
		std::move(opts.out),
		std::move(opts.in),
		opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/user_keys.h
//

//
// query
//

ircd::m::fed::user::keys::query::query(const m::user::id &user_id,
                                       const mutable_buffer &buf,
                                       opts opts)
:query
{
	user_id,
	string_view{},
	buf,
	std::move(opts)
}
{
}

ircd::m::fed::user::keys::query::query(const m::user::id &user_id,
                                       const string_view &device_id,
                                       const mutable_buffer &buf,
                                       opts opts)
:query
{
	user_devices
	{
		user_id, vector_view<const string_view>
		{
			&device_id, !empty(device_id)
		}
	},
	buf,
	std::move(opts)
}
{
}

ircd::m::fed::user::keys::query::query(const user_devices &v,
                                       const mutable_buffer &buf,
                                       opts opts)
:query
{
	vector_view<const user_devices>
	{
		&v, 1
	},
	buf,
	std::move(opts)
}
{
}

ircd::m::fed::user::keys::query::query(const users_devices &v,
                                       const mutable_buffer &buf,
                                       opts opts)
:query{[&v, &buf, &opts]
{
	const json::object &content
	{
		make_content(buf, v)
	};

	return query
	{
		content, buf + size(string_view(content)), std::move(opts)
	};
}()}
{
}

ircd::m::fed::user::keys::query::query(const users_devices_map &m,
                                       const mutable_buffer &buf,
                                       opts opts)
:query{[&m, &buf, &opts]
{
	const json::object &content
	{
		make_content(buf, m)
	};

	return query
	{
		content, buf + size(string_view(content)), std::move(opts)
	};
}()}
{
}

ircd::m::fed::user::keys::query::query(const json::object &content,
                                       const mutable_buffer &buf,
                                       opts opts)
:server::request{[&]
{
	assert(!!opts.remote);

	if(!defined(json::get<"origin"_>(opts.request)))
		json::get<"origin"_>(opts.request) = my_host();

	if(!defined(json::get<"destination"_>(opts.request)))
		json::get<"destination"_>(opts.request) = host(opts.remote);

	if(!defined(json::get<"uri"_>(opts.request)))
		json::get<"uri"_>(opts.request) = "/_matrix/federation/v1/user/keys/query";

	if(!defined(json::get<"content"_>(opts.request)))
		json::get<"content"_>(opts.request) = content;

	if(!defined(json::get<"method"_>(opts.request)))
		json::get<"method"_>(opts.request) = "POST";

	opts.out.content = json::get<"content"_>(opts.request);
	opts.out.head = opts.request(buf);

	if(!size(opts.in))
	{
		opts.in.head = buf + size(opts.out.head);
		opts.in.content = opts.dynamic?
			mutable_buffer{}:  // server::request will allocate new mem
			opts.in.head;      // server::request will auto partition
	}

	return server::request
	{
		matrix_service(opts.remote),
		std::move(opts.out),
		std::move(opts.in),
		opts.sopts
	};
}()}
{
}

ircd::json::object
ircd::m::fed::user::keys::query::make_content(const mutable_buffer &buf,
                                              const users_devices &v)
{
	json::stack out{buf};
	{
		json::stack::object top{out};
		json::stack::object device_keys
		{
			top, "device_keys"
		};

		for(const auto &[user_id, devices] : v)
		{
			json::stack::array array
			{
				device_keys, user_id
			};

			for(const auto &device_id : devices)
				array.append(device_id);
		}
	}

	return out.completed();
}

ircd::json::object
ircd::m::fed::user::keys::query::make_content(const mutable_buffer &buf,
                                              const users_devices_map &m)
{
	json::stack out{buf};
	{
		json::stack::object top{out};
		json::stack::object device_keys
		{
			top, "device_keys"
		};

		for(const auto &[user_id, devices] : m)
			json::stack::member
			{
				device_keys, user_id, devices
			};
	}

	return out.completed();
}

//
// claim
//

ircd::m::fed::user::keys::claim::claim(const m::user::id &user_id,
                                       const string_view &device_id,
                                       const string_view &algorithm,
                                       const mutable_buffer &buf,
                                       opts opts)
:claim
{
	user_id,
	device
	{
		device_id, algorithm
	},
	buf,
	std::move(opts)
}
{
}

ircd::m::fed::user::keys::claim::claim(const m::user::id &user_id,
                                       const device &device,
                                       const mutable_buffer &buf,
                                       opts opts)
:claim
{
	user_devices
	{
		user_id, { &device, 1 }
	},
	buf,
	std::move(opts)
}
{
}

ircd::m::fed::user::keys::claim::claim(const user_devices &ud,
                                       const mutable_buffer &buf,
                                       opts opts)
:claim
{
	vector_view<const user_devices>
	{
		&ud, 1
	},
	buf,
	std::move(opts)
}
{
}

ircd::m::fed::user::keys::claim::claim(const users_devices &v,
                                       const mutable_buffer &buf,
                                       opts opts)
:claim{[&v, &buf, &opts]
{
	const json::object &content
	{
		make_content(buf, v)
	};

	return claim
	{
		content, buf + size(string_view(content)), std::move(opts)
	};
}()}
{
}

ircd::m::fed::user::keys::claim::claim(const users_devices_map &m,
                                       const mutable_buffer &buf,
                                       opts opts)
:claim{[&m, &buf, &opts]
{
	const json::object &content
	{
		make_content(buf, m)
	};

	return claim
	{
		content, buf + size(string_view(content)), std::move(opts)
	};
}()}
{
}

ircd::m::fed::user::keys::claim::claim(const json::object &content,
                                       const mutable_buffer &buf,
                                       opts opts)
:server::request{[&]
{
	assert(!!opts.remote);

	if(!defined(json::get<"origin"_>(opts.request)))
		json::get<"origin"_>(opts.request) = my_host();

	if(!defined(json::get<"destination"_>(opts.request)))
		json::get<"destination"_>(opts.request) = host(opts.remote);

	if(!defined(json::get<"uri"_>(opts.request)))
		json::get<"uri"_>(opts.request) = "/_matrix/federation/v1/user/keys/claim";

	if(!defined(json::get<"content"_>(opts.request)))
		json::get<"content"_>(opts.request) = content;

	if(!defined(json::get<"method"_>(opts.request)))
		json::get<"method"_>(opts.request) = "POST";

	opts.out.content = json::get<"content"_>(opts.request);
	opts.out.head = opts.request(buf);

	if(!size(opts.in))
	{
		opts.in.head = buf + size(opts.out.head);
		opts.in.content = opts.dynamic?
			mutable_buffer{}:  // server::request will allocate new mem
			opts.in.head;      // server::request will auto partition
	}

	return server::request
	{
		matrix_service(opts.remote),
		std::move(opts.out),
		std::move(opts.in),
		opts.sopts
	};
}()}
{
}

ircd::json::object
ircd::m::fed::user::keys::claim::make_content(const mutable_buffer &buf,
                                              const users_devices &v)
{
	json::stack out{buf};
	{
		json::stack::object top{out};
		json::stack::object one_time_keys
		{
			top, "one_time_keys"
		};

		for(const auto &[user_id, devices] : v)
		{
			json::stack::object user
			{
				one_time_keys, user_id
			};

			for(const auto &[device_id, algorithm_name] : devices)
				json::stack::member
				{
					user, device_id, algorithm_name
				};
		}
	}

	return out.completed();
}

ircd::json::object
ircd::m::fed::user::keys::claim::make_content(const mutable_buffer &buf,
                                              const users_devices_map &v)
{
	json::stack out{buf};
	{
		json::stack::object top{out};
		json::stack::object one_time_keys
		{
			top, "one_time_keys"
		};

		for(const auto &[user_id, devices] : v)
		{
			json::stack::object user
			{
				one_time_keys, user_id
			};

			for(const auto &[device_id, algorithm_name] : devices)
				json::stack::member
				{
					user, device_id, algorithm_name
				};
		}
	}

	return out.completed();
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/user.h
//

ircd::m::fed::user::devices::devices(const id::user &user_id,
                                     const mutable_buffer &buf,
                                     opts opts)
:server::request{[&]
{
	if(!opts.remote)
		opts.remote = user_id.host();

	if(!defined(json::get<"origin"_>(opts.request)))
		json::get<"origin"_>(opts.request) = my_host();

	if(!defined(json::get<"destination"_>(opts.request)))
		json::get<"destination"_>(opts.request) = host(opts.remote);

	if(!defined(json::get<"uri"_>(opts.request)))
	{
		thread_local char urlbuf[2048], uidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			urlbuf, "/_matrix/federation/v1/user/devices/%s",
			url::encode(uidbuf, user_id)
		};
	}

	if(defined(json::get<"content"_>(opts.request)))
		opts.out.content = json::get<"content"_>(opts.request);

	if(!defined(json::get<"method"_>(opts.request)))
		json::get<"method"_>(opts.request) = "GET";

	opts.out.head = opts.request(buf);

	if(!size(opts.in))
	{
		opts.in.head = buf + size(opts.out.head);
		opts.in.content = opts.dynamic?
			mutable_buffer{}:  // server::request will allocate new mem
			opts.in.head;      // server::request will auto partition
	}

	return server::request
	{
		matrix_service(opts.remote),
		std::move(opts.out),
		std::move(opts.in),
		opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/query.h
//

namespace ircd::m::fed
{
	thread_local char query_arg_buf[1024];
	thread_local char query_url_buf[1024];
}

ircd::m::fed::query::directory::directory(const id::room_alias &room_alias,
                                          const mutable_buffer &buf,
                                          opts opts)
:query
{
	"directory",
	fmt::sprintf
	{
		query_arg_buf, "room_alias=%s", url::encode(query_url_buf, room_alias)
	},
	buf,
	std::move(opts)
}
{
}

ircd::m::fed::query::profile::profile(const id::user &user_id,
                                      const mutable_buffer &buf,
                                      opts opts)
:query
{
	"profile",
	fmt::sprintf
	{
		query_arg_buf, "user_id=%s", url::encode(query_url_buf, user_id)
	},
	buf,
	std::move(opts)
}
{
}

ircd::m::fed::query::profile::profile(const id::user &user_id,
                                      const string_view &field,
                                      const mutable_buffer &buf,
                                      opts opts)
:query
{
	"profile",
	fmt::sprintf
	{
		query_arg_buf, "user_id=%s%s%s",
		url::encode(query_url_buf, string_view{user_id}),
		!empty(field)? "&field=" : "",
		field
	},
	buf,
	std::move(opts)
}
{
}

ircd::m::fed::query::query(const string_view &type,
                           const string_view &args,
                           const mutable_buffer &buf,
                           opts opts)
:server::request{[&]
{
	assert(!!opts.remote);

	if(!defined(json::get<"origin"_>(opts.request)))
		json::get<"origin"_>(opts.request) = my_host();

	if(!defined(json::get<"destination"_>(opts.request)))
		json::get<"destination"_>(opts.request) = host(opts.remote);

	if(!defined(json::get<"uri"_>(opts.request)))
	{
		thread_local char urlbuf[2048];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			urlbuf, "/_matrix/federation/v1/query/%s%s%s",
			type,
			args? "?"_sv : ""_sv,
			args
		};
	}

	if(defined(json::get<"content"_>(opts.request)))
		opts.out.content = json::get<"content"_>(opts.request);

	if(!defined(json::get<"method"_>(opts.request)))
		json::get<"method"_>(opts.request) = "GET";

	opts.out.head = opts.request(buf);

	if(!size(opts.in))
	{
		opts.in.head = buf + size(opts.out.head);
		opts.in.content = opts.dynamic?
			mutable_buffer{}:  // server::request will allocate new mem
			opts.in.head;      // server::request will auto partition
	}

	return server::request
	{
		matrix_service(opts.remote),
		std::move(opts.out),
		std::move(opts.in),
		opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/key.h
//

ircd::m::fed::key::keys::keys(const string_view &server_name,
                              const mutable_buffer &buf,
                              opts opts)
:keys
{
	server_key{server_name, ""}, buf, std::move(opts)
}
{
}

ircd::m::fed::key::keys::keys(const server_key &server_key,
                              const mutable_buffer &buf,
                              opts opts)
:server::request{[&]
{
	const auto &server_name{server_key.first};
	const auto &key_id{server_key.second};

	if(!opts.remote)
		opts.remote = net::hostport{server_name};

	if(!defined(json::get<"origin"_>(opts.request)))
		json::get<"origin"_>(opts.request) = my_host();

	if(!defined(json::get<"destination"_>(opts.request)))
		json::get<"destination"_>(opts.request) = host(opts.remote);

	if(defined(json::get<"content"_>(opts.request)))
		opts.out.content = json::get<"content"_>(opts.request);

	if(!defined(json::get<"content"_>(opts.request)))
		json::get<"content"_>(opts.request) = json::object{opts.out.content};

	if(!defined(json::get<"uri"_>(opts.request)))
	{
		if(!empty(key_id))
		{
			thread_local char uribuf[512];
			json::get<"uri"_>(opts.request) = fmt::sprintf
			{
				uribuf, "/_matrix/key/v2/server/%s/", key_id
			};
		}
		else json::get<"uri"_>(opts.request) = "/_matrix/key/v2/server/";
	}

	json::get<"method"_>(opts.request) = "GET";

	opts.out.head = opts.request(buf);

	if(!size(opts.in))
	{
		opts.in.head = buf + size(opts.out.head);
		opts.in.content = opts.dynamic?
			mutable_buffer{}:  // server::request will allocate new mem
			opts.in.head;      // server::request will auto partition
	}

	return server::request
	{
		matrix_service(opts.remote),
		std::move(opts.out),
		std::move(opts.in),
		opts.sopts
	};
}()}
{
}

namespace ircd::m::fed
{
	static const_buffer
	_make_server_keys(const vector_view<const key::server_key> &,
	                  const mutable_buffer &);
}

ircd::m::fed::key::query::query(const vector_view<const server_key> &keys,
                                const mutable_buffer &buf_,
                                opts opts)
:server::request{[&]
{
	assert(!!opts.remote);

	if(!defined(json::get<"origin"_>(opts.request)))
		json::get<"origin"_>(opts.request) = my_host();

	if(!defined(json::get<"destination"_>(opts.request)))
		json::get<"destination"_>(opts.request) = host(opts.remote);

	if(defined(json::get<"content"_>(opts.request)))
		opts.out.content = json::get<"content"_>(opts.request);

	if(!defined(json::get<"content"_>(opts.request)))
		json::get<"content"_>(opts.request) = json::object{opts.out.content};

	if(!defined(json::get<"uri"_>(opts.request)))
		json::get<"uri"_>(opts.request) = "/_matrix/key/v2/query";

	json::get<"method"_>(opts.request) = "POST";

	window_buffer buf{buf_};
	if(!defined(json::get<"content"_>(opts.request)))
	{
		buf([&keys](const mutable_buffer &buf)
		{
			return _make_server_keys(keys, buf);
		});

		json::get<"content"_>(opts.request) = json::object{buf.completed()};
		opts.out.content = json::get<"content"_>(opts.request);
	}

	opts.out.head = opts.request(buf);

	if(!size(opts.in))
	{
		opts.in.head = buf + size(opts.out.head);
		opts.in.content = opts.dynamic?
			mutable_buffer{}:  // server::request will allocate new mem
			opts.in.head;      // server::request will auto partition
	}

	return server::request
	{
		matrix_service(opts.remote),
		std::move(opts.out),
		std::move(opts.in),
		opts.sopts
	};
}()}
{
}

static ircd::const_buffer
ircd::m::fed::_make_server_keys(const vector_view<const key::server_key> &keys,
                                const mutable_buffer &buf)
{
	json::stack out{buf};
	json::stack::object top{out};
	json::stack::object server_keys
	{
		top, "server_keys"
	};

	for(const auto &[server_name, key_id] : keys)
	{
		json::stack::object server_object
		{
			server_keys, server_name
		};

		if(key_id)
		{
			json::stack::object key_object
			{
				server_object, key_id
			};

			//json::stack::member mvut
			//{
			//	key_object, "minimum_valid_until_ts", json::value(0L)
			//};
		}
	}

	server_keys.~object();
	top.~object();
	return out.completed();
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/version.h
//

ircd::m::fed::version::version(const mutable_buffer &buf,
                               opts opts)
:server::request{[&]
{
	assert(!!opts.remote);

	if(!defined(json::get<"origin"_>(opts.request)))
		json::get<"origin"_>(opts.request) = my_host();

	if(!defined(json::get<"destination"_>(opts.request)))
		json::get<"destination"_>(opts.request) = host(opts.remote);

	if(!defined(json::get<"uri"_>(opts.request)))
		json::get<"uri"_>(opts.request) = "/_matrix/federation/v1/version";

	json::get<"method"_>(opts.request) = "GET";
	opts.out.head = opts.request(buf);

	if(!size(opts.in))
	{
		opts.in.head = buf + size(opts.out.head);
		opts.in.content = opts.dynamic?
			mutable_buffer{}:  // server::request will allocate new mem
			opts.in.head;      // server::request will auto partition
	}

	return server::request
	{
		matrix_service(opts.remote),
		std::move(opts.out),
		std::move(opts.in),
		opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/v1.h
//

ircd::conf::item<ircd::milliseconds>
fetch_head_timeout
{
	{ "name",     "ircd.m.v1.fetch_head.timeout" },
	{ "default",  30 * 1000L                     },
};

ircd::m::event::id::buf
ircd::m::fed::fetch_head(const id::room &room_id,
                         const net::hostport &remote)
{
	const m::room room
	{
		room_id
	};

	// When no user_id is supplied and the room exists locally we attempt
	// to find the user_id of one of our users with membership in the room.
	// This satisfies synapse's requirements for whether we have access
	// to the response. If user_id remains blank then make_join will later
	// generate a random one from our host as well.
	m::user::id::buf user_id
	{
		any_user(room, my_host(), "join")
	};

	// Make another attempt to find an invited user because that carries some
	// value (this query is not as fast as querying join memberships).
	if(!user_id)
		user_id = any_user(room, my_host(), "invite");

	return fetch_head(room_id, remote, user_id);
}

ircd::m::event::id::buf
ircd::m::fed::fetch_head(const id::room &room_id,
                         const net::hostport &remote,
                         const id::user &user_id)
{
	const unique_buffer<mutable_buffer> buf
	{
		16_KiB
	};

	make_join::opts opts;
	opts.remote = remote;
	make_join request
	{
		room_id, user_id, buf, std::move(opts)
	};

	request.wait(milliseconds(fetch_head_timeout));
	request.get();

	const json::object proto
	{
		request.in.content
	};

	const json::object event
	{
		proto.at("event")
	};

	const m::event::prev prev
	{
		event
	};

	const auto &prev_event_id
	{
		prev.prev_event(0)
	};

	return prev_event_id;
}

ircd::net::hostport
ircd::m::fed::well_known(const mutable_buffer &buf,
                         const net::hostport &remote)
try
{
	const unique_buffer<mutable_buffer> head_buf
	{
		16_KiB
	};

	window_buffer wb{head_buf};
	http::request
	{
		wb, host(remote), "GET", "/.well-known/matrix/server",
	};

	// Hard target https service; do not inherit matrix service from remote.
	const net::hostport target
	{
		host(remote), "https", port(remote)
	};

	const const_buffer out_head
	{
		wb.completed()
	};

	// Remaining space in buffer is used for received head
	const mutable_buffer in_head
	{
		buf + size(out_head)
	};

	server::request::opts opts;
	server::request request
	{
		target,
		{ out_head },
		{ in_head, buf },
		&opts
	};

	const auto code
	{
		request.get(seconds(8)) //TODO: XXX conf
	};

	const json::object &response
	{
		request.in.content
	};

	const json::string &m_server
	{
		response["m.server"]
	};

	const net::hostport ret
	{
		m_server
	};

	thread_local char rembuf[rfc3986::DOMAIN_BUFSIZE * 2];
	log::debug
	{
		log, "Well-known matrix server query to %s %u %s :%s",
		string(rembuf, target),
		uint(code),
		http::status(code),
		string_view{response},
	};

	return matrix_service(ret);
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	thread_local char rembuf[rfc3986::DOMAIN_BUFSIZE * 2];
	log::derror
	{
		log, "Matrix server well-known query for %s :%s",
		string(rembuf, remote),
		e.what(),
	};

	return remote;
}
