// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

void
ircd::m::user::keys::cross_user(json::stack::object &out)
const
{
	const auto &user_id
	{
		user_room.user.user_id
	};

	const auto event_idx
	{
		user_room.get(std::nothrow, "ircd.cross_signing.user", "")
	};

	m::get(std::nothrow, event_idx, "content", [&out, &user_id]
	(const json::object &content)
	{
		json::stack::member
		{
			out, user_id, content
		};
	});
}

void
ircd::m::user::keys::cross_self(json::stack::object &out)
const
{
	const auto &user_id
	{
		user_room.user.user_id
	};

	const auto event_idx
	{
		user_room.get(std::nothrow, "ircd.cross_signing.self", "")
	};

	m::get(std::nothrow, event_idx, "content", [&out, &user_id]
	(const json::object &content)
	{
		json::stack::member
		{
			out, user_id, content
		};
	});
}

void
ircd::m::user::keys::cross_master(json::stack::object &out)
const
{
	const auto event_idx
	{
		user_room.get(std::nothrow, "ircd.cross_signing.master", "")
	};

	m::get(std::nothrow, event_idx, "content", [this, &out]
	(const json::object &device_keys)
	{
		const auto &user_id
		{
			user_room.user.user_id
		};

		json::stack::object object
		{
			out, user_id
		};

		for(const auto &member : device_keys)
			if(member.first != "signatures")
				json::stack::member
				{
					object, member
				};

		json::stack::object sigs
		{
			object, "signatures"
		};

		json::stack::object user_sigs
		{
			sigs, user_id
		};

		attach_sigs(user_sigs, device_keys);
		const json::object device_keys_keys
		{
			device_keys["keys"]
		};

		const m::room::state state
		{
			user_room
		};

		state.for_each("ircd.keys.signatures", [this, &user_sigs, &device_keys_keys]
		(const string_view &, const string_view &state_key, const auto &event_idx)
		{
			for(const auto &[key_id_, key] : device_keys_keys)
			{
				const auto &key_id
				{
					split(key_id_, ':').second
				};

				const auto &[target, source]
				{
					unmake_sigs_state_key(state_key)
				};

				if(target != key_id)
					continue;

				attach_sigs(user_sigs, event_idx);
			}

			return true;
		});
	});
}

void
ircd::m::user::keys::device(json::stack::object &object,
                            const string_view &device_id)
const
{
	const m::user::devices devices
	{
		user_room.user
	};

	devices.get(std::nothrow, device_id, "keys", [this, &object, &device_id]
	(const auto &, const json::object &device_keys)
	{
		const auto &user_id
		{
			user_room.user.user_id
		};

		for(const auto &member : device_keys)
			if(member.first != "signatures")
				json::stack::member
				{
					object, member
				};

		json::stack::object sigs
		{
			object, "signatures"
		};

		json::stack::object user_sigs
		{
			sigs, user_id
		};

		attach_sigs(user_sigs, device_keys);
		const m::room::state state
		{
			user_room
		};

		state.for_each("ircd.keys.signatures", [this, &user_sigs, &device_id]
		(const string_view &, const string_view &state_key, const auto &event_idx)
		{
			const auto &[target, source]
			{
				unmake_sigs_state_key(state_key)
			};

			if(target && target != device_id)
				return true;

			attach_sigs(user_sigs, event_idx);
			return true;
		});
	});
}

bool
ircd::m::user::keys::attach_sigs(json::stack::object &user_sigs,
                                 const event::idx &event_idx)
const
{
	return m::get(std::nothrow, event_idx, "content", [this, &user_sigs]
	(const json::object &device_sigs)
	{
		attach_sigs(user_sigs, device_sigs);
	});
}

void
ircd::m::user::keys::attach_sigs(json::stack::object &user_sigs,
                                 const json::object &device_sigs)
const
{
	const auto &user_id
	{
		user_room.user.user_id
	};

	const json::object device_sigs_sigs
	{
		device_sigs["signatures"]
	};

	const json::object device_sigs_user_sigs
	{
		device_sigs_sigs[user_id]
	};

	for(const auto &member : device_sigs_user_sigs)
		json::stack::member
		{
			user_sigs, member
		};
}

//
// user::keys::sigs
//

std::tuple<ircd::string_view, ircd::string_view>
ircd::m::user::keys::unmake_sigs_state_key(const string_view &state_key)
noexcept
{
	const auto &[tgt, src]
	{
		rsplit(state_key, '%')
	};

	return std::tuple
	{
		tgt, src
	};
}

ircd::string_view
ircd::m::user::keys::make_sigs_state_key(const mutable_buffer &buf,
                                         const string_view &tgt,
                                         const string_view &src)
{
	return fmt::sprintf
	{
		buf, "%s%s",
		string_view{tgt},
		tgt != src && src?
			string_view{src}:
			string_view{},
	};
}
