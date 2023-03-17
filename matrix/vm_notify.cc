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
	static const size_t max_ids {64};

	assume(event_id.size() <= max_ids);
	const auto event_ids
	{
		std::min(event_id.size(), max_ids)
	};

	const uint64_t exists_mask
	{
		m::exists(event_id)
	};

	node_type node[max_ids];
	iterator_type it[event_ids];
	ctx::future<> future[event_ids];
	ctx::promise<> promise[event_ids];
	for(size_t i(0); i < event_ids; ++i)
	{
		if(exists_mask & (1UL << i))
		{
			future[i] = ctx::future<>{ctx::already};
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
			map, map.emplace(event_id[i], promise + i)
		};

		assert(it[i]->second);
		future[i] = ctx::future<>
		{
			*it[i]->second
		};
	}

	auto all
	{
		ctx::when_all(future, future + event_ids)
	};

	const bool ok
	{
		all.wait_until(now<system_point>() + to, std::nothrow)
	};

	const size_t exists
	{
		!ok?
			m::exists_count(event_id):
			event_ids
	};

	assert(exists <= event_ids);
	return exists;
}

//
// future::future
//

ircd::m::vm::notify::future::future(const event::id &event_id)
:ctx::future<>{ctx::already}
{
	if(m::exists(event_id))
		return;

	const auto &s
	{
		map.get_allocator().s
	};

	assert(s);
	assert(!s->next);
	const scope_restore next
	{
		s->next, reinterpret_cast<notify::value_type *>(&node)
	};

	it =
	{
		map, map.emplace(event_id, &promise)
	};

	assert(it->second);
	static_cast<ctx::future<> &>(*this) = ctx::future<>
	{
		*it->second
	};
}

//
// internal
//

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
		const auto &[event_id, promise] {*pit.first};

		assert(promise);
		assert(promise->valid());
		assert(event_id == event.event_id);

		if(likely(*promise))
			promise->set_value();
	}
}
