// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

/// Cancels all background work by the events database. This will make the
/// database shutdown more fluid, without waiting for large compactions.
static const ircd::run::changed
ircd_m_dbs_handle_quit
{
	ircd::run::level::QUIT, []
	{
		if(ircd::m::dbs::events)
			ircd::db::bgcancel(*ircd::m::dbs::events, false); // non-blocking
	}
};

/// Initializes the m::dbs subsystem; sets up the events database. Held/called
/// by m::init. Most of the extern variables in m::dbs are not ready until
/// this call completes.
///
/// We also update the fs::basepath for the database directory to include our
/// servername in the path component. The fs::base::DB setting was generated
/// during the build and install process, and is unaware of our servername
/// at runtime. This change deconflicts multiple instances of IRCd running in
/// the same installation prefix using different servernames (i.e clustering
/// on the same machine).
///
ircd::m::dbs::init::init(const string_view &servername,
                         std::string dbopts)
:our_dbpath
{
	fs::path_string(fs::path_views
	{
		fs::base::db, servername
	})
}
,their_dbpath
{
	fs::base::db
}
{
	const unwind_exceptional dtors
	{
		[this] { this->~init(); }
	};

	// NOTE that this is a global change that leaks outside of ircd::m. The
	// database directory for the entire process is being changed here.
	fs::base::db.set(our_dbpath);

	// Recall the db directory init manually with the now-updated basepath
	db::chdir();

	// Open database
	events = std::make_shared<database>
	(
		"events", std::move(dbopts), desc::events
	);

	init_columns();
	apply_updates();
}

/// Shuts down the m::dbs subsystem; closes the events database. The extern
/// variables in m::dbs will no longer be functioning after this call.
ircd::m::dbs::init::~init()
noexcept
{
	// Unref DB (should close)
	events = {};

	// restore the fs::base::DB path the way we found it.
	fs::base::db.set(their_dbpath);
}

void
ircd::m::dbs::init::init_columns()
{
	// Cache the columns for the event tuple in order for constant time lookup
	assert(event_columns == event::size());
	std::array<string_view, event::size()> keys;
	_key_transform(event{}, begin(keys), end(keys));

	// Construct global convenience references for the event property columns.
	for(size_t i(0); i < keys.size(); ++i)
		event_column.at(i) = db::column
		{
			*events, keys.at(i), std::nothrow
		};

	// Construct global convenience references for the metadata columns
	event_idx = db::column{*events, desc::event_idx.name};
	event_json = db::column{*events, desc::event_json.name};
	event_refs = db::domain{*events, desc::event_refs.name};
	event_horizon = db::domain{*events, desc::event_horizon.name};
	event_sender = db::domain{*events, desc::event_sender.name};
	event_type = db::domain{*events, desc::event_type.name};
	event_state = db::domain{*events, desc::event_state.name};
	room_head = db::domain{*events, desc::room_head.name};
	room_events = db::domain{*events, desc::room_events.name};
	room_type = db::domain{*events, desc::room_type.name};
	room_joined = db::domain{*events, desc::room_joined.name};
	room_state = db::domain{*events, desc::room_state.name};
	room_state_space = db::domain{*events, desc::room_state_space.name};
}

void
ircd::m::dbs::init::apply_updates()
{
	size_t i(0);
	for(auto update(init::update); *update; ++update, ++i) try
	{
		log::debug
		{
			log, "Checking for database schema update #%zu ...",
			i,
		};

		(*update)();
	}
	catch(const std::exception &e)
	{
		log::critical
		{
			log, "Database schema update #%zu :%s",
			i,
			e.what(),
		};

		throw;
	}
}

decltype(ircd::m::dbs::init::update)
ircd::m::dbs::init::update
{
	nullptr,
};
