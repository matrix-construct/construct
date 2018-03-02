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

static resource::response
get__initialsync_remote(client &client,
                        const resource::request &request,
                        const m::room::id &room_id)
{
	// only used for headers
	const unique_buffer<mutable_buffer> state_buffer
	{
		8_KiB
	};

	// only used for headers
	const unique_buffer<mutable_buffer> backfill_buffer
	{
		8_KiB
	};

	m::v1::state state_request
	{
		room_id, state_buffer,
	};

	m::v1::backfill::opts bf_opts;
	bf_opts.limit = 1024;
	m::v1::backfill backfill_request
	{
		room_id, backfill_buffer, std::move(bf_opts)
	};

	const auto when
	{
		ircd::now<steady_point>() + seconds(15) //TODO: conf
	};

	std::vector<m::event> messages;
	messages.reserve(bf_opts.limit);
	if(backfill_request.wait_until(when) != ctx::future_status::timeout)
	{
		const auto &code{backfill_request.get()};
		const json::object &response{backfill_request};
		const json::array &pdus
		{
			response.get("pdus")
		};

		for(const json::object &event : pdus)
			messages.emplace_back(event);

		std::sort(std::begin(messages), std::end(messages), []
		(const auto &a, const auto &b)
		{
			return at<"depth"_>(a) < at<"depth"_>(b);
		});
	}
	else cancel(backfill_request);

	std::vector<m::event> state;
	if(state_request.wait_until(when) != ctx::future_status::timeout)
	{
		const auto &code{state_request.get()};
		const json::object &response{state_request};
		const json::array &auth_chain
		{
			response.get("auth_chain")
		};

		const json::array &pdus
		{
			response.get("pdus")
		};

		state.reserve(auth_chain.size() + pdus.size());
		for(const json::object &event : auth_chain)
			state.emplace_back(event);

		for(const json::object &event : pdus)
			state.emplace_back(event);

		std::sort(std::begin(state), std::end(state), []
		(const auto &a, const auto &b)
		{
			return at<"depth"_>(a) < at<"depth"_>(b);
		});
	}
	else cancel(state_request);

	const string_view membership{"join"};
	const string_view visibility{"public"};
	std::vector<json::value> account_data;

	const json::strung chunk
	{
		messages.data(), messages.data() + messages.size()
	};

	const json::strung states
	{
		state.data(), state.data() + state.size()
	};

	resource::response
	{
		client, json::members
		{
			{ "room_id",       room_id                                        },
			{ "membership",    membership                                     },
			{ "state",         states                                         },
			{ "visibility",    visibility                                     },
			{ "account_data",  { account_data.data(), account_data.size() }   },
			{ "messages",      json::members
			{
				{ "chunk",  chunk }
			}},
		}
	};

	m::vm::opts vmopts;
	m::vm::eval eval{vmopts};

	vmopts.non_conform.set(m::event::conforms::MISSING_PREV_STATE);
	vmopts.non_conform.set(m::event::conforms::MISSING_MEMBERSHIP);
	vmopts.prev_check_exists = false;
	vmopts.nothrows = -1U;

	for(const auto &event : state)
		eval(event);

	for(const auto &event : messages)
		eval(event);

	return {}; // already returned
}

resource::response
get__initialsync(client &client,
                 const resource::request &request,
                 const m::room::id &room_id)
{
	if(!exists(room_id))
	{
		if(!my(room_id))
			return get__initialsync_remote(client, request, room_id);

		throw m::NOT_FOUND
		{
			"room_id '%s' does not exist", room_id
		};
	}

	const m::room room
	{
		room_id
	};

	std::vector<std::string> state;
	{
		const m::room::state state_
		{
			room_id
		};

		state.reserve(state_.count());
		state_.for_each([&state](const m::event &event)
		{
			state.emplace_back(json::strung(event));
		});
	}

	std::vector<std::string> messages;
	{
		messages.reserve(20);
		m::room::messages it{room};
		for(; it && messages.size() < 20; --it)
			messages.emplace_back(json::strung(*it));

		std::reverse(begin(messages), end(messages));
	}

	char membership_buffer[64];
	const string_view membership
	{
		request.user_id? room.membership(membership_buffer, m::user::id{request.user_id}) : string_view{}
	};

	//TODO: XXX
	const string_view visibility{"public"};

	//TODO: XXX
	std::vector<json::value> account_data;
	const json::value account_datas
	{
		account_data.data(), account_data.size()
	};

	//TODO: derp
	const json::strung states
	{
		state.data(), state.data() + state.size()
	};

	//TODO: derp
	const json::strung messages_chunk
	{
		messages.data(), messages.data() + messages.size()
	};

	const string_view &messages_start
	{
		!messages.empty()?
			unquote(json::object{messages.front()}.at("event_id")):
			string_view{}
	};

	const string_view &messages_end
	{
		!messages.empty()?
			unquote(json::object{messages.back()}.at("event_id")):
			string_view{}
	};

	return resource::response
	{
		client, json::members
		{
			{ "room_id",       room_id        },
			{ "membership",    membership     },
			{ "state",         states,        },
			{ "visibility",    visibility     },
			{ "account_data",  account_datas  },
			{ "messages",      json::members
			{
				{ "start", messages_start },
				{ "chunk", messages_chunk },
				{ "end", messages_end },
			}},
		}
	};
}
