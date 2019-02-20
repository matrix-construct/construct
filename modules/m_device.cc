// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::mapi::header
IRCD_MODULE
{
	"Matrix device library; modular components."
};

bool
IRCD_MODULE_EXPORT
ircd::m::device::set(const m::user &user,
                     const device &device)
{
	const user::room user_room{user};
	const string_view &device_id
	{
		at<"device_id"_>(device)
	};

	json::for_each(device, [&user, &user_room, &device_id]
	(const auto &prop, auto &&val)
	{
		if(!json::defined(json::value(val)))
			return;

		char buf[m::event::TYPE_MAX_SIZE];
		const string_view type{fmt::sprintf
		{
			buf, "ircd.device.%s", prop
		}};

		m::send(user_room, user, type, device_id,
		{
			{ "", val }
		});
	});

	return true;
}

/// To delete a device we iterate the user's room state for all types matching
/// ircd.device.* (and ircd.device) which have a state_key of the device_id.
/// Those events are redacted which removes them from appearing in the state.
bool
IRCD_MODULE_EXPORT
ircd::m::device::del(const m::user &user,
                     const string_view &id)
{
	const user::room user_room{user};
	const room::state state{user_room};
	const room::state::type_prefix type
	{
		"ircd.device."
	};

	state.for_each(type, [&user, &id, &user_room, &state]
	(const string_view &type)
	{
		const auto event_idx
		{
			state.get(std::nothrow, type, id)
		};

		const auto event_id
		{
			m::event_id(event_idx, std::nothrow)
		};

		if(event_id)
			m::redact(user_room, user, event_id, "deleted");

		return true;
	});

	return true;
}

bool
IRCD_MODULE_EXPORT
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
	(const string_view &type)
	{
		ret = state.has(type, id);
		return !ret;
	});

	return ret;
}

bool
IRCD_MODULE_EXPORT
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
IRCD_MODULE_EXPORT
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
	(const string_view &type)
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
IRCD_MODULE_EXPORT
ircd::m::device::for_each(const m::user &user,
                          const closure_bool &closure)
{
	const m::room::state::keys_bool state_key{[&closure]
	(const string_view &state_key)
	{
		return closure(state_key);
	}};

	const m::user::room user_room{user};
	const m::room::state state{user_room};
	return state.for_each("ircd.device.device_id", state_key);
}
