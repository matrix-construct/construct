// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "sender.int.h"


std::list<txn> txns;
std::map<std::string, node, std::less<>> nodes;

static void recv_timeout(txn &);
static void recv_timeouts();
static bool recv_handle(txn &, node &);
static void recv();
static void recv_worker();
ctx::dock recv_action;

static void send(const m::event &, const m::room::id &room_id);
static void send(const m::event &);
static void send();
static void send_worker();

context
sender
{
	"fedsnd S", 1_MiB, &send_worker, context::POST,
};

context
receiver
{
	"fedsnd R", 1_MiB, &recv_worker, context::POST,
};

mapi::header
IRCD_MODULE
{
	"federation sender",
	nullptr, []
	{
		sender.interrupt();
		receiver.interrupt();
		sender.join();
		receiver.join();
	}
};

void
send_worker()
try
{
	while(1)
		send();
}
catch(const ircd::ctx::interrupted &e)
{
	ircd::log::debug
	{
		"Sender worker interrupted..."
	};
}

void
send()
try
{
	std::unique_lock<decltype(m::vm::accept)> lock
	{
		m::vm::accept
	};

	// reference to the event on the inserter's stack
	const auto &event
	{
		m::vm::accept.wait(lock)
	};

	if(my(event))
		send(event);
}
catch(const ircd::ctx::interrupted &e)
{
	throw;
}
catch(const std::exception &e)
{
	ircd::log::error
	{
		"sender worker: %s", e.what()
	};
}

void
send(const m::event &event)
{
	const auto &room_id
	{
		json::get<"room_id"_>(event)
	};

	if(room_id)
		send(event, room_id);
}

void
send(const m::event &event,
     const m::room::id &room_id)
{
	// Unit is not allocated until we find another server in the room.
	std::shared_ptr<struct unit> unit;

	const m::room room{room_id};
	const m::room::origins origins{room};
	origins.for_each([&unit, &event]
	(const string_view &origin)
	{
		if(my_host(origin))
			return;

		auto it{nodes.lower_bound(origin)};
		if(it == end(nodes) || it->first != origin)
			it = nodes.emplace_hint(it, origin, origin);

		auto &node{it->second};
		if(!unit)
			unit = std::make_shared<struct unit>(event);

		node.push(unit);
	});
}

void
node::push(std::shared_ptr<unit> su)
{
	q.emplace_back(std::move(su));
	flush();
}

bool
node::flush()
try
{
	if(q.empty())
		return false;

	if(curtxn)
		return false;

	const auto pdus
	{
		std::count_if(begin(q), end(q), [](const auto &p)
		{
			return p->type == unit::PDU;
		})
	};

	std::vector<json::value> pdu(pdus);
	for(ssize_t i(0); i < pdus; ++i)
		pdu.at(i) = string_view{q.at(i)->s};

	std::string content
	{
		m::txn::create({pdu.data(), pdu.size()}, {})
	};

	m::v1::send::opts opts;
	opts.remote = id.host();
	opts.sopts = &sopts;
	txns.emplace_back(*this, std::move(content), std::move(opts));
	q.clear();
	recv_action.notify_one();
	return true;
}
catch(const std::exception &e)
{
	log::error
	{
		"flush error to %s :%s", string_view{id}, e.what()
	};

	return false;
}

void
recv_worker()
try
{
	while(1)
	{
		recv_action.wait([]
		{
			return !txns.empty();
		});

		recv();
		recv_timeouts();
	}
}
catch(const ircd::ctx::interrupted &e)
{
	log::debug
	{
		"Receive worker interrupted..."
	};
}

void
recv()
try
{
	auto next
	{
		ctx::when_any(begin(txns), end(txns))
	};

	if(next.wait(seconds(2), std::nothrow) == ctx::future_status::timeout)   //TODO: conf
		return;

	const auto it
	{
		next.get()
	};

	if(unlikely(it == end(txns)))
	{
		assert(0);
		return;
	}

	auto &txn
	{
		*it
	};

	assert(txn.node);
	auto &node{*txn.node};
	const auto ret
	{
		recv_handle(txn, node)
	};

	node.curtxn = nullptr;
	txns.erase(it);

	if(ret)
		node.flush();
}
catch(const ctx::interrupted &e)
{
	throw;
}
catch(const std::exception &e)
{
	ircd::assertion(e);
	return;
}

bool
recv_handle(txn &txn,
            node &node)
try
{
	const auto code
	{
		txn.get()
	};

	const json::object obj
	{
		txn
	};

	const m::v1::send::response resp
	{
		obj
	};

	log::debug
	{
		"%u %s from %s for %s",
		ushort(code),
		http::status(code),
		string_view{node.id},
		txn.txnid
	};

	resp.for_each_pdu([&txn, &node]
	(const m::event::id &event_id, const json::object &error)
	{
		if(empty(error))
			return;

		log::error
		{
			"Error from %s in %s for %s :%s",
			string_view{node.id},
			txn.txnid,
			string_view{event_id},
			string_view{error}
		};
	});

	return true;
}
catch(const http::error &e)
{
	log::error
	{
		"%u %s from %s for %s :%s",
		ushort(e.code),
		http::status(e.code),
		string_view{node.id},
		txn.txnid,
		e.what()
	};

	return false;
}
catch(const std::exception &e)
{
	log::error
	{
		"Error from %s for %s :%s",
		string_view{node.id},
		txn.txnid,
		e.what()
	};

	return false;
}

void
recv_timeouts()
{
	const auto &now
	{
		ircd::now<steady_point>()
	};

	auto it(begin(txns));
	while(it != end(txns))
	{
		auto &txn(*it);
		if(txn.timeout + seconds(15) < now) //TODO: conf
		{
			recv_timeout(txn);
			it = txns.erase(it);
		}
		else ++it;
	}
}

void
recv_timeout(txn &txn)
{
	assert(txn.node);
	auto &node(*txn.node);

	log::warning
	{
		"Timeout to %s for txn %s",
		string_view{node.id},
		txn.txnid
	};

	node.curtxn = nullptr;
}
