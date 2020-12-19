// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

//
// Eval
//
// Processes any event from any place from any time and does whatever is
// necessary to validate, reject, learn from new information, ignore old
// information and advance the state of IRCd as best as possible.

/// Instance list linkage for all of the evaluations.
template<>
decltype(ircd::util::instance_list<ircd::m::vm::eval>::allocator)
ircd::util::instance_list<ircd::m::vm::eval>::allocator
{};

template<>
decltype(ircd::util::instance_list<ircd::m::vm::eval>::list)
ircd::util::instance_list<ircd::m::vm::eval>::list
{
	allocator
};

decltype(ircd::m::vm::eval::id_ctr)
ircd::m::vm::eval::id_ctr;

decltype(ircd::m::vm::eval::executing)
ircd::m::vm::eval::executing;

decltype(ircd::m::vm::eval::injecting)
ircd::m::vm::eval::injecting;

size_t
ircd::m::vm::fetch_keys(const eval &eval)
{
	using m::fed::key::server_key;

	std::vector<server_key> queries;
	for(const auto &event : eval.pdus)
		for(const auto &[server_name, signatures] : json::get<"signatures"_>(event))
			for(const auto &[key_id, signature] : json::object(signatures))
			{
				const server_key query
				{
					json::get<"origin"_>(event), key_id
				};

				// Check if we're already making a query.
				if(std::binary_search(begin(queries), end(queries), query))
					continue;

				// Check if we already have the key.
				if(m::keys::cache::has(json::get<"origin"_>(event), key_id))
					continue;

				// If there's a cached error on the host we can skip here.
				if(m::fed::errant(json::get<"origin"_>(event)))
					continue;

				queries.emplace_back(json::get<"origin"_>(event), key_id);
				std::sort(begin(queries), end(queries));
			}

	const size_t fetched
	{
		!queries.empty()?
			m::keys::fetch(queries):
			0UL
	};

	return fetched;
}

size_t
ircd::m::vm::prefetch_refs(const eval &eval)
{
	assert(eval.opts);
	const dbs::write_opts &wopts
	{
		eval.opts->wopts
	};

	size_t prefetched(0);
	for(const auto &event : eval.pdus)
	{
		if(event.event_id)
			prefetched += m::prefetch(event.event_id, "_event_idx");

		prefetched += dbs::prefetch(event, wopts);
	}

	return prefetched;
}

ircd::string_view
ircd::m::vm::loghead(const eval &eval)
{
	thread_local char buf[128];
	return loghead(buf, eval);
}

ircd::string_view
ircd::m::vm::loghead(const mutable_buffer &buf,
                     const eval &eval)
{
	return fmt::sprintf
	{
		buf, "vm:%lu:%lu:%lu parent:%lu %s eval:%lu %s seq:%lu %s",
		sequence::retired,
		sequence::committed,
		sequence::uncommitted,
		eval.parent?
			eval.parent->id:
			0UL,
		eval.parent?
			reflect(eval.parent->phase):
			reflect(phase::NONE),
		eval.id,
		reflect(eval.phase),
		sequence::get(eval),
		eval.event_?
			string_view{eval.event_->event_id}:
			"<unidentified>"_sv,
	};
}

ircd::m::vm::eval *
ircd::m::vm::find_root(const eval &a,
                       const ctx::ctx &c)
noexcept
{
	eval *ret {nullptr}, *parent {nullptr}; do
	{
		if(!(parent = find_parent(a, c)))
			return ret;

		ret = parent;
	}
	while(1);
}

ircd::m::vm::eval *
ircd::m::vm::find_parent(const eval &a,
                         const ctx::ctx &c)
noexcept
{
	eval *ret {nullptr};
	eval::for_each(&c, [&ret, &a]
	(eval &eval)
	{
		const bool cond
		{
			(&eval != &a) && (!ret || eval.id > ret->id)
		};

		ret = cond? &eval : ret;
		return true;
	});

	return ret;
}

const ircd::m::event *
ircd::m::vm::find_pdu(const eval &eval,
                      const event::id &event_id)
noexcept
{
	const m::event *ret{nullptr};
	for(const auto &event : eval.pdus)
	{
		if(event.event_id != event_id)
			continue;

		ret = std::addressof(event);
		break;
	}

	return ret;
}

//
// eval::eval
//

ircd::m::vm::eval::eval(const vm::opts &opts)
:opts{&opts}
,parent
{
	find_parent(*this)
}
{
	if(parent)
	{
		assert(!parent->child);
		parent->child = this;
	}
}

ircd::m::vm::eval::eval(const vm::copts &opts)
:opts{&opts}
,copts{&opts}
,parent
{
	find_parent(*this)
}
{
	if(parent)
	{
		assert(!parent->child);
		parent->child = this;
	}
}

ircd::m::vm::eval::eval(json::iov &event,
                        const json::iov &content,
                        const vm::copts &opts)
:eval{opts}
{
	inject(*this, event, content);
}

ircd::m::vm::eval::eval(const event &event,
                        const vm::opts &opts)
:eval
{
	vector_view<const m::event>(&event, 1),
	opts
}
{
}

ircd::m::vm::eval::eval(const json::array &pdus,
                        const vm::opts &opts)
:eval{opts}
{
	execute(*this, pdus);
}

ircd::m::vm::eval::eval(const vector_view<const m::event> &events,
                        const vm::opts &opts)
:eval{opts}
{
	execute(*this, events);
}

ircd::m::vm::eval::~eval()
noexcept
{
	assert(!child);
	if(parent)
	{
		assert(parent->child == this);
		parent->child = nullptr;
	}
}

//
// Tools
//

void
ircd::m::vm::eval::seqsort()
{
	eval::list.sort([]
	(const auto *const &a, const auto *const &b)
	{
		if(sequence::get(*a) == 0)
			return false;

		if(sequence::get(*b) == 0)
			return true;

		return sequence::get(*a) < sequence::get(*b);
	});
}

ircd::m::vm::eval *
ircd::m::vm::eval::seqmin()
{
	const auto it
	{
		std::min_element(begin(eval::list), end(eval::list), []
		(const auto *const &a, const auto *const &b)
		{
			if(sequence::get(*a) == 0)
				return false;

			if(sequence::get(*b) == 0)
				return true;

			return sequence::get(*a) < sequence::get(*b);
		})
	};

	if(it == end(eval::list))
		return nullptr;

	if(sequence::get(**it) == 0)
		return nullptr;

	return *it;
}

ircd::m::vm::eval *
ircd::m::vm::eval::seqmax()
{
	const auto it
	{
		std::max_element(begin(eval::list), end(eval::list), []
		(const auto *const &a, const auto *const &b)
		{
			return sequence::get(*a) < sequence::get(*b);
		})
	};

	if(it == end(eval::list))
		return nullptr;

	if(sequence::get(**it) == 0)
		return nullptr;

	return *it;
}

ircd::m::vm::eval *
ircd::m::vm::eval::seqnext(const uint64_t &seq)
{
	eval *ret{nullptr};
	for(auto *const &eval : eval::list)
	{
		if(sequence::get(*eval) <= seq)
			continue;

		if(!ret || sequence::get(*eval) < sequence::get(*ret))
			ret = eval;
	}

	assert(!ret || sequence::get(*ret) > seq);
	return ret;
}

bool
ircd::m::vm::eval::sequnique(const uint64_t &seq)
{
	return 1 == std::count_if(begin(eval::list), end(eval::list), [&seq]
	(const auto *const &eval)
	{
		return sequence::get(*eval) == seq;
	});
}

ircd::m::vm::eval &
ircd::m::vm::eval::get(const event::id &event_id)
{
	auto *const ret
	{
		find(event_id)
	};

	if(unlikely(!ret))
		throw std::out_of_range
		{
			"eval::get(): event_id not being evaluated."
		};

	return *ret;
}

ircd::m::vm::eval *
ircd::m::vm::eval::find(const event::id &event_id)
{
	eval *ret{nullptr};
	for_each([&event_id, &ret](eval &e)
	{
		if(e.event_)
			if(e.event_->event_id == event_id)
				ret = &e;

		return ret == nullptr;
	});

	return ret;
}

size_t
ircd::m::vm::eval::count(const event::id &event_id)
{
	size_t ret(0);
	for_each([&event_id, &ret](eval &e)
	{
		if(e.event_)
			if(e.event_->event_id == event_id)
				++ret;

		return true;
	});

	return ret;
}

const ircd::m::event *
ircd::m::vm::eval::find_pdu(const event::id &event_id)
{
	const m::event *ret{nullptr};
	for_each_pdu([&ret, &event_id]
	(const m::event &event)
	{
		if(event.event_id != event_id)
			return true;

		ret = std::addressof(event);
		return false;
	});

	return ret;
}

size_t
ircd::m::vm::eval::count(const ctx::ctx *const &c)
{
	return std::count_if(begin(eval::list), end(eval::list), [&c]
	(const eval *const &eval)
	{
		return eval->ctx == c;
	});
}

bool
ircd::m::vm::eval::for_each(const ctx::ctx *const &c,
                            const std::function<bool (eval &)> &closure)
{
	for(eval *const &eval : eval::list)
		if(eval->ctx == c)
			if(!closure(*eval))
				return false;

	return true;
}

bool
ircd::m::vm::eval::for_each_pdu(const std::function<bool (const event &)> &closure)
{
	return for_each([&closure](eval &e)
	{
		if(!empty(e.pdus))
		{
			for(const auto &pdu : e.pdus)
				if(!closure(pdu))
					return false;
		}
		else if(e.event_)
		{
			if(!closure(*e.event_))
				return false;
		}

		return true;
	});
}

bool
ircd::m::vm::eval::for_each(const std::function<bool (eval &)> &closure)
{
	for(eval *const &eval : eval::list)
		if(!closure(*eval))
			return false;

	return true;
}
