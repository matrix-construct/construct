// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

mapi::header
IRCD_MODULE
{
	"Client 14.23 :Direct Messaging"
};

static void
_join_room__m_direct(const m::event &event,
                     m::vm::eval &eval);

const m::hookfn<m::vm::eval &>
_join_room__m_direct_hookfn
{
	{
		{ "_site",          "vm.effect"     },
		{ "type",           "m.room.member" },
		{ "membership",     "join"          },
		{ "origin",         my_host()       },
	},
	_join_room__m_direct
};

void
_join_room__m_direct(const m::event &event,
                     m::vm::eval &eval)
{
	const m::user::id &user_id
	{
		at<"sender"_>(event)
	};

	const m::room room
	{
		at<"room_id"_>(event)
	};

	const auto membership_event_idx
	{
		room.get(std::nothrow, "m.room.member", user_id)
	};

	if(!membership_event_idx)
		return;

	const auto prev_membership_event_idx
	{
		m::room::state::prev(membership_event_idx)
	};

	if(!prev_membership_event_idx)
		return;

	bool is_direct {false};
	m::get(std::nothrow, prev_membership_event_idx, "content", [&is_direct]
	(const json::object &content)
	{
		if(unquote(content.get("membership")) == "invite")
			is_direct = content.get<bool>("is_direct", false);
	});

	if(!is_direct)
		return;

	m::user::id::buf other_person;
	const m::room::members members{room};
	members.for_each(m::room::members::closure_bool{[&user_id, &other_person]
	(const auto &other_id)
	{
		if(other_id == user_id)
			return true;

		other_person = other_id;
		return false;
	}});

	if(!other_person)
		return;

	const unique_buffer<mutable_buffer> buf
	{
		48_KiB
	};

	json::stack out{buf};
	json::stack::object top{out};
	const m::user::account_data account_data{user_id};
	account_data.get("m.direct", [&room, &other_person, &top]
	(const string_view &, const json::object &object)
	{
		for(const auto &[user_id, room_ids] : object)
		{
			if(user_id != other_person)
			{
				json::stack::member
				{
					top, user_id, json::array(room_ids)
				};

				continue;
			}

			json::stack::array user_rooms
			{
				top, user_id
			};

			user_rooms.append(room.room_id);
			for(const auto &room_id : json::array(room_ids))
				user_rooms.append(room_id);
		}
	});

	top.~object();
	account_data.set("m.direct", json::object(out.completed()));
}
