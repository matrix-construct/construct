// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::vm::log)
ircd::m::vm::log
{
	"m.vm", 'v'
};

decltype(ircd::m::vm::dock)
ircd::m::vm::dock;

decltype(ircd::m::vm::ready)
ircd::m::vm::ready;

decltype(ircd::m::vm::default_copts)
ircd::m::vm::default_copts;

decltype(ircd::m::vm::default_opts)
ircd::m::vm::default_opts;

//
// init
//

ircd::m::vm::init::init()
{
	id::event::buf event_id;
	sequence::retired = sequence::get(event_id);
	sequence::committed = sequence::retired;
	sequence::uncommitted = sequence::committed;

	vm::ready = true;
	vm::dock.notify_all();

	log::info
	{
		log, "BOOT %s @%lu [%s] db:%lu",
		server_name(my()),
		sequence::retired,
		sequence::retired?
			string_view{event_id} : "NO EVENTS"_sv,
		m::dbs::events?
			db::sequence(*m::dbs::events) : 0UL,
	};
}

ircd::m::vm::init::~init()
noexcept
{
	vm::ready = false;

	if(!eval::list.empty())
		log::warning
		{
			log, "Waiting for %zu evals (exec:%zu inject:%zu pending:%zu)",
			eval::list.size(),
			eval::executing,
			eval::injecting,
			sequence::pending,
		};

	vm::dock.wait([]
	{
		return !eval::executing && !eval::injecting;
	});

	assert(!sequence::pending);

	event::id::buf event_id;
	const auto retired
	{
		sequence::get(event_id)
	};

	log::info
	{
		log, "HALT '%s' @%lu [%s] vm:%lu:%lu:%lu db:%lu",
		server_name(my()),
		retired,
		retired?
			string_view{event_id} : "NO EVENTS"_sv,
		sequence::retired,
		sequence::committed,
		sequence::uncommitted,
		m::dbs::events?
			db::sequence(*m::dbs::events) : 0UL,
	};

	assert(retired == sequence::retired);
}

//
// m/vm.h
//

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
		buf, "vm:%lu:%lu:%lu eval:%lu seq:%lu share:%lu:%lu %s",
		sequence::uncommitted,
		sequence::committed,
		sequence::retired,
		eval.id,
		sequence::get(eval),
		eval.sequence_shared[0],
		eval.sequence_shared[1],
		eval.event_?
			string_view{eval.event_->event_id}:
			"<unidentified>"_sv,
	};
}

ircd::http::code
ircd::m::vm::http_code(const fault &code)
{
	switch(code)
	{
		case fault::ACCEPT:       return http::OK;
		case fault::EXISTS:       return http::CONFLICT;
		case fault::INVALID:      return http::BAD_REQUEST;
		case fault::GENERAL:      return http::UNAUTHORIZED;
		case fault::AUTH:         return http::FORBIDDEN;
		case fault::STATE:        return http::NOT_FOUND;
		case fault::EVENT:        return http::NOT_FOUND;
		default:                  return http::INTERNAL_SERVER_ERROR;
	}
}

ircd::string_view
ircd::m::vm::reflect(const enum fault &code)
{
	switch(code)
	{
		case fault::ACCEPT:       return "#ACCEPT";
		case fault::EXISTS:       return "#EXISTS";
		case fault::GENERAL:      return "#GENERAL";
		case fault::INVALID:      return "#INVALID";
		case fault::AUTH:         return "#AUTH";
		case fault::EVENT:        return "#EVENT";
		case fault::STATE:        return "#STATE";
		case fault::INTERRUPT:    return "#INTERRUPT";
	}

	return "??????";
}

//
// sequence
//

decltype(ircd::m::vm::sequence::dock)
ircd::m::vm::sequence::dock;

decltype(ircd::m::vm::sequence::retired)
ircd::m::vm::sequence::retired;

decltype(ircd::m::vm::sequence::committed)
ircd::m::vm::sequence::committed;

decltype(ircd::m::vm::sequence::uncommitted)
ircd::m::vm::sequence::uncommitted;

uint64_t
ircd::m::vm::sequence::min()
{
	const auto *const e
	{
		eval::seqmin()
	};

	return e? get(*e) : 0;
}

uint64_t
ircd::m::vm::sequence::max()
{
	const auto *const e
	{
		eval::seqmax()
	};

	return e? get(*e) : 0;
}

uint64_t
ircd::m::vm::sequence::get(id::event::buf &event_id)
{
	static constexpr auto column_idx
	{
		json::indexof<event, "event_id"_>()
	};

	auto &column
	{
		dbs::event_column.at(column_idx)
	};

	const auto it
	{
		column.rbegin()
	};

	if(!it)
	{
		// If this iterator is invalid the events db should
		// be completely fresh.
		assert(db::sequence(*dbs::events) == 0);
		return 0;
	}

	const auto &ret
	{
		byte_view<uint64_t>(it->first)
	};

	event_id = it->second;
	return ret;
}

const uint64_t &
ircd::m::vm::sequence::get(const eval &eval)
{
	return eval.sequence;
}
