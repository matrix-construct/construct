// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	static bool room_state_fetch_result(room::state::fetch &, const room::state::fetch::opts &, const room::state::fetch::closure &, const json::array &, const string_view &);
}

ircd::m::room::state::fetch::fetch(const opts &opts,
                                   const closure &closure)
{
	m::room room
	{
		opts.room
	};

	feds::opts fopts;
	fopts.op = feds::op::state;
	fopts.room_id = room.room_id;
	fopts.event_id = room.event_id;
	fopts.arg[0] = "ids";
	fopts.exclude_myself = true;
	fopts.closure_errors = false;
	log::debug
	{
		log, "Resynchronizing %s state at %s from %zu joined servers...",
		string_view{room.room_id},
		room.event_id?
			string_view{room.event_id}:
			"HEAD"_sv,
		room::origins(room).count(),
	};

	feds::execute(fopts, [this, &opts, &closure]
	(const auto &result)
	{
		this->respond++;

		const json::array &auth_chain_ids
		{
			result.object["auth_chain_ids"]
		};

		const json::array &pdu_ids
		{
			result.object["pdu_ids"]
		};

		if(!room_state_fetch_result(*this, opts, closure, auth_chain_ids, result.origin))
			return false;

		if(!room_state_fetch_result(*this, opts, closure, pdu_ids, result.origin))
			return false;

		return true;
	});
}

bool
ircd::m::room_state_fetch_result(room::state::fetch &f,
                                 const room::state::fetch::opts &opts,
                                 const room::state::fetch::closure &closure,
                                 const json::array &ids,
                                 const string_view &remote)
{
	event::id event_id[64];
	auto it(begin(ids)); do
	{
		size_t i(0);
		for(; i < 64 && it != end(ids); ++it)
			event_id[i++] = json::string{*it};

		const vector_view<const event::id> event_ids
		(
			event_id, i
		);

		const uint64_t exists
		{
			!opts.existing?
				m::exists(event_ids):
				0UL
		};

		f.responses += i;
		f.exists += __builtin_popcountl(exists);
		for(size_t j(0); j < i; ++j)
		{
			if(exists & (1UL << j))
				continue;

			auto it
			{
				f.result.lower_bound(event_id[j])
			};

			if(likely(opts.unique))
			{
				if(it != std::end(f.result) && *it == event_id[j])
				{
					f.concur++;
					continue;
				}

				it = f.result.emplace_hint(it, event_id[j]);
			}

			if(closure && opts.unique)
			{
				// When a reference can be made to the result set: prefer it.
				assert(it != std::end(f.result));
				assert(*it == event_id[j]);
				if(!closure(*it, remote))
					return false;
			}
			else if(closure)
			{
				if(!closure(event_id[j], remote))
					return false;
			}
		}
	}
	while(it != end(ids));

	return true;
}
