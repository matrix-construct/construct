// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd::m;
using namespace ircd;

mapi::header
IRCD_MODULE
{
	"Client 7.1.1 :Create Room"
};

namespace ircd::m::name
{
    constexpr const auto visibility {"visibility"};
    constexpr const auto room_alias_name {"room_alias_name"};
    constexpr const auto name {"name"};
    constexpr const auto topic {"topic"};
    constexpr const auto invite {"invite"};
    constexpr const auto invite_3pid {"invite_3pid"};
    constexpr const auto creation_content {"creation_content"};
    constexpr const auto initial_state {"initial_state"};
    constexpr const auto preset {"preset"};
    constexpr const auto is_direct {"is_direct"};
    constexpr const auto guest_can_join {"guest_can_join"};

    constexpr const auto id_server {"id_server"};
    constexpr const auto medium {"medium"};
    constexpr const auto address {"address"};
}

struct invite_3pid
:json::tuple
<
	/// Required. The hostname+port of the identity server which should be
	/// used for third party identifier lookups.
	json::property<name::id_server, json::string>,

	/// Required. The kind of address being passed in the address field,
	/// for example email.
	json::property<name::medium, json::string>,

	/// Required. The invitee's third party identifier.
	json::property<name::address, json::string>
>
{
	using super_type::tuple;
};

struct body
:json::tuple
<
	/// A public visibility indicates that the room will be shown in the
	/// published room list. A private visibility will hide the room from
	/// the published room list. Rooms default to private visibility if this
	/// key is not included. NB: This should not be confused with join_rules
	/// which also uses the word public. One of: ["public", "private"]
	json::property<name::visibility, json::string>,

	/// The desired room alias local part. If this is included, a room alias
	/// will be created and mapped to the newly created room. The alias will
	/// belong on the same homeserver which created the room. For example, if
	/// this was set to "foo" and sent to the homeserver "example.com" the
	/// complete room alias would be #foo:example.com.
	json::property<name::room_alias_name, json::string>,

	/// If this is included, an m.room.name event will be sent into the room
	/// to indicate the name of the room. See Room Events for more information
	/// on m.room.name.
	json::property<name::name, json::string>,

	/// If this is included, an m.room.topic event will be sent into the room to
	/// indicate the topic for the room. See Room Events for more information on
	/// m.room.topic.
	json::property<name::topic, json::string>,

	/// A list of user IDs to invite to the room. This will tell the server
	/// to invite everyone in the list to the newly created room.
	json::property<name::invite, json::array>,

	/// A list of objects representing third party IDs to invite into the room.
	json::property<name::invite_3pid, invite_3pid>,

	/// Extra keys to be added to the content of the m.room.create. The server
	/// will clobber the following keys: creator. Future versions of the
	/// specification may allow the server to clobber other keys.
	json::property<name::creation_content, json::object>,

	/// A list of state events to set in the new room. This allows the user
	/// to override the default state events set in the new room. The expected
	/// format of the state events are an object with type, state_key and content
	/// keys set. Takes precedence over events set by presets, but gets overriden
	/// by name and topic keys.
	json::property<name::initial_state, json::array>,

	/// Convenience parameter for setting various default state events based on
	/// a preset. Must be either: private_chat => join_rules is set to invite.
	/// history_visibility is set to shared. trusted_private_chat => join_rules
	/// is set to invite. history_visibility is set to shared. All invitees are
	/// given the same power level as the room creator. public_chat: =>
	/// join_rules is set to public. history_visibility is set to shared. One
	/// of: ["private_chat", "public_chat", "trusted_private_chat"]
	json::property<name::preset, json::string>,

	/// This flag makes the server set the is_direct flag on the m.room.member
	/// events sent to the users in invite and invite_3pid. See Direct
	/// Messaging for more information.
	json::property<name::is_direct, bool>,

	/// Allows guests to join the room. See Guest Access for more information.
	///
	/// developer note: this is false if undefined, but an m.room.guest_access
	/// may be present in the initial vector which allows guest access. This is
	/// only meaningful if and only if true.
	json::property<name::guest_can_join, bool>
>
{
	using super_type::tuple;
};

extern "C" room
createroom__parent_type(const id::room &room_id,
                        const id::user &creator,
                        const id::room &parent,
                        const string_view &type);

extern "C" room
createroom__type(const id::room &room_id,
                 const id::user &creator,
                 const string_view &type);

extern "C" room
createroom(const id::room &room_id,
           const id::user &creator);

const room::id::buf
init_room_id
{
	"init", ircd::my_host()
};

resource
createroom_resource
{
	"/_matrix/client/r0/createRoom",
	{
		"(7.1.1) Create a new room with various configuration options."
	}
};

resource::response
post__createroom(client &client,
                 const resource::request::object<body> &request)
try
{
	const id::user &sender_id
	{
		request.user_id
	};

	const id::room::buf room_id
	{
		id::generate, my_host()
	};

	const room room
	{
		createroom(room_id, sender_id)
	};

	const event::id::buf join_event_id
	{
		join(room, sender_id)
	};

	return resource::response
	{
		client, http::CREATED,
		{
			{ "room_id", room_id },
		}
	};
}
catch(const db::not_found &e)
{
	throw m::error
	{
		http::CONFLICT, "M_ROOM_IN_USE", "The desired room name is in use."
	};

	throw m::error
	{
		http::CONFLICT, "M_ROOM_ALIAS_IN_USE", "An alias of the desired room is in use."
	};
}

resource::method
post_method
{
	createroom_resource, "POST", post__createroom,
	{
		post_method.REQUIRES_AUTH
	}
};

room
createroom(const id::room &room_id,
           const id::user &creator)
{
	return createroom__type(room_id, creator, string_view{});
}

room
createroom__type(const id::room &room_id,
                 const id::user &creator,
                 const string_view &type)
{
	return createroom__parent_type(room_id, creator, init_room_id, type);
}

room
createroom__parent_type(const id::room &room_id,
                        const id::user &creator,
                        const id::room &parent,
                        const string_view &type)
{
	json::iov event;
	json::iov content;
	const json::iov::push push[]
	{
		{ event,     { "sender",   creator  }},
		{ content,   { "creator",  creator  }},
	};

	const json::iov::add_if _parent
	{
		content, !parent.empty() && parent.local() != "init",
		{
			"parent", parent
		}
	};

	const json::iov::add_if _type
	{
		content, !type.empty() && type != "room",
		{
			"type", type
		}
	};

	const json::iov::push _push[]
	{
		{ event,  { "depth",       0L               }},
		{ event,  { "type",        "m.room.create"  }},
		{ event,  { "state_key",   ""               }},
	};

	room room
	{
		room_id
	};

	commit(room, event, content);
	return room;
}
