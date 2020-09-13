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
	assert(dbs::events);
	assert(db::sequence(*dbs::events) == 0);

	assert(opts);
	if(opts->bootstrap_vector_path)
		return bootstrap_event_vector(*this);

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

	const m::room::id::buf bridge_room_id
	{
		"bridge", origin(*this)
	};

	const m::room bridge_room
	{
		bridge_room_id
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
	const_cast<m::user &>(me).activate();

	create(my_room, me);
	create(conf_room, me);
	create(tokens_room, me);
	create(public_room, me);
	create(alias_room, me);
	create(control_room, me);
	create(bridge_room, me);

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
	const string_view &path
	{
		homeserver.opts->bootstrap_vector_path
	};

	fs::fd::opts fileopts(std::ios::in);
	const fs::fd file
	{
		path, fileopts
	};

	fs::map::opts map_opts(fileopts);
	map_opts.sequential = true;
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
	vmopts.phase[vm::phase::INDEX] = true;
	vmopts.phase[vm::phase::WRITE] = true;

	// Perform normal static-conformity checks; there's no reason to accept
	// inputs that wouldn't normally be accepted. While inputs are supposed
	// to be trusted and authentic, their correctness should still be checked;
	// attempting to recover from a catastrophic failure might be the reason
	// for the rebuild.
	vmopts.phase[vm::phase::CONFORM] = true;

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

	static const size_t window_size
	{
		4_MiB
	};

	size_t count {0}, ebytes[2] {0}, accept {0}, exists {0};
	vm::eval execute
	{
		vmopts
	};

	// Perform the eval
	util::timer stopwatch;
	for(auto it(begin(events)); it != end(events); ++it)
	{
		const json::object object
		{
			*it
		};

		const m::event event
		{
			object
		};

		const auto code
		{
			execute(event)
		};

		count += 1;
		accept += code == vm::fault::ACCEPT;
		exists += code == vm::fault::EXISTS;
		ebytes[1] += object.string_view::size();
		const size_t incore
		{
			ebytes[1] > ebytes[0]?
				ebytes[1] - ebytes[0]:
				0UL
		};

		if(incore >= window_size)
		{
			auto opts(map_opts);
			opts.offset = ebytes[0];
			ebytes[0] += evict(map, incore, opts);

			const auto db_bytes
			{
				db::ticker(*dbs::events, "rocksdb.bytes.written")
			};

			log::info
			{
				log, "Bootstrap retired:%zu count:%zu accept:%zu exists:%zu %s in %s | %zu event/s; input %s/s; output %s/s",
				vm::sequence::retired,
				count,
				accept,
				exists,
				pretty(pbuf[0], iec(ebytes[1])),
				stopwatch.pretty(pbuf[1]),
				(count / std::max(stopwatch.at<seconds>().count(), 1L)),
				pretty(pbuf[2], iec(ebytes[1] / std::max(stopwatch.at<seconds>().count(),1L)), 1),
				pretty(pbuf[3], iec(db_bytes / std::max(stopwatch.at<seconds>().count(),1L)), 1),
			};

			ctx::yield();
			ctx::interruption_point();
		}
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
		"bootstrap %s :%s",
		server_name(homeserver),
		e.what(),
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
