// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_CREATEROOM_H

namespace ircd::m
{
	struct createroom;
}

struct ircd::m::createroom
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
	json::property<name::name_, json::string>,

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
	json::property<name::guest_can_join, bool>,

	/// The power level content to override in the default power level event.
	/// This object is applied on top of the generated m.room.power_levels
	/// event content prior to it being sent to the room. Defaults to
	/// overriding nothing.
	json::property<name::power_level_content_override, json::object>
>
{
	using super_type::tuple;
};
