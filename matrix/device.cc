// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

std::map<std::string, long>
ircd::m::device::count_one_time_keys(const user &user,
                                     const string_view &device_id)
{
	std::map<std::string, long> ret;
	for_each(user, device_id, [&ret]
	(const string_view &type)
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
ircd::m::device::set(const device_list_update &update)
{
	const m::user &user
	{
		json::at<"user_id"_>(update)
	};

	const auto &device_id
	{
		json::at<"device_id"_>(update)
	};

	if(json::get<"deleted"_>(update))
		return del(user, device_id);

	// Properties we're interested in for now...
	static const string_view mask[]
	{
		"device_id",
		"device_display_name",
		"keys",
	};

	bool ret {false};
	json::for_each(update, mask, [&ret, &user, &device_id]
	(const auto &prop, auto &&val)
	{
		if constexpr(std::is_assignable<string_view, decltype(val)>())
		{
			if(json::defined(json::value(val)))
				ret |= set(user, device_id, prop, val);
		}
	});

	return ret;
}

bool
ircd::m::device::set(const m::user &user,
                     const device &device)
{
	const string_view &device_id
	{
		json::at<"device_id"_>(device)
	};

	bool ret {false};
	json::for_each(device, [&user, &device_id, &ret]
	(const auto &prop, auto &&val)
	{
		if constexpr(std::is_assignable<string_view, decltype(val)>())
		{
			if(json::defined(json::value(val)))
				ret |= set(user, device_id, prop, val);
		}
	});

	return ret;
}

bool
ircd::m::device::set(const m::user &user,
                     const string_view &id,
                     const string_view &prop,
                     const string_view &val)
{
	bool dup {false};
	const bool got
	{
		get(std::nothrow, user, id, prop, [&val, &dup]
		(const json::string &existing)
		{
			dup = val == existing;
		})
	};

	assert(!dup || got);
	return !dup?
		put(user, id, prop, val):
		false;
}

bool
ircd::m::device::put(const m::user &user,
                     const string_view &id,
                     const string_view &prop,
                     const string_view &val)
{
	char buf[m::event::TYPE_MAX_SIZE];
	const string_view type{fmt::sprintf
	{
		buf, "ircd.device.%s", prop
	}};

	const user::room user_room{user};
	m::send(user_room, user, type, id, json::members
	{
		{ "", val }
	});

	return true;
}

bool
ircd::m::device::del(const m::user &user,
                     const string_view &id)
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

	if(!my(user))
		return true;

	json::iov content;
	const json::iov::push push[]
	{
		{ content,  { "user_id",    user.user_id  } },
		{ content,  { "device_id",  id            } },
		{ content,  { "deleted",    true          } },
	};

	const bool broadcasted
	{
		device_list_update::send(content)
	};

	return true;
}

bool
ircd::m::device::has(const m::user &user,
                     const string_view &id)
{
	const user::room user_room{user};
	const room::state state{user_room};
	const room::state::type_prefix type
	{
		"ircd.device."
	};

	bool ret(false);
	state.for_each(type, [&state, &id, &ret]
	(const string_view &type, const string_view &, const event::idx &)
	{
		ret = state.has(type, id);
		return !ret;
	});

	return ret;
}

bool
ircd::m::device::has(const m::user &user,
                     const string_view &id,
                     const string_view &prop)
{
	bool ret{false};
	get(std::nothrow, user, id, prop, [&ret]
	(const string_view &value)
	{
		ret = !empty(value);
	});

	return ret;
}

bool
ircd::m::device::get(const m::user &user,
                     const string_view &id,
                     const string_view &prop,
                     const closure &c)
{
	const bool ret
	{
		get(std::nothrow, user, id, prop, c)
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
ircd::m::device::get(std::nothrow_t,
                     const m::user &user,
                     const string_view &id,
                     const string_view &prop,
                     const closure &closure)
{
	char buf[m::event::TYPE_MAX_SIZE];
	const string_view type{fmt::sprintf
	{
		buf, "ircd.device.%s", prop
	}};

	const m::user::room user_room{user};
	const m::room::state state{user_room};
	const auto event_idx
	{
		state.get(std::nothrow, type, id)
	};

	return m::get(std::nothrow, event_idx, "content", [&closure]
	(const json::object &content)
	{
		const string_view &value
		{
			content.get("")
		};

		closure(value);
	});
}

bool
ircd::m::device::for_each(const m::user &user,
                          const string_view &device_id,
                          const closure_bool &closure)
{
	const m::user::room user_room{user};
	const m::room::state state{user_room};
	const room::state::type_prefix type
	{
		"ircd.device."
	};

	return state.for_each(type, [&state, &device_id, &closure]
	(const string_view &type, const string_view &, const event::idx &)
	{
		const string_view &prop
		{
			lstrip(type, "ircd.device.")
		};

		return state.has(type, device_id)?
			closure(prop):
			true;
	});
}

bool
ircd::m::device::for_each(const m::user &user,
                          const closure_bool &closure)
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
	(const string_view &, const string_view &state_key, const event::idx &)
	{
		return closure(state_key);
	});
}

bool
ircd::m::device_list_update::send(json::iov &content)
try
{
	assert(content.has("user_id"));
	assert(content.has("device_id"));

	json::iov event;
	const json::iov::push push[]
	{
		{ event, { "type",    "m.direct_to_device"  } },
		{ event, { "sender",  content.at("user_id") } },
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

	return true;
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
		content.at("device_id"),
		content.at("user_id"),
		e.what(),
	};

	return false;
}
