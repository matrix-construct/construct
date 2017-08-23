/* 
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

using namespace ircd;

const database::descriptor room_created_descriptor
{
	"created",

	R"(### developer note:
	A UNIX epoch timestamp sampled when the room was created.
	)",
	{
		// readable key      // binary value
		typeid(string_view), typeid(time_t)
	}
};

const database::descriptor room_creator_descriptor
{
	"creator",

	R"(### protocol note:
	The user_id of the room creator. This is set by the homeserver.
	)"
};

const database::descriptor room_topic_descriptor
{
	"topic",

	R"(### protocol note:
	If this is included, an m.room.topic event will be sent into the room to indicate the
	topic for the room. See Room Events for more information on m.room.topic.
	)"
};

const database::descriptor room_visibility_descriptor
{
	"visibility",

	R"(### protocol note:
	Rooms default to private visibility if this key is not included.

	* "public" visibility indicates that the room will be shown in the published room list.

	* "private" visibility will hide the room from the published room list.

	One of: ["public", "private"]
	)"
};

const database::descriptor room_join_rules_descriptor
{
	"join_rules",

	R"(### protocol note:
	A room may be public meaning anyone can join the room without any prior action.
	Alternatively, it can be invite meaning that a user who wishes to join the room must first
	receive an invite to the room from someone already inside of the room. Currently, knock and private
	are reserved keywords which are not implemented.

	The type of rules used for users wishing to join this room.
	One of: ["public", "knock", "invite", "private"]
	)"
};

const database::descriptor room_history_visibility_descriptor
{
	"history_visibility",

	R"(### protocol note:

	* "world_readable" All events while this is the m.room.history_visibility value may be shared
	by any participating homeserver with anyone, regardless of whether they have ever joined the room.

	* "shared" Previous events are always accessible to newly joined members. All events in the room
	are accessible, even those sent when the member was not a part of the room.

	* "invited" Events are accessible to newly joined members from the point they were invited onwards.
	Events stop being accessible when the member's state changes to something other than invite or join.

	* "joined" Events are accessible to newly joined members from the point they joined the room onwards.
	Events stop being accessible when the member's state changes to something other than join.
	)"
};

const database::descriptor room_alias_descriptor
{
	"alias",

	R"(### protocol note:
	The desired room alias local part. If this is included, a room alias will be created and mapped to
	the newly created room. The alias will belong on the same homeserver which created the room.
	For example, if this was set to "foo" and sent to the homeserver "example.com" the complete
	room alias would be #foo:example.com.

	### developer note:
	The alias column on the room's primary row has a comma separated list of aliases.
	For each of those aliases this column has a key indexed for it; the value for that key is
	the primary room's name. This is a cross-reference that must be kept in sync.
	)"
};

const database::descriptor room_federate_descriptor
{
	"federate",

	R"(### protocol note:
	Whether users on other servers can join this room. Defaults to true if key does not exist.
	)",
	{
		// readable key      // binary value
		typeid(string_view), typeid(bool)
	}
};

const database::description room_description
{
	{ "default" },
	room_created_descriptor,
	room_creator_descriptor,
	room_topic_descriptor,
	room_visibility_descriptor,
	room_join_rules_descriptor,
	room_history_visibility_descriptor,
	room_alias_descriptor,
	room_federate_descriptor,
};

std::shared_ptr<database> room_database
{
	std::make_shared<database>("room"s, ""s, room_description)
};

extern database *const room
{
	room_database.get()
};

struct room
:resource
{
	using resource::resource;
}
room_resource
{
	"_matrix/client/r0/room",
	"Rooms (7.0)"
};

mapi::header IRCD_MODULE
{
	"registers the resource 'client/room' to manage Matrix rooms"
};
