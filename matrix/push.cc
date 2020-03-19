// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::push::rule::type_prefix)
ircd::m::push::rule::type_prefix
{
	"ircd.push.rule"
};

ircd::m::push::path
ircd::m::push::make_path(const event &event)
{
	return make_path(at<"type"_>(event), at<"state_key"_>(event));
}

ircd::m::push::path
ircd::m::push::make_path(const string_view &type,
                         const string_view &state_key)
{
	if(unlikely(!startswith(type, rule::type_prefix)))
		throw NOT_A_RULE
		{
			"Type '%s' does not start with prefix '%s'",
			type,
			rule::type_prefix,
		};

	const auto unprefixed
	{
		lstrip(lstrip(type, rule::type_prefix), '.')
	};

	const auto &[scope, kind]
	{
		split(unprefixed, '.')
	};

	return path
	{
		scope, kind, state_key
	};
}

ircd::string_view
ircd::m::push::make_type(const mutable_buffer &buf,
                         const path &path)
{
	const auto &[scope, kind, ruleid]
	{
		path
	};

	if(!scope)
		return fmt::sprintf
		{
			buf, "%s.",
			rule::type_prefix,
		};

	else if(!kind)
		return fmt::sprintf
		{
			buf, "%s.%s.",
			rule::type_prefix,
			scope,
		};

	return fmt::sprintf
	{
		buf, "%s.%s.%s",
		rule::type_prefix,
		scope,
		kind,
	};
}

decltype(ircd::m::push::rules::defaults)
ircd::m::push::rules::defaults{R"(
{
	"override":
	[
		{
			"rule_id": ".m.rule.master",
			"default": true,
			"enabled": false,
			"conditions": [],
			"actions":
			[
				"dont_notify"
			]
		},
		{
			"rule_id": ".m.rule.suppress_notices",
			"default": true,
			"enabled": true,
			"conditions":
			[
				{
					"kind": "event_match",
					"key": "content.msgtype",
					"pattern": "m.notice"
				}
			],
			"actions":
			[
				"dont_notify"
			]
		},
		{
			"rule_id": ".m.rule.invite_for_me",
			"default": true,
			"enabled": true,
			"conditions":
			[
				{
					"key": "type",
					"kind": "event_match",
					"pattern": "m.room.member"
				},
				{
					"key": "content.membership",
					"kind": "event_match",
					"pattern": "invite"
				},
				{
					"key": "state_key",
					"kind": "event_match",
					"pattern": "[the user's Matrix ID]"
				}
			],
			"actions":
			[
				"notify",
				{
					"set_tweak": "sound",
					"value": "default"
				},
				{
					"set_tweak": "highlight",
					"value": false
				}
			]
		},
		{
			"rule_id": ".m.rule.member_event",
			"default": true,
			"enabled": true,
			"conditions":
			[
				{
					"key": "type",
					"kind": "event_match",
					"pattern": "m.room.member"
				}
			],
			"actions":
			[
				"dont_notify"
			]
		},
		{
			"rule_id": ".m.rule.contains_display_name",
			"default": true,
			"enabled": true,
			"conditions":
			[
				{
					"kind": "contains_display_name"
				}
			],
			"actions":
			[
				"notify",
				{
					"set_tweak": "sound",
					"value": "default"
				},
				{
					"set_tweak": "highlight"
				}
			]
		},
		{
			"rule_id": ".m.rule.tombstone",
			"default": true,
			"enabled": true,
			"conditions":
			[
				{
					"kind": "event_match",
					"key": "type",
					"pattern": "m.room.tombstone"
				},
				{
					"kind": "event_match",
					"key": "state_key",
					"pattern": ""
				}
			],
			"actions":
			[
				"notify",
				{
					"set_tweak": "highlight",
					"value": true
				}
			]
		},
		{
			"rule_id": ".m.rule.roomnotif",
			"default": true,
			"enabled": true,
			"conditions":
			[
				{
					"kind": "event_match",
					"key": "content.body",
					"pattern": "@room"
				},
				{
					"kind": "sender_notification_permission",
					"key": "room"
				}
			],
			"actions":
			[
				"notify",
				{
					"set_tweak": "highlight",
					"value": true
				}
			]
		}
	],
	"underride":
	[
		{
			"rule_id": ".m.rule.call",
			"default": true,
			"enabled": true,
			"conditions":
			[
				{
					"key": "type",
					"kind": "event_match",
					"pattern": "m.call.invite"
				}
			],
			"actions":
			[
				"notify",
				{
					"set_tweak": "sound",
					"value": "ring"
				},
				{
					"set_tweak": "highlight",
					"value": false
				}
			]
		},
		{
			"rule_id": ".m.rule.encrypted_room_one_to_one",
			"default": true,
			"enabled": true,
			"conditions":
			[
				{
					"kind": "room_member_count",
					"is": "2"
				},
				{
					"kind": "event_match",
					"key": "type",
					"pattern": "m.room.encrypted"
				}
			],
			"actions":
			[
				"notify",
				{
					"set_tweak": "sound",
					"value": "default"
				},
				{
					"set_tweak": "highlight",
					"value": false
				}
			]
		},
		{
			"rule_id": ".m.rule.room_one_to_one",
			"default": true,
			"enabled": true,
			"conditions":
			[
				{
					"kind": "room_member_count",
					"is": "2"
				},
				{
					"kind": "event_match",
					"key": "type",
					"pattern": "m.room.message"
				}
			],
			"actions":
			[
				"notify",
				{
					"set_tweak": "sound",
					"value": "default"
				},
				{
					"set_tweak": "highlight",
					"value": false
				}
			]
		},
		{
			"rule_id": ".m.rule.message",
			"default": true,
			"enabled": true,
			"conditions":
			[
				{
					"kind": "event_match",
					"key": "type",
					"pattern": "m.room.message"
				}
			],
			"actions":
			[
				"notify",
				{
					"set_tweak": "highlight",
					"value": false
				}
			]
		},
		{
			"rule_id": ".m.rule.encrypted",
			"default": true,
			"enabled": true,
			"conditions":
			[
				{
					"kind": "event_match",
					"key": "type",
					"pattern": "m.room.encrypted"
				}
			],
			"actions":
			[
				"notify",
				{
					"set_tweak": "highlight",
					"value": false
				}
			]
		}
	]
}
)"};
