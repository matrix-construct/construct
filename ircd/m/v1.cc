// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/m/m.h>

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
		const auto in_max
		{
			std::max(ssize_t(size(buf) - size(opts.out.head)), ssize_t(0))
		};

		assert(in_max >= ssize_t(size(buf) / 2));
		opts.in.head = { data(buf) + size(opts.out.head), size_t(in_max) };
		opts.in.content = opts.in.head;
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

decltype(ircd::m::v1::backfill::default_opts)
ircd::m::v1::backfill::default_opts
{};

ircd::m::v1::backfill::backfill(const room::id &room_id,
                                const mutable_buffer &buf)
:backfill
{
	room_id, buf, default_opts
}
{
}

ircd::m::v1::backfill::backfill(const room::id &room_id,
                                const mutable_buffer &buf,
                                opts opts)
:server::request{[&]
{
	if(!opts.event_id)
	{
		thread_local m::event::id::buf event_id;
		v1::make_join request
		{
			room_id, m::me.user_id, buf
		};

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

		event_id = unquote(prev_event.at(0));
		opts.event_id = event_id;
	}

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
		const auto in_max
		{
			std::max(ssize_t(size(buf) - size(opts.out.head)), ssize_t(0))
		};

		assert(in_max >= ssize_t(size(buf) / 2));
		opts.in.head = { data(buf) + size(opts.out.head), size_t(in_max) };
		opts.in.content = opts.in.head;
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

decltype(ircd::m::v1::state::default_opts)
ircd::m::v1::state::default_opts
{};

ircd::m::v1::state::state(const room::id &room_id,
                          const mutable_buffer &buf)
:state
{
	room_id, buf, default_opts
}
{
}

ircd::m::v1::state::state(const room::id &room_id,
                          const mutable_buffer &buf,
                          opts opts)
:server::request{[&]
{
	if(!opts.event_id)
	{
		thread_local m::event::id::buf event_id;
		v1::make_join request
		{
			room_id, m::me.user_id, buf
		};

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

		event_id = unquote(prev_event.at(0));
		opts.event_id = event_id;
	}

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
		thread_local char urlbuf[2048], ridbuf[768], eidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			urlbuf, "/_matrix/federation/v1/state/%s/?event_id=%s",
			url::encode(room_id, ridbuf),
			url::encode(opts.event_id, eidbuf),
		};
	}

	json::get<"method"_>(opts.request) = "GET";
	opts.out.head = opts.request(buf);

	if(!size(opts.in))
	{
		const auto in_max
		{
			std::max(ssize_t(size(buf) - size(opts.out.head)), ssize_t(0))
		};

		assert(in_max >= ssize_t(size(buf) / 2));
		opts.in.head = { data(buf) + size(opts.out.head), size_t(in_max) };
		opts.in.content = opts.in.head;
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

decltype(ircd::m::v1::event::default_opts)
ircd::m::v1::event::default_opts
{};

ircd::m::v1::event::event(const m::event::id &event_id,
                          const mutable_buffer &buf)
:event
{
	event_id, buf, default_opts
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
		const auto in_max
		{
			std::max(ssize_t(size(buf) - size(opts.out.head)), ssize_t(0))
		};

		assert(in_max >= ssize_t(size(buf) / 2));
		opts.in.head = { data(buf) + size(opts.out.head), size_t(in_max) };
		opts.in.content = opts.in.head;
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

decltype(ircd::m::v1::send_join::default_opts)
ircd::m::v1::send_join::default_opts
{};

ircd::m::v1::send_join::send_join(const room::id &room_id,
                                  const user::id &user_id,
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
			url::encode(user_id, uidbuf)
		};
	}

	json::get<"method"_>(opts.request) = "PUT";
	opts.out.head = opts.request(buf);

	if(!size(opts.in))
	{
		const auto in_max
		{
			std::max(ssize_t(size(buf) - size(opts.out.head)), ssize_t(0))
		};

		assert(in_max >= ssize_t(size(buf) / 2));
		opts.in.head = { data(buf) + size(opts.out.head), size_t(in_max) };
		opts.in.content = opts.in.head;
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

decltype(ircd::m::v1::make_join::default_opts)
ircd::m::v1::make_join::default_opts
{};

ircd::m::v1::make_join::make_join(const room::id &room_id,
                                  const user::id &user_id,
                                  const mutable_buffer &buf)
:make_join
{
	room_id, user_id, buf, default_opts
}
{
}

ircd::m::v1::make_join::make_join(const room::id &room_id,
                                  const user::id &user_id,
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
		const auto in_max
		{
			std::max(ssize_t(size(buf) - size(opts.out.head)), ssize_t(0))
		};

		assert(in_max >= ssize_t(size(buf) / 2));
		opts.in.head = { data(buf) + size(opts.out.head), size_t(in_max) };
		opts.in.content = opts.in.head;
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
}

ircd::m::v1::query::directory::directory(const id::room_alias &room_alias,
                                         const mutable_buffer &buf,
                                         opts opts)
:query
{
	"directory",
	fmt::sprintf
	{
		query_arg_buf, "room_alias=%s", string_view{room_alias}
	},
	buf,
	opts
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
		query_arg_buf, "user_id=%s", string_view{user_id}
	},
	buf,
	opts
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
		string_view{user_id},
		!empty(field)? "&field=" : "",
		field
	},
	buf,
	opts
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
			urlbuf, "/_matrix/federation/v1/query/%s?%s",
			type,
			args
		};
	}

	json::get<"method"_>(opts.request) = "GET";
	opts.out.head = opts.request(buf);

	if(!size(opts.in))
	{
		const auto in_max
		{
			std::max(ssize_t(size(buf) - size(opts.out.head)), ssize_t(0))
		};

		assert(in_max >= ssize_t(size(buf) / 2));
		opts.in.head = { data(buf) + size(opts.out.head), size_t(in_max) };
		opts.in.content = opts.in.head;
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
		const auto in_max
		{
			std::max(ssize_t(size(buf) - size(opts.out.head)), ssize_t(0))
		};

		assert(in_max >= ssize_t(size(buf) / 2));
		opts.in.head = { data(buf) + size(opts.out.head), size_t(in_max) };
		opts.in.content = opts.in.head;
	}

	return server::request
	{
		opts.remote, std::move(opts.out), std::move(opts.in), opts.sopts
	};
}()}
{
}
