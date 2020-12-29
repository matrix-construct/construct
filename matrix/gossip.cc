// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

template<>
decltype(ircd::util::instance_list<ircd::m::gossip>::allocator)
ircd::util::instance_list<ircd::m::gossip>::allocator
{};

template<>
decltype(ircd::util::instance_list<ircd::m::gossip>::list)
ircd::util::instance_list<ircd::m::gossip>::list
{
	allocator
};

decltype(ircd::m::gossip::log)
ircd::m::gossip::log
{
	"m.gossip"
};

ircd::m::gossip::gossip::gossip(const struct opts &opts)
:opts{opts}
{
	for(size_t i(0); i < opts.rounds; ++i)
		if(!gossip_head())
			break;
}

ircd::m::gossip::~gossip()
noexcept try
{
	while(!requests.empty())
		while(handle());
}
catch(const ctx::interrupted &)
{
	return;
}
catch(const ctx::terminated &)
{
	return;
}

bool
ircd::m::gossip::gossip_head()
{
	bool ret
	{
		false
	};

	if(opts.hint && opts.hint_only && opts.room.event_id)
	{
		m::event result;
		result.event_id = opts.room.event_id;
		json::get<"origin"_>(result) = opts.hint;
		ret |= handle_head(result);
		return ret;
	}

	if(opts.hint && opts.hint_only)
	{
		const unique_mutable_buffer buf
		{
			16_KiB
		};

		const auto event
		{
			m::room::head::fetch::one(buf, opts.room, opts.hint)
		};

		m::for_each(event::prev{event}, [this, &ret]
		(const event::id &event_id)
		{
			m::event result;
			json::get<"origin"_>(result) = opts.hint;
			result.event_id = event_id;
			ret |= handle_head(result);
			return true;
		});

		return ret;
	}

	m::room::head::fetch::opts hfopts;
	hfopts.room_id = opts.room.room_id;
	hfopts.top = m::top(opts.room.room_id);
	hfopts.existing = true;
	m::room::head::fetch
	{
		hfopts, [this, &ret]
		(const m::event &result)
		{
			ret |= handle_head(result);
			return true;
		}
	};

	return ret;
}

bool
ircd::m::gossip::handle_head(const m::event &result)
{
	const auto &remote
	{
		json::get<"origin"_>(result)
	};

	const bool submitted
	{
		submit(result.event_id, remote)
	};

	return submitted;
}

bool
ircd::m::gossip::submit(const m::event::id &event_id,
                        const string_view &remote)
{
	const auto hash
	{
		(uint128_t(ircd::hash(event_id)))
		| (uint128_t(ircd::hash(remote)) << 64)
	};

	auto it
	{
		attempts.lower_bound(hash)
	};

	const bool exists
	{
		it != end(attempts) && *it == hash
	};

	if(!exists)
		it = attempts.emplace_hint(it, hash);

	const bool submitted
	{
		!exists && !started(event_id, remote)?
			start(event_id, remote):
			false
	};

	if(submitted || full())
		while(handle());

	return submitted;
}

bool
ircd::m::gossip::start(const m::event::id &event_id_,
                       const string_view &remote_)
try
{
	static const size_t max
	{
		48
	};

	const auto event_idx_
	{
		m::index(std::nothrow, event_id_)
	};

	const m::event::refs refs
	{
		event_idx_
	};

	size_t num{0}, i{0};
	std::array<event::idx, max> next_idx;
	refs.for_each(dbs::ref::NEXT, [this, &next_idx, &num]
	(const event::idx &event_idx, const auto &ref_type)
	{
		assert(ref_type == dbs::ref::NEXT);
		if(event_idx < opts.ref.first || event_idx > opts.ref.second)
			return true;

		next_idx.at(num) = event_idx;
		return ++num < next_idx.size();
	});

	if(!num)
		return false;

	unique_mutable_buffer _buf
	{
		(event::MAX_SIZE * num) + 16_KiB
	};

	mutable_buffer buf{_buf};
	json::stack out{buf};
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
				ircd::time<milliseconds>()
			}
		};

		json::stack::array pdus
		{
			top, "pdus"
		};

		m::event::fetch event;
		for(i = 0; i < num; ++i)
		{
			if(!seek(std::nothrow, event, next_idx.at(i)))
				continue;

			pdus.append(event.source);
			log::debug
			{
				log, "Gossip %zu/%zu in %s for %s to '%s' %s idx:%lu",
				i,
				num,
				string_view{opts.room.room_id},
				string_view{event_id_},
				remote_,
				string_view{event.event_id},
				event.event_idx,
			};
		}
	}

	if(!i)
		return false;

	const string_view txn
	{
		out.completed()
	};

	consume(buf, size(txn));
	const string_view txnid
	{
		m::txn::create_id(buf, txn)
	};

	consume(buf, size(txnid));
	const string_view remote
	{
		strlcpy{buf, remote_}
	};

	consume(buf, size(remote));
	const string_view event_id
	{
		strlcpy{buf, event_id_}
	};

	consume(buf, size(event_id));
	assert(!empty(buf));

	char pbuf[48];
	log::debug
	{
		log, "Gossip %zu/%zu in %s for %s to '%s' txn[%s] %s",
		i,
		num,
		string_view{opts.room.room_id},
		string_view{event_id_},
		remote_,
		txnid,
		pretty(pbuf, iec(size(txn))),
	};

	m::fed::send::opts fedopts;
	fedopts.remote = remote;
	requests.emplace_back(result
	{
		std::move(_buf),
		txn,
		txnid,
		remote,
		event_id,
		m::fed::send
		{
			txnid,
			txn,
			buf,
			std::move(fedopts)
		}
	});

	return true;
}
catch(const ctx::interrupted &e)
{
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Gossip %s in %s from '%s' :%s",
		string_view{event_id_},
		string_view{opts.room.room_id},
		remote_,
		e.what(),
	};

	return false;
}

bool
ircd::m::gossip::handle()
{
	if(requests.empty())
		return false;

	auto next
	{
		ctx::when_any(std::begin(requests), std::end(requests), []
		(auto &it) -> ctx::future<http::code> &
		{
			return it->request;
		})
	};

	const milliseconds timeout
	{
		full()? 5000: 50
	};

	ctx::interruption_point();
	if(!next.wait(timeout, std::nothrow))
		return full();

	const unique_iterator it
	{
		requests, next.get()
	};

	assert(it.it != std::end(requests));
	return handle(*it.it);
}

bool
ircd::m::gossip::handle(result &result)
try
{
	auto response
	{
		result.request.get()
	};

	const json::object body
	{
		result.request
	};

	fed::send::response{body}.for_each_pdu([&]
	(const event::id &event_id, const json::object &errors)
	{
		const bool ok
		{
			empty(errors)
		};

		log::logf
		{
			log, ok? log::level::DEBUG: log::level::DERROR,
			"Gossip %s in %s to '%s'%s%s",
			string_view{event_id},
			string_view{opts.room.room_id},
			string_view{result.remote},
			!ok? " :"_sv: " "_sv,
			string_view{errors},
		};
	});

	return true;
}
catch(const ctx::interrupted &e)
{
	throw;
}
catch(const std::exception &e)
{
	log::logf
	{
		log, log::level::DERROR,
		"Gossip %s in %s to '%s' :%s",
		string_view{result.event_id},
		string_view{opts.room.room_id},
		string_view{result.remote},
		e.what(),
	};

	return true;
}

bool
ircd::m::gossip::started(const event::id &event_id,
                         const string_view &remote)
const
{
	const auto it
	{
		std::find_if(std::begin(requests), std::end(requests), [&]
		(const auto &result)
		{
			return result.event_id == event_id && result.remote == remote;
		})
	};

	return it != std::end(requests);
}

bool
ircd::m::gossip::full()
const noexcept
{
	return requests.size() >= opts.width;
}
