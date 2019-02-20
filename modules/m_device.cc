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
	return true;
}

bool
IRCD_MODULE_EXPORT
ircd::m::device::del(const m::user &user,
                     const string_view &id)
{
	const m::user::room user_room{user};
	const m::room::state state{user_room};
	const m::event::idx event_idx
	{
		state.get(std::nothrow, "ircd.device", id)
	};

	if(!event_idx)
		return false;

	const m::event::id::buf event_id
	{
		m::event_id(event_idx, std::nothrow)
	};

	m::redact(user_room, user, event_id, "deleted");
	return true;
}

bool
IRCD_MODULE_EXPORT
ircd::m::device::get(std::nothrow_t,
                     const m::user &user,
                     const string_view &id,
                     const closure &closure)
{
	const m::user::room user_room{user};
	const m::room::state state{user_room};
	const m::event::idx event_idx
	{
		state.get(std::nothrow, "ircd.device", id)
	};

	if(!event_idx)
		return false;

	return m::get(std::nothrow, event_idx, "content", [&closure]
	(const json::object &content)
	{
		closure(content);
	});
}

bool
IRCD_MODULE_EXPORT
ircd::m::device::for_each(const m::user &user,
                          const closure_bool &closure)
{
	return for_each(user, id_closure_bool{[&user, &closure]
	(const string_view &device_id)
	{
		bool ret(true);
		get(std::nothrow, user, device_id, [&closure, &ret]
		(const device &device)
		{
			ret = closure(device);
		});

		return ret;
	}});
}

bool
IRCD_MODULE_EXPORT
ircd::m::device::for_each(const m::user &user,
                          const id_closure_bool &closure)
{
	const m::room::state::keys_bool state_key{[&closure]
	(const string_view &state_key)
	{
		return closure(state_key);
	}};

	const m::user::room user_room{user};
	const m::room::state state{user_room};
	return state.for_each("ircd.device", state_key);
}
