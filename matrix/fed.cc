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
:request{[&]
{
	if(likely(!opts.remote))
		opts.remote = node;

	if(likely(!defined(json::get<"uri"_>(opts.request))))
		json::get<"uri"_>(opts.request) = "/_matrix/federation/v1/get_groups_publicised";

	if(likely(!defined(json::get<"method"_>(opts.request))))
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

	return request
	{
		buf, std::move(opts)
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

	for(const auto &[event_id, error] : pdus)
		closure(event_id, error);
}

ircd::m::fed::send::send(const txn::array &pdu,
                         const txn::array &edu,
                         const mutable_buffer &buf_,
                         opts opts)
:send{[&]
{
	assert(!!opts.remote);

	mutable_buffer buf{buf_};
	const string_view &content
	{
		txn::create(buf, pdu, edu)
	};

	consume(buf, size(content));
	const string_view &txnid
	{
		txn::create_id(buf, content)
	};

	consume(buf, size(txnid));
	return send
	{
		txnid, content, buf, std::move(opts)
	};
}()}
{
}

ircd::m::fed::send::send(const string_view &txnid,
                         const const_buffer &content,
                         const mutable_buffer &buf_,
                         opts opts)
:request{[&]
{
	assert(!!opts.remote);

	assert(!size(opts.out.content));
	assert(!defined(json::get<"content"_>(opts.request)));
	json::get<"content"_>(opts.request) = json::object
	{
		content
	};

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "PUT";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char txnidbuf[256];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v1/send/%s",
			url::encode(txnidbuf, txnid),
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/rooms.h
//

ircd::m::fed::rooms::complexity::complexity(const m::id::room &room_id,
                                            const mutable_buffer &buf_,
                                            opts opts)
:request{[&]
{
	if(!opts.remote)
		opts.remote = room_id.host();

	window_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char ridbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/unstable/rooms/%s/complexity",
			url::encode(ridbuf, room_id)
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "GET";

	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/public_rooms.h
//

ircd::m::fed::public_rooms::public_rooms(const string_view &remote,
                                         const mutable_buffer &buf_,
                                         opts opts)
:request{[&]
{
	if(!opts.remote)
		opts.remote = remote;

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "POST";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char query[2048], tpid[1024], since[1024];
		std::stringstream qss;
		pubsetbuf(qss, query);

		if(json::get<"method"_>(opts.request) == "GET")
		{
			if(opts.since)
				qss << "&since="
				    << url::encode(since, opts.since);

			if(opts.third_party_instance_id)
				qss << "&third_party_instance_id="
				    << url::encode(tpid, opts.third_party_instance_id);
		}

		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v1/publicRooms?limit=%zu%s%s",
			opts.limit,
			opts.include_all_networks?
				"&include_all_networks=true"_sv:
				string_view{},
			view(qss, query)
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	if(likely(!defined(json::get<"content"_>(opts.request))))
	{
		json::stack out{buf};
		json::stack::object top{out};
		if(likely(json::get<"method"_>(opts.request) == "POST"))
		{
			if(opts.limit)
				json::stack::member
				{
					top, "limit", json::value
					{
						long(opts.limit)
					}
				};

			if(opts.include_all_networks)
				json::stack::member
				{
					top, "include_all_networks", json::value
					{
						opts.include_all_networks
					}
				};

			if(opts.third_party_instance_id)
				json::stack::member
				{
					top, "third_party_instance_id", json::value
					{
						opts.third_party_instance_id
					}
				};

			if(opts.search_term)
			{
				json::stack::object filter
				{
					top, "filter"
				};

				json::stack::member
				{
					filter, "generic_search_term", opts.search_term
				};
			}
		}

		top.~object();
		json::get<"content"_>(opts.request) = out.completed();
		consume(buf, size(string_view(json::get<"content"_>(opts.request))));
	}

	return request
	{
		buf, std::move(opts)
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
:request{[&]
{
	assert(!!opts.remote);

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "POST";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char ridbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v1/get_missing_events/%s",
			url::encode(ridbuf, room_id)
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	if(likely(!defined(json::get<"content"_>(opts.request))))
	{
		json::get<"content"_>(opts.request) = json::object
		{
			make_content(buf, pair, opts)
		};

		consume(buf, size(string_view(json::get<"content"_>(opts.request))));
	}

	return request
	{
		buf, std::move(opts)
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

		// earliest
		{
			json::stack::array array
			{
				top, "earliest_events"
			};

			for(const auto &id : pair.first)
				if(likely(id))
					array.append(id);
		}

		// latest
		{
			json::stack::array array
			{
				top, "latest_events"
			};

			for(const auto &id : pair.second)
				if(likely(id))
					array.append(id);
		}

		json::stack::member
		{
			top, "limit", json::value
			{
				int64_t(opts.limit)
			}
		};

		json::stack::member
		{
			top, "min_depth", json::value
			{
				int64_t(opts.min_depth)
			}
		};
	}

	return out.completed();
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/backfill.h
//

ircd::m::fed::backfill::backfill(const room::id &room_id,
                                 const mutable_buffer &buf_,
                                 opts opts)
:request{[&]
{
	m::event::id::buf event_id_buf;
	if(!opts.event_id)
	{
		event_id_buf = fetch_head(room_id, opts.remote);
		opts.event_id = event_id_buf;
	}

	if(!opts.remote)
		opts.remote = room_id.host();

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "GET";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char ridbuf[768], eidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v1/backfill/%s/?limit=%zu&v=%s",
			url::encode(ridbuf, room_id),
			opts.limit,
			url::encode(eidbuf, opts.event_id),
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/state.h
//

ircd::m::fed::state::state(const room::id &room_id,
                           const mutable_buffer &buf_,
                           opts opts)
:request{[&]
{
	if(!opts.remote)
		opts.remote = room_id.host();

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "GET";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char eidbuf[768], eidqbuf[768];
		const string_view event_id_query{fmt::sprintf
		{
			eidqbuf, "event_id=%s",
			opts.event_id?
				url::encode(eidbuf, opts.event_id):
				string_view{}
		}};

		thread_local char ridbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v1/%s/%s/?%s%s%s",
			opts.ids_only? "state_ids"_sv : "state"_sv,
			url::encode(ridbuf, room_id),
			opts.event_id?
				event_id_query:
				string_view{},
			opts.event_id && opts.ids_only?
				"&"_sv:
				string_view{},
			opts.ids_only?
				"auth_chain_ids=0"_sv:
				string_view{}
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	return request
	{
		buf, std::move(opts)
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
                                     const mutable_buffer &buf_,
                                     opts opts)
:request{[&]
{
	if(!opts.remote && event_id.version() == "1")
		opts.remote = event_id.host();

	if(likely(!defined(json::get<"content"_>(opts.request))))
		json::get<"content"_>(opts.request) = content;

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "POST";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char ridbuf[768], eidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v1/query_auth/%s/%s",
			url::encode(ridbuf, room_id),
			url::encode(eidbuf, event_id),
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	assert(!!opts.remote);
	return request
	{
		buf, std::move(opts)
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
                                     const mutable_buffer &buf_,
                                     opts opts)
:request{[&]
{
	if(!opts.remote && event_id.version() == "1")
		opts.remote = event_id.host();

	if(!opts.remote)
		opts.remote = room_id.host();

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "GET";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char ridbuf[768], eidbuf[768];
		if(opts.ids_only)
			json::get<"uri"_>(opts.request) = fmt::sprintf
			{
				buf, "/_matrix/federation/v1/state_ids/%s/?event_id=%s&pdu_ids=0",
				url::encode(ridbuf, room_id),
				url::encode(eidbuf, event_id),
			};
		else
			json::get<"uri"_>(opts.request) = fmt::sprintf
			{
				buf, "/_matrix/federation/v1/event_auth/%s/%s",
				url::encode(ridbuf, room_id),
				url::encode(eidbuf, event_id),
			};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	assert(!!opts.remote);
	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/event.h
//

ircd::m::fed::event::event(const m::event::id &event_id,
                           const mutable_buffer &buf_,
                           opts opts)
:request{[&]
{
	if(!opts.remote && event_id.version() == "1")
		opts.remote = event_id.host();

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "GET";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char eidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v1/event/%s/",
			url::encode(eidbuf, event_id),
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	assert(!!opts.remote);
	return request
	{
		buf, std::move(opts)
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
                             const mutable_buffer &buf_,
                             opts opts)
:request{[&]
{
	assert(!size(opts.out.content));
	assert(!defined(json::get<"content"_>(opts.request)));
	json::get<"content"_>(opts.request) = content;

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "PUT";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char ridbuf[768], eidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v1/invite/%s/%s",
			url::encode(ridbuf, room_id),
			url::encode(eidbuf, event_id)
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	assert(!!opts.remote);
	return request
	{
		buf, std::move(opts)
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
                               const mutable_buffer &buf_,
                               opts opts)
:request{[&]
{
	assert(!size(opts.out.content));
	assert(!defined(json::get<"content"_>(opts.request)));
	json::get<"content"_>(opts.request) = content;

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "PUT";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char ridbuf[768], eidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v2/invite/%s/%s",
			url::encode(ridbuf, room_id),
			url::encode(eidbuf, event_id)
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	assert(!!opts.remote);
	return request
	{
		buf, std::move(opts)
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
                                   const mutable_buffer &buf_,
                                   opts opts)
:request{[&]
{
	assert(!!opts.remote);

	assert(!size(opts.out.content));
	assert(!defined(json::get<"content"_>(opts.request)));
	json::get<"content"_>(opts.request) = json::object
	{
		content
	};

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "PUT";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char ridbuf[768], uidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v1/send_join/%s/%s",
			url::encode(ridbuf, room_id),
			url::encode(uidbuf, event_id)
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	return request
	{
		buf, std::move(opts)
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
                                   const mutable_buffer &buf_,
                                   opts opts)
:request{[&]
{
	if(!opts.remote)
		opts.remote = room_id.host();

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "GET";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		id::user::buf user_id_buf;
		const id::user &user_id
		{
			user_id_?: id::user
			{
				user_id_buf, id::generate, my_host()
			}
		};

		thread_local char ridbuf[768], uidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v1/make_join/%s/%s"
			"?ver=1"
			"&ver=2"
			"&ver=3"
			"&ver=4"
			"&ver=5"
			"&ver=6"
			"&ver=7"
			"&ver=8"
			"&ver=org.matrix.msc2432"
			,url::encode(ridbuf, room_id)
			,url::encode(uidbuf, user_id)
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	return request
	{
		buf, std::move(opts)
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
:request{[&]
{
	assert(!!opts.remote);

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "POST";

	if(likely(!defined(json::get<"uri"_>(opts.request))))
		json::get<"uri"_>(opts.request) = "/_matrix/federation/v1/user/keys/query";

	if(likely(!defined(json::get<"content"_>(opts.request))))
		json::get<"content"_>(opts.request) = content;

	return request
	{
		buf, std::move(opts)
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
:request{[&]
{
	assert(!!opts.remote);

	assert(!defined(json::get<"content"_>(opts.request)));
	json::get<"content"_>(opts.request) = content;

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "POST";

	if(likely(!defined(json::get<"uri"_>(opts.request))))
		json::get<"uri"_>(opts.request) = "/_matrix/federation/v1/user/keys/claim";

	return request
	{
		buf, std::move(opts)
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
                                     const mutable_buffer &buf_,
                                     opts opts)
:request{[&]
{
	if(!opts.remote)
		opts.remote = user_id.host();

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "GET";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char uidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v1/user/devices/%s",
			url::encode(uidbuf, user_id)
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	return request
	{
		buf, std::move(opts)
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
		query_arg_buf, "room_alias=%s",
		url::encode(query_url_buf, room_alias)
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
		query_arg_buf, "user_id=%s",
		url::encode(query_url_buf, user_id)
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
		!empty(field)? "&field="_sv: string_view{},
		field
	},
	buf,
	std::move(opts)
}
{
}

ircd::m::fed::query::query(const string_view &type,
                           const string_view &args,
                           const mutable_buffer &buf_,
                           opts opts)
:request{[&]
{
	assert(!!opts.remote);

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "GET";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v1/query/%s%s%s",
			type,
			args? "?"_sv: string_view{},
			args
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	return request
	{
		buf, std::move(opts)
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
                              const mutable_buffer &buf_,
                              opts opts)
:request{[&]
{
	const auto &[server_name, key_id]
	{
		server_key
	};

	if(likely(!opts.remote))
		opts.remote = server_name;

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "GET";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		if(!empty(key_id))
		{
			json::get<"uri"_>(opts.request) = fmt::sprintf
			{
				buf, "/_matrix/key/v2/server/%s/",
				key_id
			};

			consume(buf, size(json::get<"uri"_>(opts.request)));
		}
		else json::get<"uri"_>(opts.request) = "/_matrix/key/v2/server/";
	}

	return request
	{
		buf, std::move(opts)
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
:request{[&]
{
	assert(!!opts.remote);

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "POST";

	if(likely(!defined(json::get<"uri"_>(opts.request))))
		json::get<"uri"_>(opts.request) = "/_matrix/key/v2/query";

	window_buffer buf{buf_};
	if(likely(!defined(json::get<"content"_>(opts.request))))
	{
		buf([&keys](const mutable_buffer &buf)
		{
			return _make_server_keys(keys, buf);
		});

		json::get<"content"_>(opts.request) = json::object
		{
			buf.completed()
		};
	}

	return request
	{
		buf, std::move(opts)
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
:request{[&]
{
	assert(!!opts.remote);

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "GET";

	if(likely(!defined(json::get<"uri"_>(opts.request))))
		json::get<"uri"_>(opts.request) = "/_matrix/federation/v1/version";

	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/request.h
//

//
// request::request
//

ircd::m::fed::request::request(const mutable_buffer &buf_,
                               opts &&opts)
:server::request{[&]
{
	// Requestor must always provide a remote by this point.
	assert(!!opts.remote);

	// Requestor must always generate a uri by this point
	assert(defined(json::get<"uri"_>(opts.request)));

	// Default the origin to my primary homeserver
	if(likely(!defined(json::get<"origin"_>(opts.request))))
		json::get<"origin"_>(opts.request) = my_host();

	// Default the destination to the remote origin
	if(likely(!defined(json::get<"destination"_>(opts.request))))
		json::get<"destination"_>(opts.request) = opts.remote;

	// Set the outgoing HTTP content from the request's content field.
	if(likely(defined(json::get<"content"_>(opts.request))))
		opts.out.content = json::get<"content"_>(opts.request);

	// Allows for the reverse to ensure these values are set.
	if(!defined(json::get<"content"_>(opts.request)))
		json::get<"content"_>(opts.request) = json::object{opts.out.content};

	// Defaults the method as a convenience if none specified.
	if(!defined(json::get<"method"_>(opts.request)))
		json::get<"method"_>(opts.request) = "GET";

	// Perform well-known query; note that we hijack the user's buffer to make
	// this query and receive the result at the front of it. When there's no
	// well-known this fails silently by just returning the input (likely).
	const string_view &target
	{
		fed::server(buf_, opts.remote)
	};

	// Devote the remaining buffer for HTTP as otherwise intended.
	const mutable_buffer buf
	{
		buf_ + size(target)
	};

	const net::hostport remote
	{
		target
	};

	// Note that we override the HTTP Host header with the well-known
	// remote; otherwise default is the destination above which may differ.
	const http::header addl_headers[]
	{
		{ "Host", service(remote)? host(remote): target }
	};

	// Generate the request head including the X-Matrix into buffer.
	opts.out.head = opts.request(buf, addl_headers);

	// Setup some buffering features which can optimize the server::request
	if(!size(opts.in))
	{
		opts.in.head = buf + size(opts.out.head);
		opts.in.content = opts.dynamic?
			mutable_buffer{}:  // server::request will allocate new mem
			opts.in.head;      // server::request will auto partition
	}

	// Launch the request
	return server::request
	{
		remote,
		std::move(opts.out),
		std::move(opts.in),
		opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/fed.h
//

namespace ircd::m::fed
{
	template<class closure>
	static decltype(auto)
	with_server(const string_view &,
	            closure &&);
}

template<class closure>
decltype(auto)
ircd::m::fed::with_server(const string_view &origin,
                          closure&& c)
{
	char buf[rfc3986::DOMAIN_BUFSIZE];
	const auto remote
	{
		server(buf, origin)
	};

	return c(remote);
}

bool
ircd::m::fed::clear_error(const string_view &origin)
{
	return with_server(origin, []
	(const auto &remote)
	{
		return server::errclear(remote);
	});
}

bool
ircd::m::fed::avail(const string_view &origin)
{
	return with_server(origin, []
	(const auto &remote)
	{
		return server::avail(remote);
	});
}

bool
ircd::m::fed::exists(const string_view &origin)
{
	return with_server(origin, []
	(const auto &remote)
	{
		return server::exists(remote);
	});
}

bool
ircd::m::fed::linked(const string_view &origin)
{
	return with_server(origin, []
	(const auto &remote)
	{
		return server::linked(remote);
	});
}

bool
ircd::m::fed::errant(const string_view &origin)
{
	return with_server(origin, []
	(const auto &remote)
	{
		return server::errant(remote);
	});
}

ircd::string_view
ircd::m::fed::server(const mutable_buffer &buf,
                     const string_view &origin)
{
	net::hostport remote
	{
		origin
	};

	string_view target
	{
		!port(remote)?
			well_known::get(buf, host(remote)):
			origin
	};

	remote = target;
	if(!port(remote) && !service(remote))
	{
		service(remote) = m::canon_service;
		target = net::canonize(buf, remote);
	}

	return target;
}

//
// fetch_head util
//

ircd::conf::item<ircd::milliseconds>
fetch_head_timeout
{
	{ "name",     "ircd.m.v1.fetch_head.timeout" },
	{ "default",  30 * 1000L                     },
};

ircd::m::event::id::buf
ircd::m::fed::fetch_head(const id::room &room_id,
                         const string_view &remote)
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
                         const string_view &remote,
                         const id::user &user_id)
{
	const unique_buffer<mutable_buffer> buf
	{
		32_KiB
	};

	make_join::opts opts;
	opts.remote = remote;
	opts.dynamic = false;
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
