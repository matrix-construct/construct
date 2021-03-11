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
	static void bootstrap_event_vector(homeserver &);
}

void
ircd::m::homeserver::bootstrap()
try
{
	assert(opts);
	assert(dbs::events);
	if(opts->bootstrap_vector_path)
		return bootstrap_event_vector(*this);

	assert(db::sequence(*dbs::events) == 0);
	assert(this->self);
	const m::user::id &my_id
	{
		this->self
	};

	const m::user me
	{
		my_id
	};

	const m::node my_node
	{
		origin(*this)
	};

	const m::node::room node_room
	{
		my_node
	};

	const m::room::id::buf my_room_id
	{
		"ircd", origin(*this)
	};

	const m::room my_room
	{
		my_room_id
	};

	const m::room::id::buf conf_room_id
	{
		"conf", origin(*this)
	};

	const m::room conf_room
	{
		conf_room_id
	};

	const m::room::id::buf tokens_room_id
	{
		"tokens", origin(*this)
	};

	const m::room tokens_room
	{
		tokens_room_id
	};

	const m::room::id::buf public_room_id
	{
		"public", origin(*this)
	};

	const m::room public_room
	{
		public_room_id
	};

	const m::room::id::buf alias_room_id
	{
		"alias", origin(*this)
	};

	const m::room alias_room
	{
		alias_room_id
	};

	const m::room::id::buf control_room_id
	{
		"control", origin(*this)
	};

	const m::room control_room
	{
		control_room_id
	};

	if(my_id.hostname() == "localhost")
		log::warning
		{
			"The server's name is configured to localhost. This is probably not what you want."
		};

	assert(key);
	assert(!key->verify_keys.empty());
	m::keys::cache::set(key->verify_keys);

	create(me);
	mutable_cast(me).activate();

	create(my_room, me);
	create(conf_room, me);
	create(tokens_room, me);
	create(public_room, me);
	create(alias_room, me);
	create(control_room, me);

	send(my_room, me, "m.room.name", "",
	{
		{ "name", "IRCd's Room" }
	});

	send(my_room, me, "m.room.topic", "",
	{
		{ "topic", "The daemon's den." }
	});

	send(conf_room, me, "m.room.name", "",
	{
		{ "name", "Server Configuration" }
	});

	send(tokens_room, me, "m.room.name", "",
	{
		{ "name", "User Tokens" }
	});

	send(control_room, me, "m.room.name", "",
	{
		{ "name", "Control Room" }
	});

	log::info
	{
		log, "Bootstrap event generation completed nominally."
	};
}
catch(const std::exception &e)
{
	log::logf
	{
		log, log::level::CRITICAL,
		"Failed to bootstrap server '%s' on network '%s' :%s",
		server_name(*this),
		origin(*this),
		e.what()
	};

	throw ircd::panic
	{
		"bootstrap %s error :%s",
		server_name(*this),
		e.what()
	};
}

void
ircd::m::bootstrap_event_vector(homeserver &homeserver)
try
{
	assert(homeserver.opts);
	const auto &hs_opts
	{
		*homeserver.opts
	};

	const string_view &path
	{
		hs_opts.bootstrap_vector_path
	};

	const bool validate_json_only
	{
		has(string_view(ircd::diagnostic), "valid-json")
	};

	fs::fd::opts fileopts(std::ios::in);
	const fs::fd file
	{
		path, fileopts
	};

	fs::map::opts map_opts(fileopts);
	map_opts.sequential = true;
	map_opts.huge2mb = true;
	const fs::map map
	{
		file, map_opts
	};

	// This array is backed by the mmap
	const json::array events
	{
		const_buffer{map}
	};

	char pbuf[4][48];
	log::notice
	{
		log, "Bootstrapping database from events @ `%s' %s",
		path,
		pretty(pbuf[0], iec(size(map))),
	};

	auto &current(ctx::cur());
	const run::changed handle_quit
	{
		run::level::QUIT, [&current]
		{
			ctx::interrupt(current);
		}
	};

	// Options for eval. This eval disables all phases except a select few.
	// These may change based on assumptions about the input.
	vm::opts vmopts;
	vmopts.phase.reset();

	// Primary interest is to perform the INDEX and WRITE phase which create
	// a database transaction and commit it respectively.
	vmopts.phase.set(vm::phase::INDEX, true);
	vmopts.phase.set(vm::phase::WRITE, true);

	// Perform prefetches over the whole batch;
	vmopts.mprefetch_refs = true;

	//XXX Consider enable for large batch size.
	//vmopts.phase.set(vm::phase::PREINDEX, true);

	// Optimize the bootstrap by not updating room heads at every step.
	vmopts.wopts.appendix.set(dbs::appendix::ROOM_HEAD, false);
	vmopts.wopts.appendix.set(dbs::appendix::ROOM_HEAD_RESOLVE, false);

	// Perform normal static-conformity checks; there's no reason to accept
	// inputs that wouldn't normally be accepted. While inputs are supposed
	// to be trusted and authentic, their correctness should still be checked;
	// attempting to recover from a catastrophic failure might be the reason
	// for the rebuild.
	vmopts.phase.set(vm::phase::CONFORM, true);

	//TODO: XXX
	// This workaround is required for internal rooms to work, for now.
	vmopts.non_conform.set(event::conforms::MISMATCH_ORIGIN_SENDER);

	// Indicates the input JSON is canonical (to optimize eval).
	//vmopts.json_source = true;
	vmopts.non_conform.set(event::conforms::MISMATCH_HASHES);

	// Optimize eval if we assume there's only one copy of each event in the
	// input array.
	vmopts.unique = false;

	// Optimize eval if we guarantee there's only one copy of each event
	// in the input. This assumption is made when bootstrapping fresh DB.
	vmopts.replays = sequence(*dbs::events) == 0;

	// Error control
	//vmopts.nothrows = -1UL;

	// Outputs to infolog for each event; may be noisy;
	vmopts.infolog_accept = false;

	static const size_t batch_max {64};
	std::vector<m::event> vec(batch_max);
	size_t count {0}, ebytes[2] {0, 1}, accept {0};
	vm::eval eval
	{
		vmopts
	};

	util::timer stopwatch;
	auto it(begin(events));
	while(it != end(events)) try
	{
		// page in the JSON
		size_t i(0);
		for(; i < vec.size() && it != end(events); ++i, ++it)
		{
			// Account for the bytes of the value plus one `,` separator
			const string_view &elem(*it);
			ebytes[1] += elem.size() + 1;

			// In validate mode there's no need to load up the event tuple
			if(validate_json_only)
				continue;

			vec[i] = json::object{elem};
		}

		const vector_view<const m::event> batch
		{
			vec.data(), vec.data() + i
		};

		// process the event batch; make the batch size 0 for validate
		if(likely(!validate_json_only))
			execute(eval, batch);

		count += i;

		auto opts(map_opts);
		opts.offset = ebytes[0];
		const size_t incore
		{
			ebytes[1] > ebytes[0]?
				ebytes[1] - ebytes[0]:
				0UL
		};

		// advise dontneed
		ebytes[0] += evict(map, incore, opts);
		if(count % (batch_max * 256) != 0)
			continue;

		const auto db_bytes
		{
			db::ticker(*dbs::events, "rocksdb.bytes.written")
		};

		const auto elapsed
		{
			stopwatch.at<seconds>().count()
		};

		log::info
		{
			log, "Bootstrap sequence:%zu accepts:%zu faults:%zu %s in %s | %zu event/s; input %s/s; output %s/s",
			vm::sequence::retired,
			eval.accepted,
			eval.faulted,
			pretty(pbuf[0], iec(ebytes[1])),
			stopwatch.pretty(pbuf[1]),
			(count / std::max(elapsed, 1L)),
			pretty(pbuf[2], iec(ebytes[1] / std::max(elapsed, 1L)), 1),
			pretty(pbuf[3], iec(db_bytes / std::max(elapsed, 1L)), 1),
		};

		ctx::yield();
		ctx::interruption_point();
	}
	catch(const json::parse_error &e)
	{
		log::critical
		{
			log, "Bootstrap retired:%zu count:%zu accept:%zu offset:%zu :%s",
			vm::sequence::retired,
			count,
			accept,
			std::distance(begin(events).start, it.start),
			e.what(),
		};

		break;
	}

	// Manual flush of the memtables is required in case the user disabled the
	// WAL (which is advised in the documentation). If this isn't run several
	// thousand keys in memory will be dropped inconsistently between database
	// columns. If WAL is enabled then it tidies the DB up just as well.
	if(likely(sequence(*dbs::events) > 0))
	{
		const bool blocking(true), allow_stall(true);
		db::sort(*dbs::events, blocking, allow_stall);
	}

	log::notice
	{
		log, "Bootstrapped count:%zu retired:%zu in %s from `%s' in %s",
		count,
		vm::sequence::retired,
		pretty(pbuf[0], iec(size(string_view(events)))),
		path,
		stopwatch.pretty(pbuf[1]),
	};
}
catch(const std::exception &e)
{
	throw ircd::error
	{
		ircd::error::hide_name, "%s", e.what(),
	};
}
catch(const ctx::terminated &)
{
	throw ircd::error
	{
		"bootstrap %s :terminated without completion",
		server_name(homeserver),
	};
}
