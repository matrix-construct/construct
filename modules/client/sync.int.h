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

extern conf::item<size_t> sync_flush_hiwat;

/// Argument parser for the client's /sync request
struct syncargs
{
	string_view filter_id;
	uint64_t since;
	steady_point timesout;
	bool full_state;
	bool set_presence;

	syncargs(const resource::request &);
};

struct shortpoll
{
	ircd::client &client;
	const resource::request &request;
	const syncargs &args;

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

	unique_buffer<mutable_buffer> buf {96_KiB};
	std::unique_ptr<resource::response::chunked> response;
	json::stack out
	{
		buf, [this](const const_buffer &buf)
		{
			if(!committed)
				return buf;

			if(!response)
				response = std::make_unique<resource::response::chunked>
				(
					client, http::OK, "application/json; charset=utf-8"
				);

			response->write(buf);
			return buf;
		},
		size_t(sync_flush_hiwat)
	};

	shortpoll(ircd::client &client,
	          const resource::request &request,
	          const syncargs &args)
	:client{client}
	,request{request}
	,args{args}
	{}
};

static void longpoll_sync(client &, const resource::request &, const syncargs &);
static bool polylog_sync(client &, const resource::request &, shortpoll &, json::stack::object &);
static bool linear_sync(client &, const resource::request &, shortpoll &, json::stack::object &);
static bool shortpoll_sync(client &, const resource::request &, const syncargs &);
static resource::response since_sync(client &, const resource::request &, const syncargs &);
static resource::response initial_sync(client &, const resource::request &, const syncargs &);
static resource::response sync(client &, const resource::request &);
extern resource::method get_sync;

extern const string_view sync_description;
extern resource sync_resource;

void on_unload();
extern mapi::header IRCD_MODULE;
