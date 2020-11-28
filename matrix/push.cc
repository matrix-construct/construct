// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::push::log)
ircd::m::push::log
{
	"m.push"
};

decltype(ircd::m::push::pusher::type_prefix)
ircd::m::push::pusher::type_prefix
{
	"ircd.push.pusher"
};

decltype(ircd::m::push::rule::type_prefix)
ircd::m::push::rule::type_prefix
{
	"ircd.push.rule"
};

//
// request
//

template<>
decltype(ircd::util::instance_list<ircd::m::push::request>::allocator)
ircd::util::instance_list<ircd::m::push::request>::allocator
{};

template<>
decltype(ircd::util::instance_list<ircd::m::push::request>::list)
ircd::util::instance_list<ircd::m::push::request>::list
{
	allocator
};

decltype(ircd::m::push::request::enable)
ircd::m::push::request::enable
{
	{ "name",     "ircd.m.push.request.enable" },
	{ "default",  true                         },
};

decltype(ircd::m::push::request::timeout)
ircd::m::push::request::timeout
{
	{ "name",     "ircd.m.push.request.timeout" },
	{ "default",  8L                            },
};

decltype(ircd::m::push::request::mutex)
ircd::m::push::request::mutex;

decltype(ircd::m::push::request::dock)
ircd::m::push::request::dock;

decltype(ircd::m::push::request::id_ctr)
ircd::m::push::request::id_ctr;

//
// match
//

namespace ircd::m::push
{
	static bool unknown_condition_kind(const event &, const cond &, const match::opts &);
	static bool sender_notification_permission(const event &, const cond &, const match::opts &);
	static bool contains_display_name(const event &, const cond &, const match::opts &);
	static bool state_key_user_mxid(const event &, const cond &, const match::opts &);
	static bool contains_user_mxid(const event &, const cond &, const match::opts &);
	static bool room_member_count(const event &, const cond &, const match::opts &);
	static bool event_match(const event &, const cond &, const match::opts &);
}

decltype(ircd::m::push::match::cond_kind)
ircd::m::push::match::cond_kind
{
	event_match,
	room_member_count,
	contains_user_mxid,
	state_key_user_mxid,
	contains_display_name,
	sender_notification_permission,
	unknown_condition_kind,
};

decltype(ircd::m::push::match::cond_kind_name)
ircd::m::push::match::cond_kind_name
{
	"event_match",
	"room_member_count",
	"contains_user_mxid",
	"state_key_user_mxid",
	"contains_display_name",
	"sender_notification_permission",
};

//
// match::match
//

ircd::m::push::match::match(const event &event,
                            const rule &rule,
                            const match::opts &opts)
:boolean{[&event, &rule, &opts]
{
	if(json::get<"pattern"_>(rule))
	{
		const push::cond cond
		{
			{ "kind",     "event_match"               },
			{ "key",      "content.body"              },
			{ "pattern",  json::get<"pattern"_>(rule) },
		};

		if(!match(event, cond, opts))
			return false;
	}

	for(const json::object cond : json::get<"conditions"_>(rule))
		if(!match(event, push::cond(cond), opts))
			return false;

	return true;
}}
{
}

ircd::m::push::match::match(const event &event,
                            const cond &cond,
                            const match::opts &opts)
:boolean{[&event, &cond, &opts]
{
	const string_view &kind
	{
		json::get<"kind"_>(cond)
	};

	const auto pos
	{
		indexof(kind, string_views(cond_kind_name))
	};

	assert(pos <= sizeof(cond_kind_name) / sizeof(string_view));
	const auto &func
	{
		cond_kind[pos]
	};

	return func(event, cond, opts);
}}
{
}

//
// push::match condition functors (internal)
//

bool
ircd::m::push::event_match(const event &event,
                           const cond &cond,
                           const match::opts &opts)
try
{
	assert(json::get<"kind"_>(cond) == "event_match");

	const auto &[top, path]
	{
		split(json::get<"key"_>(cond), '.')
	};

	string_view value
	{
		json::get(event, top, json::object{})
	};

	tokens(path, ".", [&value]
	(const string_view &key)
	{
		if(!json::type(value, json::OBJECT))
			return false;

		value = json::object(value)[key];
		if(likely(!json::type(value, json::STRING)))
			return true;

		value = json::string(value);
		return false;
	});

	 //TODO: XXX spec leading/trailing; not imatch
	const globular_imatch pattern
	{
		json::get<"pattern"_>(cond)
	};

	return pattern(value);
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Push condition 'event_match' %s :%s",
		string_view{event.event_id},
		e.what(),
	};

	return false;
}

bool
ircd::m::push::contains_user_mxid(const event &event,
                                  const cond &cond,
                                  const match::opts &opts)
try
{
	assert(json::get<"kind"_>(cond) == "contains_user_mxid");

	assert(opts.user_id);
	if(unlikely(!opts.user_id))
		return false;

	const json::object &content
	{
		json::get<"content"_>(event)
	};

	const json::string &body
	{
		content["body"]
	};

	if(has(body, opts.user_id))
		return true;

	const json::string &formatted_body
	{
		content["formatted_body"]
	};

	if(has(formatted_body, opts.user_id))
		return true;

	return false;
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Push condition 'contains_user_mxid' %s :%s",
		string_view{event.event_id},
		e.what(),
	};

	return false;
}

bool
ircd::m::push::state_key_user_mxid(const event &event,
                                   const cond &cond,
                                   const match::opts &opts)
try
{
	assert(json::get<"kind"_>(cond) == "state_key_user_mxid");

	const auto &state_key
	{
		json::get<"state_key"_>(event)
	};

	assert(opts.user_id);
	return state_key == opts.user_id;
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Push condition 'state_key_user_mxid' %s :%s",
		string_view{event.event_id},
		e.what(),
	};

	return false;
}

bool
ircd::m::push::contains_display_name(const event &event,
                                     const cond &cond,
                                     const match::opts &opts)
try
{
	assert(json::get<"kind"_>(cond) == "contains_display_name");

	const json::object &content
	{
		json::get<"content"_>(event)
	};

	const json::string &body
	{
		content["body"]
	};

	if(!body)
		return false;

	assert(opts.user_id);
	if(unlikely(!opts.user_id))
		return false;

	const m::user::profile profile
	{
		opts.user_id
	};

	bool ret{false};
	profile.get(std::nothrow, "displayname", [&ret, &body]
	(const string_view &, const json::string &displayname)
	{
		ret = displayname && has(body, displayname);
	});

	return ret;
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Push condition 'contains_display_name' %s :%s",
		string_view{event.event_id},
		e.what(),
	};

	return false;
}

bool
ircd::m::push::sender_notification_permission(const event &event,
                                              const cond &cond,
                                              const match::opts &opts)
try
{
	assert(json::get<"kind"_>(cond) == "sender_notification_permission");

	const auto &key
	{
		json::get<"key"_>(cond)
	};

	const m::room room
	{
		at<"room_id"_>(event)
	};

	const room::power power
	{
		room
	};

	const auto &user_level
	{
		power.level_user(at<"sender"_>(event))
	};

	int64_t required_level
	{
		room::power::default_power_level
	};

	power.for_each("notifications", [&required_level, &key]
	(const auto &name, const auto &level)
	{
		if(name == key)
		{
			required_level = level;
			return false;
		}
		else return true;
	});

	const bool ret
	{
		user_level >= required_level
	};

	if(!ret)
		log::dwarning
		{
			log, "Insufficient power level %ld for %s to notify '%s' to %s (require:%ld).",
			user_level,
			string_view{json::get<"sender"_>(event)},
			key,
			string_view{room.room_id},
			required_level
		};

	return ret;
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Push condition 'sender_notification_permission' %s :%s",
		string_view{event.event_id},
		e.what(),
	};

	return false;
}

bool
ircd::m::push::room_member_count(const event &event,
                                 const cond &cond,
                                 const match::opts &opts)
try
{
	assert(json::get<"kind"_>(cond) == "room_member_count");

	const m::room room
	{
		json::get<"room_id"_>(event)
	};

	const m::room::members members
	{
		room
	};

	const auto &is
	{
		json::get<"is"_>(cond)
	};

	string_view valstr(is);
	while(valstr && !all_of<std::isdigit>(valstr))
		valstr.pop_front();

	const ulong val
	{
		lex_cast<ulong>(valstr)
	};

	const string_view op
	{
		begin(is), begin(valstr)
	};

	switch(hash(op))
	{
		default:
		case "=="_:
			return
				val?
					members.count("join") == val:
					members.empty("join");

		case ">="_:
			return
				val?
					members.count("join") >= val:
					true;

		case "<="_:
			return
				val?
					members.count("join") <= val:
					members.empty("join");

		case ">"_:
			return
				val?
					members.count("join") > val:
					!members.empty("join");

		case "<"_:
			return
				val > 1?
					members.count("join") < val:
				val == 1?
					members.empty("join"):
					false;
	};
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Push condition 'room_member_count' %s :%s",
		string_view{event.event_id},
		e.what(),
	};

	return false;
}

bool
ircd::m::push::unknown_condition_kind(const event &event,
                                      const cond &cond,
                                      const match::opts &opts)
{
	const string_view &kind
	{
		json::get<"kind"_>(cond)
	};

	log::derror
	{
		log, "Push condition for %s by %s :unknown kind '%s' rule always fails...",
		string_view{event.event_id},
		string_view{opts.user_id},
		kind,
	};

	return false;
}

//
// rule
//

bool
ircd::m::push::rule::for_each(const path &path,
                              const closure_bool &closure)
{
	using json::at;

	char typebuf[event::TYPE_MAX_SIZE];
	const string_view &type
	{
		make_type(typebuf, path)
	};

	return events::type::for_each_in(type, [&closure]
	(const string_view &type, const event::idx &event_idx)
	{
		static const event::fetch::opts fopts
		{
			event::keys::include
			{
				"content",
				"room_id",
				"sender",
				"state_key",
			}
		};

		if(!m::room::state::present(event_idx))
			return true;

		const m::event::fetch event
		{
			std::nothrow, event_idx, fopts
		};

		if(!event.valid)
			return true;

		const m::user::id &sender
		{
			at<"sender"_>(event)
		};

		const m::room::id &room_id
		{
			at<"room_id"_>(event)
		};

		if(!my(sender) || !m::user::room::is(room_id, sender))
			return true;

		const push::path path
		{
			make_path(type, at<"state_key"_>(event))
		};

		return closure(at<"sender"_>(event), path, at<"content"_>(event));
	});
}

bool
ircd::m::push::notifying(const rule &rule)
{
	const json::array &actions
	{
		json::get<"actions"_>(rule)
	};

	for(const string_view &action : actions)
	{
		if(!json::type(action, json::STRING))
			continue;

		const json::string &string
		{
			action
		};

		if(string == "notify")
			return true;

		if(string == "coalesce")
			return true;
	}

	return false;
}

bool
ircd::m::push::highlighting(const rule &rule)
{
	const json::array &actions
	{
		json::get<"actions"_>(rule)
	};

	for(const string_view &action : actions)
	{
		if(!json::type(action, json::OBJECT))
			continue;

		const json::object &object
		{
			action
		};

		const json::string set_tweak
		{
			object["set_tweak"]
		};

		if(set_tweak != "highlight")
			continue;

		const auto &value
		{
			object["value"]
		};

		// Spec sez: If a highlight tweak is given with no value, its value is
		// defined to be true. If no highlight tweak is given at all then the
		// value of highlight is defined to be false.
		return !value || value == "true";
	}

	return false;
}

//
// path
//

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
					"kind": "state_key_user_mxid"
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
		},
		{
			"rule_id": ".m.rule.reaction",
			"default": true,
			"enabled": true,
			"conditions":
			[
				{
					"kind": "event_match",
					"key": "type",
					"pattern": "m.reaction"
				}
			],
			"actions":
			[
				"dont_notify"
			]
		}
	],
	"content":
	[
		{
			"rule_id": ".m.rule.contains_user_name",
			"default": true,
			"enabled": true,
			"conditions":
			[
				{
					"kind": "contains_user_mxid"
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
					"kind": "event_match",
					"key": "type",
					"pattern": "m.room.encrypted"
				},
				{
					"kind": "room_member_count",
					"is": "2"
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
					"kind": "event_match",
					"key": "type",
					"pattern": "m.room.message"
				},
				{
					"kind": "room_member_count",
					"is": "2"
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
			"enabled": false,
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
