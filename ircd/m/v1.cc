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
// v1/groups.h
//

ircd::m::v1::groups::publicised::publicised(const id::node &node,
                                            const vector_view<const id::user> &user_ids,
                                            const mutable_buffer &buf_,
                                            opts opts)
:server::request{[&]
{
	if(!opts.remote)
		opts.remote = node.host();

	if(!defined(json::get<"origin"_>(opts.request)))
		json::get<"origin"_>(opts.request) = my_host();

	if(!defined(json::get<"destination"_>(opts.request)))
		json::get<"destination"_>(opts.request) = node.host();

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
		opts.remote, std::move(opts.out), std::move(opts.in), opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// v1/send.h
//

void
ircd::m::v1::send::response::for_each_pdu(const pdus_closure &closure)
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

ircd::m::v1::send::send(const string_view &txnid,
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
			url::encode(txnid, txnidbuf),
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
		opts.remote, std::move(opts.out), std::move(opts.in), opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// v1/public_rooms.h
//

ircd::m::v1::public_rooms::public_rooms(const net::hostport &remote,
                                        const mutable_buffer &buf)
:public_rooms
{
	remote, buf, opts{}
}
{
}

ircd::m::v1::public_rooms::public_rooms(const net::hostport &remote,
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
			    << url::encode(opts.since, since);

		if(opts.third_party_instance_id)
			qss << "&third_party_instance_id="
			    << url::encode(opts.third_party_instance_id, tpid);

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
		opts.remote, std::move(opts.out), std::move(opts.in), opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// v1/backfill.h
//

ircd::m::v1::backfill::backfill(const room::id &room_id,
                                const mutable_buffer &buf)
:backfill
{
	room_id, buf, opts{}
}
{
}

ircd::m::v1::backfill::backfill(const room::id &room_id,
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
			url::encode(room_id, ridbuf),
			opts.limit,
			url::encode(opts.event_id, eidbuf),
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
		opts.remote, std::move(opts.out), std::move(opts.in), opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// v1/state.h
//

ircd::m::v1::state::state(const room::id &room_id,
                          const mutable_buffer &buf)
:state
{
	room_id, buf, opts{}
}
{
}

ircd::m::v1::state::state(const room::id &room_id,
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
			urlbuf, "/_matrix/federation/v1/%s/%s/?event_id=%s",
			opts.ids_only? "state_ids" : "state",
			url::encode(room_id, ridbuf),
			url::encode(opts.event_id, eidbuf),
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
		opts.remote, std::move(opts.out), std::move(opts.in), opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// v1/event_auth.h
//

ircd::m::v1::event_auth::event_auth(const m::room::id &room_id,
                                    const m::event::id &event_id,
                                    const mutable_buffer &buf)
:event_auth
{
	room_id, event_id, buf, opts{}
}
{
}

ircd::m::v1::event_auth::event_auth(const m::room::id &room_id,
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
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			urlbuf, "/_matrix/federation/v1/event_auth/%s/%s",
			url::encode(room_id, ridbuf),
			url::encode(event_id, eidbuf),
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
		opts.remote, std::move(opts.out), std::move(opts.in), opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// v1/event.h
//

ircd::m::v1::event::event(const m::event::id &event_id,
                          const mutable_buffer &buf)
:event
{
	event_id, buf, opts{}
}
{
}

ircd::m::v1::event::event(const m::event::id &event_id,
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
			url::encode(event_id, eidbuf),
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
		opts.remote, std::move(opts.out), std::move(opts.in), opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// v1/invite.h
//

ircd::m::v1::invite::invite(const room::id &room_id,
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
			url::encode(room_id, ridbuf),
			url::encode(event_id, eidbuf)
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
		opts.remote, std::move(opts.out), std::move(opts.in), opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// v1/send_join.h
//

ircd::m::v1::send_join::send_join(const room::id &room_id,
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
			url::encode(room_id, ridbuf),
			url::encode(event_id, uidbuf)
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
		opts.remote, std::move(opts.out), std::move(opts.in), opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// v1/make_join.h
//

ircd::m::v1::make_join::make_join(const room::id &room_id,
                                  const id::user &user_id,
                                  const mutable_buffer &buf)
:make_join
{
	room_id, user_id, buf, opts{}
}
{
}

ircd::m::v1::make_join::make_join(const room::id &room_id,
                                  const id::user &user_id,
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

	if(!defined(json::get<"uri"_>(opts.request)))
	{
		thread_local char urlbuf[2048], ridbuf[768], uidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			urlbuf, "/_matrix/federation/v1/make_join/%s/%s",
			url::encode(room_id, ridbuf),
			url::encode(user_id, uidbuf)
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
		opts.remote, std::move(opts.out), std::move(opts.in), opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// v1/user.h
//

ircd::m::v1::user::devices::devices(const id::user &user_id,
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
			url::encode(user_id, uidbuf)
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
		opts.remote, std::move(opts.out), std::move(opts.in), opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// v1/query.h
//

namespace ircd::m::v1
{
	thread_local char query_arg_buf[1024];
	thread_local char query_url_buf[1024];
}

ircd::m::v1::query::client_keys::client_keys(const id::user &user_id,
                                             const string_view &device_id,
                                             const mutable_buffer &buf)
:client_keys
{
	user_id, device_id, buf, opts{user_id.host()}
}
{
}

ircd::m::v1::query::client_keys::client_keys(const id::user &user_id,
                                             const string_view &device_id,
                                             const mutable_buffer &buf,
                                             opts opts)
:query{[&]() -> query
{
	const json::value device_ids[]
	{
		{ device_id }
	};

	const json::members body
	{
		{ "device_keys", json::members
		{
			{ string_view{user_id}, { device_ids, 1 } }
		}}
	};

	mutable_buffer out{buf};
	const string_view content
	{
		stringify(out, body)
	};

	json::get<"content"_>(opts.request) = content;
	return
	{
		"client_keys", string_view{}, out, std::move(opts)
	};
}()}
{
}

ircd::m::v1::query::user_devices::user_devices(const id::user &user_id,
                                               const mutable_buffer &buf)
:user_devices
{
	user_id, buf, opts{user_id.host()}
}
{
}

ircd::m::v1::query::user_devices::user_devices(const id::user &user_id,
                                               const mutable_buffer &buf,
                                               opts opts)
:query
{
	"user_devices",
	fmt::sprintf
	{
		query_arg_buf, "user_id=%s", url::encode(user_id, query_url_buf)
	},
	buf,
	std::move(opts)
}
{
}

ircd::m::v1::query::directory::directory(const id::room_alias &room_alias,
                                         const mutable_buffer &buf)
:directory
{
	room_alias, buf, opts{room_alias.host()}
}
{
}

ircd::m::v1::query::directory::directory(const id::room_alias &room_alias,
                                         const mutable_buffer &buf,
                                         opts opts)
:query
{
	"directory",
	fmt::sprintf
	{
		query_arg_buf, "room_alias=%s", url::encode(room_alias, query_url_buf)
	},
	buf,
	std::move(opts)
}
{
}

ircd::m::v1::query::profile::profile(const id::user &user_id,
                                     const mutable_buffer &buf)
:profile
{
	user_id, buf, opts{user_id.host()}
}
{
}

ircd::m::v1::query::profile::profile(const id::user &user_id,
                                     const mutable_buffer &buf,
                                     opts opts)
:query
{
	"profile",
	fmt::sprintf
	{
		query_arg_buf, "user_id=%s", url::encode(user_id, query_url_buf)
	},
	buf,
	std::move(opts)
}
{
}

ircd::m::v1::query::profile::profile(const id::user &user_id,
                                     const string_view &field,
                                     const mutable_buffer &buf)
:profile
{
	user_id, field, buf, opts{user_id.host()}
}
{
}

ircd::m::v1::query::profile::profile(const id::user &user_id,
                                     const string_view &field,
                                     const mutable_buffer &buf,
                                     opts opts)
:query
{
	"profile",
	fmt::sprintf
	{
		query_arg_buf, "user_id=%s%s%s",
		url::encode(string_view{user_id}, query_url_buf),
		!empty(field)? "&field=" : "",
		field
	},
	buf,
	std::move(opts)
}
{
}

ircd::m::v1::query::query(const string_view &type,
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
		opts.remote, std::move(opts.out), std::move(opts.in), opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// v1/key.h
//

namespace ircd::m::v1
{
	static const_buffer
	_make_server_keys(const vector_view<const key::server_key> &,
	                  const mutable_buffer &);
}

ircd::m::v1::key::key(const vector_view<const server_key> &keys,
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
		opts.remote, std::move(opts.out), std::move(opts.in), opts.sopts
	};
}()}
{
}

static ircd::const_buffer
ircd::m::v1::_make_server_keys(const vector_view<const key::server_key> &keys,
                               const mutable_buffer &buf)
{
	json::stack out{buf};
	{
		json::stack::object top{out};
		json::stack::member server_keys{top, "server_keys"};
		for(const auto &sk : keys)
		{
			json::stack::object object{server_keys};
			json::stack::member server_name{object, sk.first};
			{
				json::stack::object object{server_name};
				json::stack::member key_name{object, sk.second};
				{
					json::stack::object object{key_name};
					json::stack::member mvut
					{
						object, "minimum_valid_until_ts", json::value{0L}
					};
				}
			}
		}
	}

	return out.completed();
}

///////////////////////////////////////////////////////////////////////////////
//
// v1/version.h
//

ircd::m::v1::version::version(const mutable_buffer &buf,
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
		opts.remote, std::move(opts.out), std::move(opts.in), opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// v1/v1.h
//

ircd::conf::item<ircd::milliseconds>
fetch_head_timeout
{
	{ "name",     "ircd.m.v1.fetch_head.timeout" },
	{ "default",  30 * 1000L                     },
};

ircd::m::event::id::buf
ircd::m::v1::fetch_head(const id::room &room_id,
                        const net::hostport &remote)
{
	return fetch_head(room_id, remote, m::me.user_id);
}

ircd::m::event::id::buf
ircd::m::v1::fetch_head(const id::room &room_id,
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

	const json::array prev_events
	{
		proto.at({"event", "prev_events"})
	};

	const json::array prev_event
	{
		prev_events.at(0)
	};

	const auto &prev_event_id
	{
		prev_event.at(0)
	};

	return unquote(prev_event_id);
}
