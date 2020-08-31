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
		{
			if(e.event_->event_id == event_id)
				ret = &e;
		}
		else if(e.issue)
		{
			if(e.issue->has("event_id"))
				if(string_view{e.issue->at("event_id")} == event_id)
					ret = &e;
		}
		else if(e.event_id == event_id)
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
		{
			if(e.event_->event_id == event_id)
				++ret;
		}
		else if(e.issue)
		{
			if(e.issue->has("event_id"))
				if(string_view{e.issue->at("event_id")} == event_id)
					++ret;
		}
		else if(e.event_id == event_id)
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

const ircd::m::event *
ircd::m::vm::eval::find_pdu(const eval &eval,
                            const event::id &event_id)
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

size_t
ircd::m::vm::eval::count(const ctx::ctx *const &c)
{
	return std::count_if(begin(eval::list), end(eval::list), [&c]
	(const eval *const &eval)
	{
		return eval->ctx == c;
	});
}

ircd::m::vm::eval *
ircd::m::vm::eval::find_root(const eval &a,
                             const ctx::ctx &c)
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
ircd::m::vm::eval::find_parent(const eval &a,
                               const ctx::ctx &c)
{
	eval *ret {nullptr};
	for_each(&c, [&ret, &a]
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
	operator()(event, content);
}

ircd::m::vm::eval::eval(const event &event,
                        const vm::opts &opts)
:eval{opts}
{
	operator()(event);
}

ircd::m::vm::eval::eval(const json::array &pdus,
                        const vm::opts &opts)
:eval{opts}
{
	std::vector<m::event> events(begin(pdus), end(pdus));

	// Sort the events first to avoid complicating the evals; the events might
	// be from different rooms but it doesn't matter.
	if(likely(!opts.ordered))
		std::sort(begin(events), end(events));

	operator()(events);
}

ircd::m::vm::eval::eval(const vector_view<m::event> &events,
                        const vm::opts &opts)
:eval{opts}
{
	operator()(events);
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

size_t
ircd::m::vm::eval::operator()(const vector_view<m::event> &events)
{
	assert(opts);
	const scope_restore eval_pdus
	{
		this->pdus, events
	};

	if(likely(opts->phase[phase::VERIFY] && opts->mfetch_keys))
		mfetch_keys();

	// Conduct each eval without letting any one exception ruin things for the
	// others, including an interrupt. The only exception is a termination.
	size_t ret(0);
	for(auto it(begin(events)); it != end(events); ++it) try
	{
		const m::event &event
		{
			*it
		};

		const auto status
		{
			operator()(event)
		};

		ret += status == fault::ACCEPT;
	}
	catch(const ctx::interrupted &e)
	{
		throw;
	}
	catch(const std::exception &e)
	{
		continue;
	}

	return ret;
}

/// Inject a new event originating from this server.
///
ircd::m::vm::fault
ircd::m::vm::eval::operator()(json::iov &event,
                              const json::iov &contents)
{
	return vm::inject(*this, event, contents);
}

ircd::m::vm::fault
ircd::m::vm::eval::operator()(const event &event)
{
	return vm::execute(*this, event);
}

ircd::m::vm::eval::operator
const event::id::buf &()
const
{
	return event_id;
}

void
ircd::m::vm::eval::mfetch_keys()
const
{
	using m::fed::key::server_key;

	// Determine federation keys which we don't have.
	std::set<server_key> miss;
	for(const auto &event : this->pdus)
	{
		// When the node_id is set (eval on behalf of remote) we only parallel
		// fetch keys from that node for events from that node. This is to
		// prevent amplification. Note that these will still be evaluated and
		// key fetching may be attempted, but not here.
		assert(opts);
		const auto &origin(json::get<"origin"_>(event));
		if(opts->node_id && opts->node_id != origin)
			continue;

		const json::object &signature
		{
			json::get<"signatures"_>(event).get(origin)
		};

		for(const auto &[key_id, sig] : signature)
		{
			const server_key key(origin, key_id);
			const auto it(miss.lower_bound(key));
			if(it != end(miss) && *it == key)
				continue;

			if(m::keys::cache::has(origin, key_id))
				continue;

			miss.emplace_hint(it, key);
		}
	}

	//TODO: XXX
	const std::vector<server_key> queries(begin(miss), end(miss));
	if(!queries.empty())
		log::debug
		{
			log, "%s fetching %zu new keys from %zu events...",
			loghead(*this),
			queries.size(),
			this->pdus.size(),
		};

	const size_t fetched
	{
		!queries.empty()?
			m::keys::fetch(queries):
			0UL
	};

	if(fetched)
		log::info
		{
			log, "%s fetched %zu of %zu new keys from %zu events",
			loghead(*this),
			fetched,
			queries.size(),
			this->pdus.size(),
		};
}
