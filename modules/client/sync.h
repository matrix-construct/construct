// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::sync
{
	struct args;
	struct stats;
	struct shortpoll;

	extern log::log log;
	extern const string_view description;
	extern resource resource;
	extern resource::method method_get;

	static long notification_count(const room &, const event::idx &a, const event::idx &b);
	static long highlight_count(const room &, const user &, const event::idx &a, const event::idx &b);
	static resource::response handle_get(client &, const resource::request &);
}

namespace ircd::m::sync::longpoll
{
	struct accepted
	:m::event
	{
		json::strung strung;
		std::string client_txnid;

		accepted(const m::vm::eval &eval)
		:strung
		{
			*eval.event_
		}
		,client_txnid
		{
			eval.copts?
				eval.copts->client_txnid:
				string_view{}
		}
		{
			const json::object object{this->strung};
			static_cast<m::event &>(*this) = m::event{object};
		}

		accepted(accepted &&) = default;
		accepted(const accepted &) = delete;
	};

	size_t polling {0};
	std::deque<accepted> queue;
	ctx::dock dock;

	static std::string sync_room(client &, const m::room &, const args &, const accepted &);
	static std::string sync_rooms(client &, const m::user::id &, const m::room &, const args &, const accepted &);
	static bool handle(client &, const args &, const accepted &, const m::room &);
	static bool handle(client &, const args &, const accepted &);
	static void poll(client &, const args &);

	static void handle_notify(const m::event &, m::vm::eval &);
	extern m::hookfn<m::vm::eval &> notified;
}

namespace ircd::m::sync::linear
{
	static bool handle(client &, shortpoll &, json::stack::object &);
}

namespace ircd::m::sync::polylog
{
	extern conf::item<bool> prefetch_state;
	extern conf::item<bool> prefetch_timeline;

	static void room_state(shortpoll &, json::stack::object &, const m::room &);
	static m::event::id::buf room_timeline_events(shortpoll &, json::stack::array &, const m::room &, bool &limited);
	static void room_timeline(shortpoll &, json::stack::object &, const m::room &);
	static void room_ephemeral_events(shortpoll &, json::stack::array &, const m::room &);
	static void room_ephemeral(shortpoll &, json::stack::object &, const m::room &);
	static void room_account_data(shortpoll &, json::stack::object &, const m::room &);
	static void room_unread_notifications(shortpoll &, json::stack::object &, const m::room &);
	static void sync_room(shortpoll &, json::stack::object &, const m::room &, const string_view &membership);
	static void sync_rooms(shortpoll &, json::stack::object &, const string_view &membership);
	static void rooms(shortpoll &, json::stack::object &);
	static void presence(shortpoll &, json::stack::object &);
	static void account_data(shortpoll &, json::stack::object &);
	static bool handle(client &, shortpoll &, json::stack::object &);
}

/// Argument parser for the client's /sync request
struct ircd::m::sync::args
{
	static conf::item<milliseconds> timeout_max;
	static conf::item<milliseconds> timeout_min;
	static conf::item<milliseconds> timeout_default;

	args(const resource::request &request)
	:request{request}
	{}

	const resource::request &request;

	string_view filter_id
	{
		// 6.2.1 The ID of a filter created using the filter API or a filter JSON object
		// encoded as a string. The server will detect whether it is an ID or a JSON object
		// by whether the first character is a "{" open brace. Passing the JSON inline is best
		// suited to one off requests. Creating a filter using the filter API is recommended
		// for clients that reuse the same filter multiple times, for example in long poll requests.
		request.query["filter"]
	};

	uint64_t since
	{
		// 6.2.1 A point in time to continue a sync from.
		request.query.get<uint64_t>("since", 0)
	};

	steady_point timesout{[this]
	{
		// 6.2.1 The maximum time to poll in milliseconds before returning this request.
		auto ret(request.query.get("timeout", milliseconds(timeout_default)));
		ret = std::min(ret, milliseconds(timeout_max));
		ret = std::max(ret, milliseconds(timeout_min));
		return now<steady_point>() + ret;
	}()};

	bool full_state
	{
		// 6.2.1 Controls whether to include the full state for all rooms the user is a member of.
		// If this is set to true, then all state events will be returned, even if since is non-empty.
		// The timeline will still be limited by the since parameter. In this case, the timeout
		// parameter will be ignored and the query will return immediately, possibly with an
		// empty timeline. If false, and since is non-empty, only state which has changed since
		// the point indicated by since will be returned. By default, this is false.
		request.query.get("full_state", false)
	};

	bool set_presence
	{
		// 6.2.1 Controls whether the client is automatically marked as online by polling this API.
		// If this parameter is omitted then the client is automatically marked as online when it
		// uses this API. Otherwise if the parameter is set to "offline" then the client is not
		// marked as being online when it uses this API. One of: ["offline"]
		request.query.get("set_presence", true)
	};
};

struct ircd::m::sync::stats
{
	ircd::timer timer;
	size_t flush_bytes {0};
	size_t flush_count {0};
};

struct ircd::m::sync::shortpoll
{
	static conf::item<size_t> flush_hiwat;

	shortpoll(ircd::client &client,
	          const sync::args &args)
	:client{client}
	,args{args}
	{}

	sync::stats stats;
	ircd::client &client;
	const sync::args &args;
	const resource::request &request
	{
		args.request
	};

	const uint64_t &since
	{
		args.since
	};

	const uint64_t current
	{
		m::vm::current_sequence
	};

	const uint64_t delta
	{
		current - since
	};

	const m::user user
	{
		request.user_id
	};

	const std::string filter_buf
	{
		args.filter_id?
			user.filter(std::nothrow, args.filter_id):
			std::string{}
	};

	const m::filter filter
	{
		json::object{filter_buf}
	};

	const m::user::room user_room
	{
		user
	};

	const m::user::rooms rooms
	{
		user
	};

	uint64_t state_at
	{
		0
	};

	bool committed
	{
		false
	};

	unique_buffer<mutable_buffer> buf
	{
		96_KiB
	};

	std::unique_ptr<resource::response::chunked> response;
	json::stack out
	{
		buf, std::bind(&shortpoll::flush, this, ph::_1), size_t(flush_hiwat)
	};

	void commit()
	{
		response = std::make_unique<resource::response::chunked>
		(
			client, http::OK, "application/json; charset=utf-8"
		);
	}

	const_buffer flush(const const_buffer &buf)
	{
		if(!committed)
			return buf;

		if(!response)
			commit();

		stats.flush_bytes += response->write(buf);
		stats.flush_count++;
		return buf;
	}
};
