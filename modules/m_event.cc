// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

// NOTE: !!!
// Definitions re currently split between libircd and modules until the API
// and dependency graph has stabilized. Eventually most/all of ircd::m::
// should migrate out of libircd into modules.

ircd::mapi::header
IRCD_MODULE
{
	"Matrix event library"
};

std::ostream &
IRCD_MODULE_EXPORT
ircd::m::pretty_stateline(std::ostream &out,
                          const event &event,
                          const event::idx &event_idx)
{
	const room room
	{
		json::get<"room_id"_>(event)
	};

	const room::state &state
	{
		room
	};

	const bool active
	{
		event_idx?
			state.has(event_idx):
			false
	};

	const bool redacted
	{
		event_idx?
			bool(m::redacted(event_idx)):
			false
	};

	const bool power
	{
		m::room::auth::is_power_event(event)
	};

	const room::auth::passfail auth[]
	{
		event_idx?
			room::auth::check_static(event):
			room::auth::passfail{false, {}},

		event_idx && m::exists(event.event_id)?
			room::auth::check_relative(event):
			room::auth::passfail{false, {}},

		event_idx?
			room::auth::check_present(event):
			room::auth::passfail{false, {}},
	};

	char buf[32];
	const string_view flags
	{
		fmt::sprintf
		{
			buf, "%c %c%c%c%c%c",

			active? '*' : ' ',
			power? '@' : ' ',
			redacted? 'R' : ' ',

			std::get<bool>(auth[0]) && !std::get<std::exception_ptr>(auth[0])? ' ':
			!std::get<bool>(auth[0]) && std::get<std::exception_ptr>(auth[0])? 'X': '?',

			std::get<bool>(auth[1]) && !std::get<std::exception_ptr>(auth[1])? ' ':
			!std::get<bool>(auth[1]) && std::get<std::exception_ptr>(auth[1])? 'X': '?',

			std::get<bool>(auth[2]) && !std::get<std::exception_ptr>(auth[2])? ' ':
			!std::get<bool>(auth[2]) && std::get<std::exception_ptr>(auth[2])? 'X': '?',
		}
	};

	const auto &type
	{
		at<"type"_>(event)
	};

	const auto &state_key
	{
		at<"state_key"_>(event)
	};

	const auto &depth
	{
		at<"depth"_>(event)
	};

	thread_local char smbuf[48];
	if(event.event_id.version() == "1")
	{
		out
		<< smalldate(smbuf, json::get<"origin_server_ts"_>(event) / 1000L)
		<< std::right << " "
		<< std::setw(9) << json::get<"depth"_>(event)
		<< std::right << " [ "
		<< std::setw(30) << type
		<< std::left << " | "
		<< std::setw(50) << state_key
		<< std::left << " ]" << flags << " "
		<< std::setw(10) << event_idx
		<< std::left << "  "
		<< std::setw(72) << string_view{event.event_id}
		<< std::left << " "
		;
	} else {
		out
		<< std::left
		<< smalldate(smbuf, json::get<"origin_server_ts"_>(event) / 1000L)
		<< ' '
		<< string_view{event.event_id}
		<< std::right << " "
		<< std::setw(9) << json::get<"depth"_>(event)
		<< std::right << " [ "
		<< std::setw(40) << type
		<< std::left << " | "
		<< std::setw(56) << state_key
		<< std::left << " ]" << flags << " "
		<< std::setw(10) << event_idx
		<< ' '
		;
	}

	if(std::get<1>(auth[0]))
		out << ":" << trunc(what(std::get<1>(auth[0])), 72);

	out << std::endl;
	return out;
}

///////////////////////////////////////////////////////////////////////////////
//
// event/horizon.h
//

//TODO: XXX remove fwd decl
namespace ircd::m::dbs
{
	void _index_event_horizon(db::txn &, const event &, const write_opts &, const m::event::id &);
}

size_t
ircd::m::event::horizon::rebuild()
{
	m::dbs::write_opts opts;
	opts.appendix.reset();
	opts.appendix.set(dbs::appendix::EVENT_HORIZON);
	db::txn txn
	{
		*dbs::events
	};

	size_t ret(0);
	m::events::for_each({0UL, -1UL}, [&ret, &txn, &opts]
	(const m::event::idx &event_idx, const m::event &event)
	{
		const m::event::prev prev
		{
			event
		};

		m::for_each(prev, [&ret, &txn, &opts, &event_idx, &event]
		(const m::event::id &event_id)
		{
			if(m::exists(event_id))
				return true;

			opts.event_idx = event_idx;
			m::dbs::_index_event_horizon(txn, event, opts, event_id);
			if(++ret % 1024 == 0)
				log::info
				{
					m::log, "event::horizon rebuild @ %lu/%lu",
					event_idx,
					m::vm::sequence::retired,
				};

			return true;
		});

		return true;
	});

	txn();
	return ret;
}

size_t
ircd::m::room::events::horizon::rebuild()
{
	m::dbs::write_opts opts;
	opts.appendix.reset();
	opts.appendix.set(dbs::appendix::EVENT_HORIZON);
	db::txn txn
	{
		*dbs::events
	};

	size_t ret(0);
	m::room::events it
	{
		room
	};

	for(; it; --it)
	{
		const m::event &event{*it};
		const event::prev prev_events{event};

		opts.event_idx = it.event_idx();
		m::for_each(prev_events, [&]
		(const m::event::id &event_id)
		{
			if(m::exists(event_id))
				return true;

			m::dbs::_index_event_horizon(txn, event, opts, event_id);
			++ret;
			return true;
		});
	}

	txn();
	return ret;
}
