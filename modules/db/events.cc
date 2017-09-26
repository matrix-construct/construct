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

const database::descriptor events_event_id_descriptor
{
	// name
	"event_id",

	// explanation
	R"(### protocol note:

	10.1
	The id of event.

	10.4
	MUST NOT exceed 255 bytes.

	### developer note:
	key is event_id. This is redundant data but we have to have it for now.
	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	}
};

const database::descriptor events_type_descriptor
{
	// name
	"type",

	// explanation
	R"(### protocol note:

	10.1
	The type of event. This SHOULD be namespaced similar to Java package naming conventions
	e.g. 'com.example.subdomain.event.type'.

	10.4
	MUST NOT exceed 255 bytes.

	### developer note:
	key is event_id
	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	}
};

const database::descriptor events_content_descriptor
{
	// name
	"content",

	// explanation
	R"(### protocol note:

	10.1
	The fields in this object will vary depending on the type of event. When interacting
	with the REST API, this is the HTTP body.

	### developer note:
	Since events must not exceed 65 KB the maximum size for the content is the remaining
	space after all the other fields for the event are rendered.

	key is event_id
	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	}
};

const database::descriptor events_room_id_descriptor
{
	// name
	"room_id",

	// explanation
	R"(### protocol note:

	10.2 (apropos room events)
	Required. The ID of the room associated with this event.

	10.4
	MUST NOT exceed 255 bytes.

	### developer note:
	key is event_id
	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	}
};

const database::descriptor events_sender_descriptor
{
	// name
	"sender",

	// explanation
	R"(### protocol note:

	10.2 (apropos room events)
	Required. Contains the fully-qualified ID of the user who sent this event.

	10.4
	MUST NOT exceed 255 bytes.

	### developer note:
	key is event_id
	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	}
};

const database::descriptor events_state_key_descriptor
{
	// name
	"state_key",

	// explanation
	R"(### protocol note:

	10.3 (apropos room state events)
	A unique key which defines the overwriting semantics for this piece of room state.
	This value is often a zero-length string. The presence of this key makes this event a
	State Event. The key MUST NOT start with '_'.

	10.4
	MUST NOT exceed 255 bytes.

	### developer note:
	key is event_id
	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	}
};

const database::descriptor events_origin_descriptor
{
	// name
	"origin",

	// explanation
	R"(### protocol note:

	FEDERATION 4.1
	DNS name of homeserver that created this PDU

	### developer note:
	key is event_id
	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	}
};

const database::descriptor events_origin_server_ts_descriptor
{
	// name
	"origin_server_ts",

	// explanation
	R"(### protocol note:

	FEDERATION 4.1
	Timestamp in milliseconds on origin homeserver when this PDU was created.

	### developer note:
	key is event_id
	value is a machine integer (binary)

	TODO: consider unsigned rather than time_t because of millisecond precision

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(time_t)
	}
};

const database::descriptor events_prev_ids_descriptor
{
	// name
	"prev_ids",

	// explanation
	R"(### protocol note:

	FEDERATION 4.1 (INCONSISTENT)
	List of (String, String, Object) Triplets
	The originating homeserver, PDU ids and hashes of the most recent PDUs the homeserver was
	aware of for the room when it made this PDU. ["blue.example.com","99d16afbc8", {"sha256":
	"abase64encodedsha256hashshouldbe43byteslong"}]

	### developer note:
	key is event_id

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	}
};

const database::descriptor events_unsigned_descriptor
{
	// name
	"unsigned",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	}
};

const database::descriptor events_signatures_descriptor
{
	// name
	"signatures",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	}
};

/// prefix transform for event_id suffixes
///
/// This transform expects a concatenation ending with an event_id which means
/// the prefix can be the same for multiple event_id's; therefor we can find
/// or iterate "event_id in X" where X is some key like a room_id
///
const ircd::db::prefix_transform event_id_in
{
	"event_id in",
	[](const string_view &key)
	{
		return key.find('$') != key.npos;
	},
	[](const string_view &key)
	{
		return rsplit(key, '$').first;
	}
};

const database::descriptor event_id_in_sender
{
	// name
	"event_id in sender",

	// explanation
	R"(### developer note:

	key is "@sender$event_id"
	the prefix transform is in effect. this column indexes events by
	sender offering an iterable bound of the index prefixed by sender

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	event_id_in,
};

const database::descriptor event_id_in_room_id
{
	// name
	"event_id in room_id",

	// explanation
	R"(### developer note:

	key is "!room_id$event_id"
	the prefix transform is in effect. this column indexes events by
	room_id offering an iterable bound of the index prefixed by room_id

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	},

	// options
	{},

	// comparator - sorts from highest to lowest
	ircd::db::reverse_cmp_string_view{},

	// prefix transform
	event_id_in,
};

/// prefix transform for room_id
///
/// This transform expects a concatenation ending with a room_id which means
/// the prefix can be the same for multiple room_id's; therefor we can find
/// or iterate "room_id in X" where X is some repeated prefix
///
const ircd::db::prefix_transform room_id_in
{
	"room_id in",
	[](const string_view &key)
	{
		return key.find('!') != key.npos;
	},
	[](const string_view &key)
	{
		return rsplit(key, '!').first;
	}
};

const database::descriptor event_id_for_room_id_in_type
{
	// name
	"event_id for room_id in type",

	// explanation
	R"(### developer note:

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	room_id_in,
};

const database::descriptor event_id_for_room_id_in_sender
{
	// name
	"event_id for room_id in sender",

	// explanation
	R"(### developer note:

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	room_id_in,
};

/// prefix transform for type,state_key in room_id
///
/// This transform is special for concatenating room_id with type and state_key
/// in that order with prefix being the room_id (this may change to room_id+
/// type
///
const ircd::db::prefix_transform type_state_key_in_room_id
{
	"type,state_key in room_id",
	[](const string_view &key)
	{
		return key.find("..") != key.npos;
	},
	[](const string_view &key)
	{
		return split(key, "..").first;
	}
};

const database::descriptor event_id_for_type_state_key_in_room_id
{
	// name
	"event_id for type,state_key in room_id",

	// explanation
	R"(### developer note:

	)",

	// typing (key, value)
	{
		typeid(string_view), typeid(string_view)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	type_state_key_in_room_id
};

const database::description events_description
{
	{ "default" },
	events_event_id_descriptor,
	events_type_descriptor,
	events_content_descriptor,
	events_room_id_descriptor,
	events_sender_descriptor,
	events_state_key_descriptor,
	events_origin_descriptor,
	events_origin_server_ts_descriptor,
	events_prev_ids_descriptor,
	events_unsigned_descriptor,
	events_signatures_descriptor,
	event_id_in_sender,
	event_id_in_room_id,
	event_id_for_room_id_in_type,
	event_id_for_room_id_in_sender,
	event_id_for_type_state_key_in_room_id,
};

std::shared_ptr<database> events_database
{
	std::make_shared<database>("events"s, ""s, events_description)
};

mapi::header IRCD_MODULE
{
	"Hosts the 'events' database"
};
