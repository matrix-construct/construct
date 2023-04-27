// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::m::user::devices::send::send(const m::user::devices &devices,
                                   const m::id::device &device_id,
                                   const string_view room_id)
try
{
	assert(device_id);

	const auto &user_id
	{
		devices.user.user_id
	};

	const bool deleted
	{
		devices.has(device_id)
	};

	const m::user::keys user_keys
	{
		user_id
	};

	const bool has_keys
	{
		!deleted && user_keys.has_device(device_id)
	};

	const unique_mutable_buffer keys_buf
	{
		has_keys? 4_KiB: 0_KiB
	};

	json::stack keys{keys_buf};
	if(has_keys)
	{
		json::stack::object top{keys};
		user_keys.device(top, device_id);
	}

	// Triggers a devices request from the remote; also see
	// modules/federation/user_devices.cc
	static const auto stream_id{1L};

	json::iov event, content;
	const json::iov::push push[]
	{
		{ event,    { "type",       "m.device_list_update"  } },
		{ event,    { "sender",     user_id                 } },
		{ content,  { "deleted",    deleted                 } },
		{ content,  { "device_id",  device_id               } },
		{ content,  { "stream_id",  stream_id               } },
		{ content,  { "user_id",    user_id                 } },
	};

	const json::iov::push push_keys
	{
		content, has_keys,
		{
			"keys", [&keys]
			{
				return keys.completed();
			}
		}
	};

	// For diagnostic purposes; usually not defined.
	const json::iov::push push_room_id
	{
		event, m::valid(m::id::ROOM, room_id),
		{
			"room_id", [&room_id]
			{
				return room_id;
			}
		}
	};

	m::vm::copts opts;
	opts.edu = true;
	opts.prop_mask.reset();
	opts.prop_mask.set("origin");
	opts.notify_clients = false;
	m::vm::eval
	{
		event, content, opts
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
		m::log, "Send m.device_list_update for '%s' belonging to %s :%s",
		string_view{device_id},
		string_view{devices.user.user_id},
		e.what(),
	};
}

bool
ircd::m::user::devices::update(const device_list_update &update)
{
	const m::user &user
	{
		json::at<"user_id"_>(update)
	};

	// Don't create unknown users on this codepath since there's no efficient
	// check if this is just spam; updates for unknowns are just dropped here.
	if(!exists(user))
	{
		log::derror
		{
			log, "Refusing device update for unknown user %s",
			string_view{user.user_id},
		};

		return false;
	}

	const m::user::devices devices
	{
		user
	};

	const auto &device_id
	{
		json::at<"device_id"_>(update)
	};

	if(json::get<"deleted"_>(update))
		return devices.del(device_id);

	// Properties we're interested in for now...
	static const string_view mask[]
	{
		"device_id",
		"device_display_name",
		"keys",
	};

	bool ret {false};
	json::for_each(update, mask, [&ret, &devices, &device_id]
	(const auto &prop, auto &&val)
	{
		if constexpr(std::is_assignable<string_view, decltype(val)>())
		{
			if(json::defined(json::value(val)))
				ret |= devices.set(device_id, prop, val);
		}
	});

	return ret;
}

std::map<std::string, long>
ircd::m::user::devices::count_one_time_keys(const m::user &user,
                                            const string_view &device_id)
{
	const m::user::devices devices
	{
		user
	};

	std::map<std::string, long> ret;
	devices.for_each(device_id, [&ret]
	(const auto &event_idx, const string_view &type)
	{
		if(!startswith(type, "one_time_key|"))
			return true;

		const auto &[prefix, ident]
		{
			split(type, '|')
		};

		const auto &[algorithm, name]
		{
			split(ident, ':')
		};

		assert(prefix == "one_time_key");
		assert(!empty(algorithm));
		assert(!empty(ident));
		assert(!empty(name));

		auto it(ret.lower_bound(algorithm));
		if(it == end(ret) || it->first != algorithm)
			it = ret.emplace_hint(it, algorithm, 0L);

		auto &count(it->second);
		++count;
		return true;
	});

	return ret;
}

bool
ircd::m::user::devices::del(const string_view &id)
const
{
	const user::room user_room
	{
		user
	};

	const auto event_idx
	{
		user_room.get(std::nothrow, "ircd.device.device_id", id)
	};

	const auto event_id
	{
		m::event_id(std::nothrow, event_idx)
	};

	if(!event_id)
		return false;

	const auto redaction_id
	{
		m::redact(user_room, user_room.user, event_id, "deleted")
	};

	if(my(user))
		user::devices::send
		{
			*this, id
		};

	return true;
}

bool
ircd::m::user::devices::set(const device &device)
const
{
	const string_view &device_id
	{
		json::at<"device_id"_>(device)
	};

	bool ret {false};
	json::for_each(device, [this, &device_id, &ret]
	(const auto &prop, auto &&val)
	{
		if constexpr(std::is_assignable<string_view, decltype(val)>())
		{
			if(json::defined(json::value(val)))
				ret |= set(device_id, prop, val);
		}
	});

	return ret;
}

bool
ircd::m::user::devices::set(const string_view &id,
                            const string_view &prop,
                            const string_view &val)
const
{
	bool dup {false};
	const bool got
	{
		get(std::nothrow, id, prop, [&val, &dup]
		(const auto &event_idx, const json::string &existing) noexcept
		{
			dup = val == existing;
		})
	};

	assert(!dup || got);
	return !dup?
		put(id, prop, val):
		false;
}

bool
ircd::m::user::devices::put(const string_view &id,
                            const string_view &prop,
                            const string_view &val)
const
{
	char buf[m::event::TYPE_MAX_SIZE];
	const string_view type{fmt::sprintf
	{
		buf, "ircd.device.%s", prop
	}};

	const user::room user_room
	{
		user
	};

	m::send(user_room, user, type, id, json::members
	{
		{ "", val }
	});

	return true;
}

bool
ircd::m::user::devices::has(const string_view &id)
const
{
	const user::room user_room
	{
		user
	};

	return user_room.has("ircd.device.device_id", id);
}

bool
ircd::m::user::devices::has(const string_view &id,
                            const string_view &prop)
const
{
	bool ret{false};
	get(std::nothrow, id, prop, [&ret]
	(const auto &event_idx, const string_view &value) noexcept
	{
		ret = !empty(value);
	});

	return ret;
}

bool
ircd::m::user::devices::get(const string_view &id,
                            const string_view &prop,
                            const closure &c)
const
{
	const bool ret
	{
		get(std::nothrow, id, prop, c)
	};

	if(!ret)
		throw m::NOT_FOUND
		{
			"Property '%s' for device '%s' for user %s not found",
			id,
			prop,
			string_view{user.user_id}
		};

	return ret;
}

bool
ircd::m::user::devices::get(std::nothrow_t,
                            const string_view &id,
                            const string_view &prop,
                            const closure &closure)
const
{
	const m::user::room user_room
	{
		user
	};

	char buf[m::event::TYPE_MAX_SIZE];
	const string_view type{fmt::sprintf
	{
		buf, "ircd.device.%s", prop
	}};

	const auto event_idx
	{
		user_room.get(std::nothrow, type, id)
	};

	return m::get(std::nothrow, event_idx, "content", [&event_idx, &closure]
	(const json::object &content)
	{
		const string_view &value
		{
			content.get("")
		};

		closure(event_idx, value);
	});
}

bool
ircd::m::user::devices::for_each(const string_view &device_id,
                                 const closure_bool &closure)
const
{
	const m::user::room user_room
	{
		user
	};

	const m::room::state state
	{
		user_room
	};

	if(!state.has("ircd.device.device_id", device_id))
		return true;

	const room::state::type_prefix type
	{
		"ircd.device."
	};

	return state.for_each(type, [&state, &device_id, &closure]
	(const string_view &type, const string_view &state_key, const event::idx &event_idx)
	{
		if(state_key != device_id)
			return true;

		const string_view &prop
		{
			lstrip(type, "ircd.device.")
		};

		return closure(event_idx, prop);
	});
}

bool
ircd::m::user::devices::for_each(const closure_bool &closure)
const
{
	const m::user::room user_room
	{
		user
	};

	const m::room::state state
	{
		user_room
	};

	return state.for_each("ircd.device.device_id", [&closure]
	(const string_view &, const string_view &state_key, const event::idx &event_idx)
	{
		return closure(event_idx, state_key);
	});
}
