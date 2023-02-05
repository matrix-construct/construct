// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::vm::notify
{
	static void hook_handle(const m::event &, vm::eval &);

	extern hookfn<vm::eval &> hook;
	extern alloc_type alloc;
	extern map_type map;
}

decltype(ircd::m::vm::notify::alloc)
ircd::m::vm::notify::alloc;

decltype(ircd::m::vm::notify::map)
ircd::m::vm::notify::map
{
	alloc
};

decltype(ircd::m::vm::notify::hook)
ircd::m::vm::notify::hook
{
	hook_handle,
	{
		{ "_site",  "vm.notify" }
	}
};

/// Yields ctx until all events were successfully evaluated or timeout.
/// Returns the count of events which were successfully evaluated.
size_t
ircd::m::vm::notify::wait(const vector_view<const event::id> &event_id,
                          const milliseconds to)
{
	using iterator_type = unique_iterator<map_type>;
	using node_type = std::pair<map_type::node_type, value_type>;

	static const size_t max_ids
	{
		64
	};

	assume(event_id.size() <= max_ids);
	const auto event_ids
	{
		std::min(event_id.size(), max_ids)
	};

	const uint64_t exists_mask
	{
		m::exists(event_id)
	};

	size_t exists(0);
	node_type node[max_ids];
	iterator_type it[event_ids];
	for(size_t i(0); i < event_ids; ++i)
	{
		if(exists_mask & (1UL << i))
		{
			exists++;
			continue;
		}

		const auto &s
		{
			map.get_allocator().s
		};

		assert(s);
		assert(!s->next);
		const scope_restore next
		{
			s->next, reinterpret_cast<value_type *>(node + i)
		};

		it[i] =
		{
			map, map.emplace(event_id[i], ctx::current)
		};
	}

	bool timeout(false);
	const auto tp(now<system_point>() + to);
	while(exists < event_ids && !timeout)
	{
		timeout = ctx::wait_until(tp, std::nothrow);
		exists = m::exists_count(event_id);
	}

	assert(exists <= event_ids);
	return exists;
}

void
ircd::m::vm::notify::hook_handle(const m::event &event,
                                 vm::eval &)
{
	auto pit
	{
		map.equal_range(event.event_id)
	};

	for(; pit.first != pit.second; ++pit.first)
	{
		const auto &[event_id, ctx] {*pit.first};

		assert(event_id == event.event_id);
		assert(ctx != nullptr);

		ctx::notify(*ctx);
	}
}
