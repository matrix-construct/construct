// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::mapi::header
IRCD_MODULE
{
	"Matrix event library; modular components."
};

namespace ircd::m::vm
{
	extern const m::hookfn<eval &> issue_missing_auth;
	extern const m::hookfn<eval &> conform_check_origin;
	extern const m::hookfn<eval &> conform_check_size;
	extern const m::hookfn<eval &> conform_report;
}

/// Check if an eval with a copts structure (indicating this server is
/// creating the event) has an origin set to !my_host().
decltype(ircd::m::vm::conform_check_origin)
ircd::m::vm::conform_check_origin
{
	{
		{ "_site", "vm.conform" }
	},
	[](const m::event &event, eval &eval)
	{
		if(unlikely(eval.copts && !my_host(at<"origin"_>(event))))
			throw error
			{
				fault::GENERAL, "Issuing event for origin: %s", at<"origin"_>(event)
			};
	}
};

/// Check if an event originating from this server exceeds maximum size.
decltype(ircd::m::vm::conform_check_size)
ircd::m::vm::conform_check_size
{
	{
		{ "_site",  "vm.conform"  },
		{ "origin",  my_host()    }
	},
	[](const m::event &event, eval &eval)
	{
		const size_t &event_size
		{
			serialized(event)
		};

		if(event_size > size_t(event::max_size))
			throw m::BAD_JSON
			{
				"Event is %zu bytes which is larger than the maximum %zu bytes",
				event_size,
				size_t(event::max_size)
			};
	}
};

/// Check if an event originating from this server exceeds maximum size.
decltype(ircd::m::vm::conform_report)
ircd::m::vm::conform_report
{
	{
		{ "_site",  "vm.conform"  }
	},
	[](const m::event &event, eval &eval)
	{
		assert(eval.opts);
		auto &opts(*eval.opts);

		// When opts.conformed is set the report is already generated
		if(opts.conformed)
		{
			eval.report = opts.report;
			return;
		}

		// Generate the report here.
		eval.report = event::conforms
		{
			event, opts.non_conform.report
		};

		// When opts.conforming is false a bad report is not an error.
		if(!opts.conforming)
			return;

		// Otherwise this will kill the eval
		if(!eval.report.clean())
			throw error
			{
				fault::INVALID, "Non-conforming event: %s", string(eval.report)
			};
	}
};

IRCD_MODULE_EXPORT
std::string
ircd::m::pretty(const event &event)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 4096);
	pretty(s, event);
	resizebuf(s, ret);
	return ret;
}

IRCD_MODULE_EXPORT
std::ostream &
ircd::m::pretty(std::ostream &s,
                const event &event)
{
	const auto out{[&s]
	(const string_view &key, auto&& val)
	{
		if(defined(json::value(val)))
			s << std::setw(16) << std::right << key << " :" << val << std::endl;
	}};

	const string_view top_keys[]
	{
		"origin",
		"event_id",
		"room_id",
		"sender",
		"type",
		"depth",
		"state_key",
		"redacts",
	};

	json::for_each(event, top_keys, out);

	const auto &ts{json::get<"origin_server_ts"_>(event)};
	{
		thread_local char buf[128];
		s << std::setw(16) << std::right << "origin_server_ts" << " :"
		  << timef(buf, ts / 1000L, ircd::localtime)
		  << " (" << ts << ")"
		  << std::endl;
	}

	const json::object &contents{json::get<"content"_>(event)};
	if(!contents.empty())
		s << std::setw(16) << std::right << "content" << " :"
		  << size(contents) << " keys; "
		  << size(string_view{contents}) << " bytes."
		  << std::endl;

	const auto &hashes{json::get<"hashes"_>(event)};
	for(const auto &hash : hashes)
	{
		s << std::setw(16) << std::right << "[hash]" << " :"
		  << hash.first
		  << " "
		  << unquote(hash.second)
		  << std::endl;
	}

	const auto &signatures{json::get<"signatures"_>(event)};
	for(const auto &signature : signatures)
	{
		s << std::setw(16) << std::right << "[signature]" << " :"
		  << signature.first << " ";

		for(const auto &key : json::object{signature.second})
			s << key.first << " ";

		s << std::endl;
	}

	const auto &auth_events{json::get<"auth_events"_>(event)};
	for(const json::array auth_event : auth_events)
	{
		s << std::setw(16) << std::right << "[auth event]"
		  << " :" << unquote(auth_event[0]);

		for(const auto &hash : json::object{auth_event[1]})
			s << " " << unquote(hash.first)
			  << ": " << unquote(hash.second);

		s << std::endl;
	}

	const auto &prev_states{json::get<"prev_state"_>(event)};
	for(const json::array prev_state : prev_states)
	{
		s << std::setw(16) << std::right << "[prev state]"
		  << " :" << unquote(prev_state[0]);

		for(const auto &hash : json::object{prev_state[1]})
			s << " " << unquote(hash.first)
			  << ": " << unquote(hash.second);

		s << std::endl;
	}

	const auto &prev_events{json::get<"prev_events"_>(event)};
	for(const json::array prev_event : prev_events)
	{
		s << std::setw(16) << std::right << "[prev_event]"
		  << " :" << unquote(prev_event[0]);

		for(const auto &hash : json::object{prev_event[1]})
			s << " " << unquote(hash.first)
			  << ": " << unquote(hash.second);

		s << std::endl;
	}

	if(!contents.empty())
		for(const json::object::member &content : contents)
			s << std::setw(16) << std::right << "[content]" << " :"
			  << std::setw(7) << std::left << reflect(json::type(content.second)) << " "
			  << std::setw(5) << std::right << size(string_view{content.second}) << " bytes "
			  << ':' << content.first
			  << std::endl;

	return s;
}

IRCD_MODULE_EXPORT
std::string
ircd::m::pretty_oneline(const event &event,
                        const bool &content_keys)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 4096);
	pretty_oneline(s, event, content_keys);
	resizebuf(s, ret);
	return ret;
}

IRCD_MODULE_EXPORT
std::ostream &
ircd::m::pretty_oneline(std::ostream &s,
                        const event &event,
                        const bool &content_keys)
{
	if(defined(json::get<"room_id"_>(event)))
		s << json::get<"room_id"_>(event) << " ";
	else
		s << "* ";

	if(json::get<"depth"_>(event) != json::undefined_number)
		s << json::get<"depth"_>(event) << " ";
	else
		s << "* ";

	thread_local char sdbuf[48];
	if(json::get<"origin_server_ts"_>(event) != json::undefined_number)
		s << smalldate(sdbuf, json::get<"origin_server_ts"_>(event) / 1000L) << " ";
	else
		s << "* ";

	if(defined(json::get<"origin"_>(event)))
		s << ':' << json::get<"origin"_>(event) << " ";
	else
		s << ":* ";

	if(defined(json::get<"sender"_>(event)))
		s << json::get<"sender"_>(event) << " ";
	else
		s << "* ";

	if(defined(json::get<"event_id"_>(event)))
		s << json::get<"event_id"_>(event) << " ";
	else
		s << "* ";

	const auto &auth_events{json::get<"auth_events"_>(event)};
	s << "A:" << auth_events.count() << " ";

	const auto &prev_states{json::get<"prev_state"_>(event)};
	s << "S:" << prev_states.count() << " ";

	const auto &prev_events{json::get<"prev_events"_>(event)};
	s << "E:" << prev_events.count() << " ";

	const auto &hashes{json::get<"hashes"_>(event)};
	s << "[ ";
	for(const auto &hash : hashes)
		s << hash.first << " ";
	s << "] ";

	const auto &signatures{json::get<"signatures"_>(event)};
	s << "[ ";
	for(const auto &signature : signatures)
	{
		s << signature.first << "[ ";
		for(const auto &key : json::object{signature.second})
			s << key.first << " ";

		s << "] ";
	}
	s << "] ";

	if(defined(json::get<"type"_>(event)))
		s << json::get<"type"_>(event) << " ";
	else
		s << "* ";

	const auto &state_key
	{
		json::get<"state_key"_>(event)
	};

	if(defined(state_key) && empty(state_key))
		s << "\"\"" << " ";
	else if(defined(state_key))
		s << state_key << " ";
	else
		s << "*" << " ";

	const string_view &membership
	{
		json::get<"type"_>(event) == "m.room.member"?
			m::membership(event):
			"*"_sv
	};

	s << membership << " ";

	if(defined(json::get<"redacts"_>(event)))
		s << json::get<"redacts"_>(event) << " ";
	else
		s << "* ";

	const json::object &contents
	{
		content_keys?
			json::get<"content"_>(event):
			json::object{}
	};

	if(!contents.empty())
	{
		s << "+" << string_view{contents}.size() << " bytes :";
		for(const auto &content : contents)
			s << content.first << " ";
	}

	return s;
}

IRCD_MODULE_EXPORT
std::string
ircd::m::pretty_msgline(const event &event)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 4096);
	pretty_msgline(s, event);
	resizebuf(s, ret);
	return ret;
}

IRCD_MODULE_EXPORT
std::ostream &
ircd::m::pretty_msgline(std::ostream &s,
                        const event &event)
{
	s << json::get<"depth"_>(event) << " :";
	s << json::get<"type"_>(event) << " ";
	s << json::get<"sender"_>(event) << " ";
	s << json::get<"event_id"_>(event) << " ";

	const auto &state_key
	{
		json::get<"state_key"_>(event)
	};

	if(defined(state_key) && empty(state_key))
		s << "\"\"" << " ";
	else if(defined(state_key))
		s << state_key << " ";
	else
		s << "*" << " ";

	const json::object &content
	{
		json::get<"content"_>(event)
	};

	switch(hash(json::get<"type"_>(event)))
	{
		case "m.room.message"_:
			s << unquote(content.get("msgtype")) << " ";
			s << unquote(content.get("body")) << " ";
			break;

		default:
			s << string_view{content};
			break;
	}

	return s;
}

IRCD_MODULE_EXPORT
std::string
ircd::m::pretty(const event::prev &prev)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 4096);
	pretty(s, prev);
	resizebuf(s, ret);
	return ret;
}

IRCD_MODULE_EXPORT
std::ostream &
ircd::m::pretty(std::ostream &s,
                const event::prev &prev)
{
	const auto out{[&s]
	(const string_view &key, auto&& val)
	{
		if(json::defined(val))
			s << key << " :" << val << std::endl;
	}};

	const auto &auth_events{json::get<"auth_events"_>(prev)};
	for(const json::array auth_event : auth_events)
	{
		s << std::setw(16) << std::right << "[auth event]"
		  << " :" << unquote(auth_event[0]);

		for(const auto &hash : json::object{auth_event[1]})
			s << " " << unquote(hash.first)
			  << ": " << unquote(hash.second);

		s << std::endl;
	}

	const auto &prev_states{json::get<"prev_state"_>(prev)};
	for(const json::array prev_state : prev_states)
	{
		s << std::setw(16) << std::right << "[prev state]"
		  << " :" << unquote(prev_state[0]);

		for(const auto &hash : json::object{prev_state[1]})
			s << " " << unquote(hash.first)
			  << ": " << unquote(hash.second);

		s << std::endl;
	}

	const auto &prev_events{json::get<"prev_events"_>(prev)};
	for(const json::array prev_event : prev_events)
	{
		s << std::setw(16) << std::right << "[prev_event]"
		  << " :" << unquote(prev_event[0]);

		for(const auto &hash : json::object{prev_event[1]})
			s << " " << unquote(hash.first)
			  << ": " << unquote(hash.second);

		s << std::endl;
	}

	return s;
}

IRCD_MODULE_EXPORT
std::ostream &
ircd::m::pretty_oneline(std::ostream &s,
                        const event::prev &prev)
{
	const auto &auth_events{json::get<"auth_events"_>(prev)};
	s << "A[ ";
	for(const json::array auth_event : auth_events)
		s << unquote(auth_event[0]) << " ";
	s << "] ";

	const auto &prev_states{json::get<"prev_state"_>(prev)};
	s << "S[ ";
	for(const json::array prev_state : prev_states)
		s << unquote(prev_state[0]) << " ";
	s << "] ";

	const auto &prev_events{json::get<"prev_events"_>(prev)};
	s << "E[ ";
	for(const json::array prev_event : prev_events)
		s << unquote(prev_event[0]) << " ";
	s << "] ";

	return s;
}

IRCD_MODULE_EXPORT
void
ircd::m::append(json::stack::object &object,
                const event &event,
                const event_append_opts &opts)
{
	const bool has_event_idx
	{
		opts.event_idx && *opts.event_idx
	};

	const bool has_client_txnid
	{
		opts.client_txnid && *opts.client_txnid
	};

	const bool has_user
	{
		opts.user_id && opts.user_room
	};

	const bool sender_is_user
	{
		has_user && json::get<"sender"_>(event) == *opts.user_id
	};

	const auto txnid_idx
	{
		!has_client_txnid && sender_is_user && opts.query_txnid?
			opts.user_room->get(std::nothrow, "ircd.client.txnid", at<"event_id"_>(event)):
			0UL
	};

	#if defined(RB_DEBUG) && 0
	if(!has_client_txnid && !txnid_idx && sender_is_user && opts.query_txnid)
		log::dwarning
		{
			log, "Could not find transaction_id for %s from %s in %s",
			json::get<"event_id"_>(event),
			json::get<"sender"_>(event),
			json::get<"room_id"_>(event)
		};
	#endif

	if(!json::get<"state_key"_>(event) && has_user)
	{
		const m::user::ignores ignores{*opts.user_id};
		if(ignores.enforce("events") && ignores.has(json::get<"sender"_>(event)))
		{
			log::debug
			{
				log, "Not sending event '%s' because '%s' is ignored by '%s'",
				json::get<"event_id"_>(event),
				json::get<"sender"_>(event),
				string_view{*opts.user_id}
			};

			return;
		}
	}

	object.append(event);

	if(json::get<"state_key"_>(event) && has_event_idx)
	{
		const auto prev_idx
		{
			room::state::prev(*opts.event_idx)
		};

		if(prev_idx)
			m::get(std::nothrow, prev_idx, "content", [&object]
			(const json::object &content)
			{
				json::stack::member
				{
					object, "prev_content", content
				};
			});
	}

	json::stack::object unsigned_
	{
		object, "unsigned"
	};

	json::stack::member
	{
		unsigned_, "age", json::value
		{
			opts.age != std::numeric_limits<long>::min()?
				opts.age:
			json::get<"depth"_>(event) >= 0 && opts.room_depth && *opts.room_depth >= 0L?
				((*opts.room_depth + 1) - json::get<"depth"_>(event)) + 100:
			!opts.room_depth && json::get<"origin_server_ts"_>(event)?
				ircd::time<milliseconds>() - json::get<"origin_server_ts"_>(event):
			json::undefined_number
		}
	};

	if(has_client_txnid)
		json::stack::member
		{
			unsigned_, "transaction_id", *opts.client_txnid
		};

	if(txnid_idx)
		m::get(std::nothrow, txnid_idx, "content", [&unsigned_]
		(const json::object &content)
		{
			json::stack::member
			{
				unsigned_, "transaction_id", unquote(content.get("transaction_id"))
			};
		});
}

IRCD_MODULE_EXPORT
bool
ircd::m::event::auth::chain::for_each(const auth::chain &c,
                                      const closure_bool &closure)
{
	m::event::fetch e, a;
	std::set<event::idx> ae;
	std::deque<event::idx> aq {c.idx}; do
	{
		const auto idx(aq.front());
		aq.pop_front();
		if(!seek(e, idx, std::nothrow))
			continue;

		const m::event::prev prev{e};
		const auto count(prev.auth_events_count());
		for(size_t i(0); i < count && i < 4; ++i)
		{
			const auto &auth_event_id(prev.auth_event(i));
			const auto &auth_event_idx(index(auth_event_id, std::nothrow));
			if(!auth_event_idx)
				continue;

			auto it(ae.lower_bound(auth_event_idx));
			if(it == end(ae) || *it != auth_event_idx)
			{
				seek(a, auth_event_idx, std::nothrow);
				ae.emplace_hint(it, auth_event_idx);
				if(a.valid)
					aq.emplace_back(auth_event_idx);
			}
		}
	}
	while(!aq.empty());

	for(const auto &idx : ae)
		if(!closure(idx))
			return false;

	return true;
}

IRCD_MODULE_EXPORT
void
ircd::m::event::refs::rebuild()
{
	static const size_t pool_size{96};
	static const size_t log_interval{8192};

	db::txn txn
	{
		*m::dbs::events
	};

	auto &column
	{
		dbs::event_json
	};

	auto it
	{
		column.begin()
	};

	ctx::dock dock;
	ctx::pool pool;
	pool.min(pool_size);

	size_t i(0), j(0);
	const ctx::uninterruptible::nothrow ui;
	for(; it; ++it)
	{
		if(ctx::interruption_requested())
			break;

		const m::event::idx event_idx
		{
			byte_view<m::event::idx>(it->first)
		};

		std::string event{it->second};
		pool([&txn, &dock, &i, &j, event(std::move(event)), event_idx]
		{
			m::dbs::write_opts wopts;
			wopts.event_idx = event_idx;

			m::dbs::_index_event_refs(txn, json::object{event}, wopts);

			if(++j % log_interval == 0) log::info
			{
				m::log, "Refs builder @%zu:%zu of %lu (@idx: %lu)",
				i,
				j,
				m::vm::sequence::retired,
				event_idx
			};

			if(j >= i)
				dock.notify_one();
		});

		++i;
	}

	dock.wait([&i, &j]
	{
		return i == j;
	});

	txn();
}

IRCD_MODULE_EXPORT
ircd::string_view
ircd::m::event::auth::failed(const m::event &event)
{
	const m::event::prev refs(event);
	const auto count(refs.auth_events_count());
	if(count > 4)
		return "Too many auth_events references.";

	const m::event *authv[4];
	const m::event::fetch auth[4]
	{
		{ count > 0? refs.auth_event(0) : event::id{}, std::nothrow },
		{ count > 1? refs.auth_event(1) : event::id{}, std::nothrow },
		{ count > 2? refs.auth_event(2) : event::id{}, std::nothrow },
		{ count > 3? refs.auth_event(3) : event::id{}, std::nothrow },
	};

	size_t i(0), j(0);
	for(; i < 4; ++i)
		if(auth[i].valid)
			authv[j++] = &auth[i];

	return failed(event, {authv, j});
}

IRCD_MODULE_EXPORT
ircd::string_view
ircd::m::event::auth::failed(const m::event &event,
                             const vector_view<const m::event *> &auth_events)
{
	const m::event::prev refs{event};

	// 1. If type is m.room.create
	if(json::get<"type"_>(event) == "m.room.create")
	{
		// a. If it has any previous events, reject.
		if(count(refs) || !empty(auth_events))
			return "m.room.create has previous events.";

		// b. If the domain of the room_id does not match the domain of the
		// sender, reject.
		assert(!conforms(event).has(conforms::MISMATCH_CREATE_SENDER));
		if(conforms(event).has(conforms::MISMATCH_CREATE_SENDER))
			return "m.room.create room_id domain does not match sender domain.";

		// c. If content.room_version is present and is not a recognised
		// version, reject.
		if(json::get<"content"_>(event).has("room_version"))
			if(unquote(json::get<"content"_>(event).get("room_version")) != "1")
				return "m.room.create room_version is not recognized.";

		// d. If content has no creator field, reject.
		assert(!empty(json::get<"content"_>(event).get("creator")));
		if(empty(json::get<"content"_>(event).get("creator")))
			return "m.room.create content.creator is missing.";

		// e. Otherwise, allow.
		return {};
	}

	// 2. Reject if event has auth_events that:
	const m::event *auth_create{nullptr};
	const m::event *auth_power{nullptr};
	const m::event *auth_join_rules{nullptr};
	const m::event *auth_member_target{nullptr};
	const m::event *auth_member_sender{nullptr};
	for(size_t i(0); i < auth_events.size(); ++i)
	{
		// a. have duplicate entries for a given type and state_key pair
		assert(auth_events.at(i));
		const m::event &a(*auth_events.at(i));
		for(size_t j(0); j < auth_events.size(); ++j) if(i != j)
		{
			assert(auth_events.at(j));
			const auto &b(*auth_events.at(j));
			if(json::get<"type"_>(a) == json::get<"type"_>(b))
				if(json::get<"state_key"_>(a) == json::get<"state_key"_>(b))
					return "Duplicate (type,state_key) in auth_events.";
		}

		// b. have entries whose type and state_key don't match those specified by
		// the auth events selection algorithm described in the server...
		const auto &type(json::get<"type"_>(a));
		if(type == "m.room.create")
		{
			assert(!auth_create);
			auth_create = &a;
			continue;
		}

		if(type == "m.room.power_levels")
		{
			assert(!auth_power);
			auth_power = &a;
			continue;
		}

		if(type == "m.room.join_rules")
		{
			assert(!auth_join_rules);
			auth_join_rules = &a;
			continue;
		}

		if(type == "m.room.member")
		{
			if(json::get<"sender"_>(event) == json::get<"state_key"_>(a))
			{
				assert(!auth_member_sender);
				auth_member_sender = &a;
			}

			if(json::get<"state_key"_>(event) == json::get<"state_key"_>(a))
			{
				assert(!auth_member_target);
				auth_member_target = &a;
			}

			continue;
		}

		return "Reference in auth_events is not an auth_event.";
	}

	// 3. If event does not have a m.room.create in its auth_events, reject.
	if(!auth_create)
		return "Missing m.room.create in auth_events";

	const m::room::power power
	{
		auth_power? *auth_power : m::event{}, *auth_create
	};

	// 4. If type is m.room.aliases:
	if(json::get<"type"_>(event) == "m.room.aliases")
	{
		// a. If event has no state_key, reject.
		assert(!conforms(event).has(conforms::MISMATCH_ALIASES_STATE_KEY));
		if(empty(json::get<"state_key"_>(event)))
			return "m.room.aliases event is missing a state_key.";

		// b. If sender's domain doesn't matches state_key, reject.
		if(json::get<"state_key"_>(event) != m::user::id(json::get<"sender"_>(event)).host())
			return "m.room.aliases event state_key is not the sender's domain.";

		// c. Otherwise, allow
		return {};
	}

	// 5. If type is m.room.member:
	if(json::get<"type"_>(event) == "m.room.member")
	{
		// a. If no state_key key ...
		assert(!conforms(event).has(conforms::MISSING_MEMBER_STATE_KEY));
		if(empty(json::get<"state_key"_>(event)))
			return "m.room.member event is missing a state_key.";

		// a. ... or membership key in content, reject.
		assert(!conforms(event).has(conforms::MISSING_CONTENT_MEMBERSHIP));
		if(empty(unquote(json::get<"content"_>(event).get("membership"))))
			return "m.room.member event is missing a content.membership.";

		assert(!conforms(event).has(conforms::INVALID_MEMBER_STATE_KEY));
		if(!valid(m::id::USER, json::get<"state_key"_>(event)))
			return "m.room.member event state_key is not a valid user mxid.";

		// b. If membership is join
		if(membership(event) == "join")
		{
			// i. If the only previous event is an m.room.create and the
			// state_key is the creator, allow.
			if(refs.prev_events_count() == 1 && refs.auth_events_count() == 1)
				if(auth_create && at<"event_id"_>(*auth_create) == refs.prev_event(0))
					return {};

			// ii. If the sender does not match state_key, reject.
			if(json::get<"sender"_>(event) != json::get<"state_key"_>(event))
				return "m.room.member membership=join sender does not match state_key.";

			// iii. If the sender is banned, reject.
			if(auth_member_target)
				if(membership(*auth_member_target) == "ban")
					return "m.room.member membership=join references membership=ban auth_event.";

			if(auth_member_sender)
				if(membership(*auth_member_sender) == "ban")
					return "m.room.member membership=join references membership=ban auth_event.";

			if(auth_join_rules)
			{
				// iv. If the join_rule is invite then allow if membership state
				// is invite or join.
				if(unquote(json::get<"content"_>(*auth_join_rules).get("join_rule")) == "invite")
					if(auth_member_target)
					{
						if(membership(*auth_member_target) == "invite")
							return {};

						if(membership(*auth_member_target) == "join")
							return {};
					}

				// v. If the join_rule is public, allow.
				if(unquote(json::get<"content"_>(*auth_join_rules).get("join_rule")) == "public")
					return {};

			}

			// vi. Otherwise, reject.
			return "m.room.member membership=join fails authorization.";
		}

		// c. If membership is invite
		if(membership(event) == "invite")
		{
			// i. If content has third_party_invite key
			if(json::get<"content"_>(event).has("third_party_invite"))
			{
				//TODO: XXX
				// 1. If target user is banned, reject.
				// 2. If content.third_party_invite does not have a signed key, reject.
				// 3. If signed does not have mxid and token keys, reject.
				// 4. If mxid does not match state_key, reject.
				// 5. If there is no m.room.third_party_invite event in the current room state with state_key matching token, reject.
				// 6. If sender does not match sender of the m.room.third_party_invite, reject.
				// 7. If any signature in signed matches any public key in the m.room.third_party_invite event, allow. The public keys are in content of m.room.third_party_invite as:
				// 7.1. A single public key in the public_key field.
				// 7.2. A list of public keys in the public_keys field.
				// 8. Otherwise, reject.
				return "third_party_invite fails authorization.";
			}

			// ii. If the sender's current membership state is not join, reject.
			if(auth_member_sender)
				if(membership(*auth_member_sender) != "join")
					return "m.room.member membership=invite sender must have membership=join.";

			// iii. If target user's current membership state is join or ban, reject.
			if(auth_member_target)
			{
				if(membership(*auth_member_target) == "join")
					return "m.room.member membership=invite target cannot have membership=join.";

				if(membership(*auth_member_target) == "ban")
					return "m.room.member membership=invite target cannot have membership=ban.";
			}

			// iv. If the sender's power level is greater than or equal to the invite level,
			// allow.
			if(power(at<"sender"_>(event), "invite"))
				return {};

			// v. Otherwise, reject.
			return "m.room.member membership=invite fails authorization.";
		}

		// d. If membership is leave
		if(membership(event) == "leave")
		{
			// i. If the sender matches state_key, allow if and only if that
			// user's current membership state is invite or join.
			if(json::get<"sender"_>(event) == json::get<"state_key"_>(event))
			{
				if(auth_member_target && membership(*auth_member_target) == "join")
					return {};

				if(auth_member_target && membership(*auth_member_target) == "invite")
					return {};

				return "m.room.member membership=leave self-target must have membership=join|invite.";
			}

			// ii. If the sender's current membership state is not join, reject.
			if(auth_member_sender)
				if(membership(*auth_member_sender) != "join")
					return "m.room.member membership=leave sender must have membership=join.";

			// iii. If the target user's current membership state is ban, and the sender's
			// power level is less than the ban level, reject.
			if(auth_member_target)
				if(membership(*auth_member_target) == "ban")
					if(!power(at<"sender"_>(event), "ban"))
						return "m.room.member membership=ban->leave sender must have ban power to unban.";

			// iv. If the sender's power level is greater than or equal to the
			// kick level, and the target user's power level is less than the
			// sender's power level, allow.
			if(power(at<"sender"_>(event), "kick"))
				if(power.level_user(at<"state_key"_>(event)) < power.level_user(at<"sender"_>(event)))
					return {};

			// v. Otherwise, reject.
			return "m.room.member membership=leave fails authorization.";
		}

		// e. If membership is ban
		if(membership(event) == "ban")
		{
			// i. If the sender's current membership state is not join, reject.
			if(auth_member_sender)
				if(membership(*auth_member_sender) != "join")
					return "m.room.member membership=ban sender must have membership=join.";

			// ii. If the sender's power level is greater than or equal to the
			// ban level, and the target user's power level is less than the
			// sender's power level, allow.
			if(power(at<"sender"_>(event), "ban"))
				if(power.level_user(at<"state_key"_>(event)) < power.level_user(at<"sender"_>(event)))
					return {};

			// iii. Otherwise, reject.
			return "m.room.member membership=ban fails authorization.";
		}

		// f. Otherwise, the membership is unknown. Reject.
		return "m.room.member membership=unknown.";
	}

	// 6. If the sender's current membership state is not join, reject.
	if(auth_member_sender)
		if(membership(*auth_member_sender) != "join")
			return "sender is not joined to room.";

	// 7. If type is m.room.third_party_invite:
	if(json::get<"type"_>(event) == "m.room.third_party_invite")
	{
		// a. Allow if and only if sender's current power level is greater
		// than or equal to the invite level.
		if(power(at<"sender"_>(event), "invite"))
			return {};

		return "sender has power level less than required for invite.";
	}

	// 8. If the event type's required power level is greater than the
	// sender's power level, reject.
	if(!power(at<"sender"_>(event), "events", at<"type"_>(event)))
		return "sender has insufficient power for event type.";

	// 9. If the event has a state_key that starts with an @ and does not
	// match the sender, reject.
	if(startswith(json::get<"state_key"_>(event), '@'))
		if(at<"state_key"_>(event) != at<"sender"_>(event))
			return "sender cannot set another user's mxid in a state_key.";

	// 10. If type is m.room.power_levels:
	if(json::get<"type"_>(event) == "m.room.power_levels")
	{
		// a. If users key in content is not a dictionary with keys that are
		// valid user IDs with values that are integers (or a string that is
		// an integer), reject.
		if(json::type(json::get<"content"_>(event).get("users")) != json::OBJECT)
			return "m.room.power_levels content.users is not a json object.";

		for(const auto &member : json::object(at<"content"_>(event).at("users")))
		{
			if(!m::valid(m::id::USER, member.first))
				return "m.room.power_levels content.users key is not a user mxid";

			if(!try_lex_cast<int64_t>(unquote(member.second)))
				return "m.room.power_levels content.users value is not an integer.";
		}

		// b. If there is no previous m.room.power_levels event in the room, allow.
		if(!auth_power)
			return {};

		const m::room::power old_power{*auth_power, *auth_create};
		const m::room::power new_power{event, *auth_create};
		const int64_t current_level
		{
			old_power.level_user(at<"sender"_>(event))
		};

		// c. For each of the keys users_default, events_default,
		// state_default, ban, redact, kick, invite, as well as each entry
		// being changed under the events or users keys:
		static const string_view keys[]
		{
			"users_default",
			"events_default",
			"state_default",
			"ban",
			"redact",
			"kick",
			"invite",
		};

		for(const auto &key : keys)
		{
			const int64_t old_level(old_power.level(key));
			const int64_t new_level(new_power.level(key));
			if(old_level == new_level)
				continue;

			// i. If the current value is higher than the sender's current
			// power level, reject.
			if(old_level > current_level)
				return "m.room.power_levels property denied to sender's power level.";

			// ii. If the new value is higher than the sender's current power
			// level, reject.
			if(new_level > current_level)
				return "m.room.power_levels property exceeds sender's power level.";
		}

		string_view ret;
		using closure = m::room::power::closure_bool;
		old_power.for_each("users", closure{[&ret, &new_power, &current_level]
		(const string_view &user_id, const int64_t &old_level)
		{
			if(new_power.has_user(user_id))
				if(new_power.level_user(user_id) == old_level)
					return true;

			// i. If the current value is higher than the sender's current
			// power level, reject.
			if(old_level > current_level)
			{
				ret = "m.room.power_levels user property denied to sender's power level.";
				return false;
			};

			// ii. If the new value is higher than the sender's current power
			// level, reject.
			if(new_power.level_user(user_id) > current_level)
			{
				ret = "m.room.power_levels user property exceeds sender's power level.";
				return false;
			};

			return true;
		}});

		if(ret)
			return ret;

		new_power.for_each("users", closure{[&ret, &old_power, &current_level]
		(const string_view &user_id, const int64_t &new_level)
		{
			if(old_power.has_user(user_id))
				if(old_power.level_user(user_id) == new_level)
					return true;

			// i. If the current value is higher than the sender's current
			// power level, reject.
			if(new_level > current_level)
			{
				ret = "m.room.power_levels user property exceeds sender's power level.";
				return false;
			};

			return true;
		}});

		if(ret)
			return ret;

		old_power.for_each("events", closure{[&ret, &new_power, &current_level]
		(const string_view &type, const int64_t &old_level)
		{
			if(new_power.has_event(type))
				if(new_power.level_event(type) == old_level)
					return true;

			// i. If the current value is higher than the sender's current
			// power level, reject.
			if(old_level > current_level)
			{
				ret = "m.room.power_levels event property denied to sender's power level.";
				return false;
			};

			// ii. If the new value is higher than the sender's current power
			// level, reject.
			if(new_power.level_event(type) > current_level)
			{
				ret = "m.room.power_levels event property exceeds sender's power level.";
				return false;
			};

			return true;
		}});

		if(ret)
			return ret;

		new_power.for_each("events", closure{[&ret, &old_power, &current_level]
		(const string_view &type, const int64_t &new_level)
		{
			if(old_power.has_event(type))
				if(old_power.level_event(type) == new_level)
					return true;

			// i. If the current value is higher than the sender's current
			// power level, reject.
			if(new_level > current_level)
			{
				ret = "m.room.power_levels event property exceeds sender's power level.";
				return false;
			};

			return true;
		}});

		// d. For each entry being changed under the users key...
		old_power.for_each("users", closure{[&ret, &event, &new_power, &current_level]
		(const string_view &user_id, const int64_t &old_level)
		{
			// ...other than the sender's own entry:
			if(user_id == at<"sender"_>(event))
				return true;

			if(new_power.has_user(user_id))
				if(new_power.level_user(user_id) == old_level)
					return true;

			// i. If the current value is equal to the sender's current power
			// level, reject.
			if(old_level == current_level)
			{
				ret = "m.room.power_levels user property denied to sender's power level.";
				return false;
			};

			return true;
		}});

		if(ret)
			return ret;

		// e. Otherwise, allow.
		assert(!ret);
		return ret;
	}

	// 11. If type is m.room.redaction:
	if(json::get<"type"_>(event) == "m.room.redaction")
	{
		// a. If the sender's power level is greater than or equal to the
		// redact level, allow.
		if(power(at<"sender"_>(event), "redact"))
			return {};

		// b. If the domain of the event_id of the event being redacted is the
		// same as the domain of the event_id of the m.room.redaction, allow.
		if(event::id(json::get<"redacts"_>(event)).host() == event::id(at<"event_id"_>(event)).host())
			return {};

		// c. Otherwise, reject.
		return "m.room.redaction fails authorization.";
	}

	// 12. Otherwise, allow.
	return {};
}

IRCD_MODULE_EXPORT
bool
ircd::m::event::auth::is_power_event(const m::event &event)
{
	if(!json::get<"type"_>(event))
		return false;

	if(json::get<"type"_>(event) == "m.room.create")
		return true;

	if(json::get<"type"_>(event) == "m.room.power_levels")
		return true;

	if(json::get<"type"_>(event) == "m.room.join_rules")
		return true;

	if(json::get<"type"_>(event) != "m.room.member")
		return false;

	if(!json::get<"sender"_>(event) || !json::get<"state_key"_>(event))
		return false;

	if(json::get<"sender"_>(event) == json::get<"state_key"_>(event))
		return false;

	if(membership(event) == "leave" || membership(event) == "ban")
		return true;

	return false;
}

IRCD_MODULE_EXPORT
void
ircd::m::event::essential(json::iov &event,
                          const json::iov &contents,
                          const event::closure_iov_mutable &closure)
{
	const auto &type
	{
		event.at("type")
	};

	if(type == "m.room.aliases")
	{
		const json::iov::push _content{event,
		{
			"content", json::members
			{
				{ "aliases", contents.at("aliases") }
			}
		}};

		closure(event);
	}
	else if(type == "m.room.create")
	{
		const json::iov::push _content{event,
		{
			"content", json::members
			{
				{ "creator", contents.at("creator") }
			}
		}};

		closure(event);
	}
	else if(type == "m.room.history_visibility")
	{
		const json::iov::push _content{event,
		{
			"content", json::members
			{
				{ "history_visibility", contents.at("history_visibility") }
			}
		}};

		closure(event);
	}
	else if(type == "m.room.join_rules")
	{
		const json::iov::push _content{event,
		{
			"content", json::members
			{
				{ "join_rule", contents.at("join_rule") }
			}
		}};

		closure(event);
	}
	else if(type == "m.room.member")
	{
		const json::iov::push _content{event,
		{
			"content", json::members
			{
				{ "membership", contents.at("membership") }
			}
		}};

		closure(event);
	}
	else if(type == "m.room.power_levels")
	{
		const json::iov::push _content{event,
		{
			"content", json::members
			{
				{ "ban", contents.at("ban")                        },
				{ "events", contents.at("events")                  },
				{ "events_default", contents.at("events_default")  },
				{ "kick", contents.at("kick")                      },
				{ "redact", contents.at("redact")                  },
				{ "state_default", contents.at("state_default")    },
				{ "users", contents.at("users")                    },
				{ "users_default", contents.at("users_default")    },
			}
		}};

		closure(event);
	}
	else if(type == "m.room.redaction")
	{
		// This simply finds the redacts key and swaps it with jsundefined for
		// the scope's duration. The redacts key will still be present and
		// visible in the json::iov which is incorrect if directly serialized.
		// However, this iov is turned into a json::tuple (m::event) which ends
		// up being serialized for signing. That serialization is where the
		// jsundefined redacts value is ignored.
		auto &redacts{event.at("redacts")};
		json::value temp(std::move(redacts));
		redacts = json::value{};
		const unwind _{[&redacts, &temp]
		{
			redacts = std::move(temp);
		}};

		const json::iov::push _content
		{
			event, { "content", "{}" }
		};

		closure(event);
	}
	else
	{
		const json::iov::push _content
		{
			event, { "content", "{}" }
		};

		closure(event);
	}
}

IRCD_MODULE_EXPORT
ircd::m::event
ircd::m::essential(m::event event,
                   const mutable_buffer &contentbuf)
{
	const auto &type
	{
		json::at<"type"_>(event)
	};

	json::object &content
	{
		json::get<"content"_>(event)
	};

	mutable_buffer essential
	{
		contentbuf
	};

	if(type == "m.room.aliases")
	{
		content = json::stringify(essential, json::members
		{
			{ "aliases", content.at("aliases") }
		});
	}
	else if(type == "m.room.create")
	{
		content = json::stringify(essential, json::members
		{
			{ "creator", content.at("creator") }
		});
	}
	else if(type == "m.room.history_visibility")
	{
		content = json::stringify(essential, json::members
		{
			{ "history_visibility", content.at("history_visibility") }
		});
	}
	else if(type == "m.room.join_rules")
	{
		content = json::stringify(essential, json::members
		{
			{ "join_rule", content.at("join_rule") }
		});
	}
	else if(type == "m.room.member")
	{
		content = json::stringify(essential, json::members
		{
			{ "membership", content.at("membership") }
		});
	}
	else if(type == "m.room.power_levels")
	{
		content = json::stringify(essential, json::members
		{
			{ "ban", content.at("ban")                       },
			{ "events", content.at("events")                 },
			{ "events_default", content.at("events_default") },
			{ "kick", content.at("kick")                     },
			{ "redact", content.at("redact")                 },
			{ "state_default", content.at("state_default")   },
			{ "users", content.at("users")                   },
			{ "users_default", content.at("users_default")   },
		});
	}
	else if(type == "m.room.redaction")
	{
		json::get<"redacts"_>(event) = string_view{};
		content = "{}"_sv;
	}
	else
	{
		content = "{}"_sv;
	}

	json::get<"signatures"_>(event) = {};
	return event;
}
