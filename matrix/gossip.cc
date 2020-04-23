// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::gossip::log)
ircd::m::gossip::log
{
	"m.gossip"
};

/// Initial gossip protocol works by sending the remote server some events which
/// reference an event contained in the remote's head which we just obtained.
/// This is part of a family of active measures taken to reduce forward
/// extremities on other servers but without polluting the chain with
/// permanent data for this purpose such as with org.matrix.dummy_event.
ircd::m::gossip::gossip::gossip(const room::id &room_id,
                                const opts &opts)
{
	assert(opts.event_id);
	const auto &event_id
	{
		opts.event_id
	};

	const m::event::refs refs
	{
		m::index(std::nothrow, event_id)
	};

	static const size_t max{48};
	const size_t count
	{
		std::min(refs.count(dbs::ref::NEXT), max)
	};

	if(!count)
		return;

	const unique_mutable_buffer buf[]
	{
		{ event::MAX_SIZE * (count + 1)  },
		{ 16_KiB                         },
	};

	size_t i{0};
	std::array<event::idx, max> next_idx;
	refs.for_each(dbs::ref::NEXT, [&next_idx, &i]
	(const event::idx &event_idx, const auto &ref_type)
	{
		assert(ref_type == dbs::ref::NEXT);
		next_idx.at(i) = event_idx;
		return ++i < next_idx.size();
	});

	size_t ret{0};
	json::stack out{buf[0]};
	{
		json::stack::object top
		{
			out
		};

		json::stack::member
		{
			top, "origin", m::my_host()
		};

		json::stack::member
		{
			top, "origin_server_ts", json::value
			{
				long(ircd::time<milliseconds>())
			}
		};

		json::stack::array pdus
		{
			top, "pdus"
		};

		m::event::fetch event;
		for(assert(ret == 0); ret < i; ++ret)
			if(seek(std::nothrow, event, next_idx.at(ret)))
				pdus.append(event.source);
	}

	const string_view txn
	{
		out.completed()
	};

	char idbuf[64];
	const string_view txnid
	{
		m::txn::create_id(idbuf, txn)
	};

	m::fed::send::opts fedopts;
	fedopts.remote = opts.remote;
	m::fed::send request
	{
		txnid, txn, buf[1], std::move(fedopts)
	};

	http::code code{0};
	std::exception_ptr eptr;
	if(request.wait(opts.timeout, std::nothrow)) try
	{
		code = request.get();
		ret += code == http::OK;
	}
	catch(...)
	{
		eptr = std::current_exception();
	}

	log::logf
	{
		log, code == http::OK? log::DEBUG : log::DERROR,
		"gossip %zu:%zu to %s reference to %s in %s :%s %s",
		ret,
		count,
		opts.remote,
		string_view{event_id},
		string_view{room_id},
		code?
			status(code):
			"failed",
		eptr?
			what(eptr):
			string_view{},
	};
}
