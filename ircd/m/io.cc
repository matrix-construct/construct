// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/m/m.h>

namespace ircd::m::io
{
	bool acquire_local(event::fetch &);
	bool acquire_local(room::fetch &);
	bool acquire_local(room::state::fetch &);
}

///////////////////////////////////////////////////////////////////////////////
//
// m/io.h
//

struct ircd::m::io::fetch::opts
const ircd::m::io::fetch::defaults
{};

struct ircd::m::io::sync::opts
const ircd::m::io::sync::defaults
{};

ircd::json::object
ircd::m::io::get(const id::event &event_id,
                 const mutable_buffer &buf)
{
	v1::event request
	{
		event_id, buf
	};

	request.wait();

	return json::object
	{
		request
	};
}

void
ircd::m::io::release(event::sync &tab)
{
	release({&tab, 1});
	if(unlikely(bool(tab.error)))
		std::rethrow_exception(tab.error);
}

ircd::json::array
ircd::m::io::acquire(room::state::fetch &tab)
{
	acquire({&tab, 1});
	if(unlikely(bool(tab.error)))
		std::rethrow_exception(tab.error);

	return tab.pdus;
}

ircd::json::array
ircd::m::io::acquire(room::fetch &tab)
{
	acquire({&tab, 1});
	if(unlikely(bool(tab.error)))
		std::rethrow_exception(tab.error);

	return tab.pdus;
}

ircd::json::object
ircd::m::io::acquire(event::fetch &tab)
{
	acquire({&tab, 1});
	if(unlikely(bool(tab.error)))
		std::rethrow_exception(tab.error);

	return tab.pdu;
}

//
// mass release suite
//

//TODO: XXX
namespace ircd::m::io
{
	net::remote
	destination_remote(const string_view &destination)
	{
		return net::remote{destination};
	}
}

size_t
ircd::m::io::release(vector_view<event::sync> tab)
{
	const auto count
	{
		tab.size()
	};

	size_t out(0);
	std::string url[count];
	m::io::request request[count];
	struct session session[count];
	for(size_t i(0); i < count; ++i) try
	{
		if(!tab[i].destination || !tab[i].buf)
			continue;

		url[i] = fmt::snstringf
		{
			1024, "_matrix/federation/v1/send/%zu/", tab[i].txnid
		};

		session[i] =
		{
			tab[i].opts->hint? tab[i].opts->hint : destination_remote(tab[i].destination)
		};

		request[i] =
		{
			"PUT", url[i], {}, json::object{tab[i].buf}
		};

		request[i].destination = tab[i].destination;
		request[i](session[i].server);
		++out;
	}
	catch(const std::exception &e)
	{
		tab[i].error = std::make_exception_ptr(e);
		log.warning("sync txn %ld will not succeed: %s",
		            tab[i].txnid,
		            e.what());
	}

	static const size_t rbuf_size{4_KiB};
	const unique_buffer<mutable_buffer> rbuf
	{
		out * rbuf_size
	};

	size_t in(0), ret(0);
	json::object response[count];
	for(size_t i(0); i < count; ++i) try
	{
		if(!tab[i].destination)
			continue;

		if(bool(tab[i].error))
			continue;

		const mutable_buffer buf
		{
			data(rbuf) + (in * rbuf_size), rbuf_size
		};

		ircd::parse::buffer pb{buf};
		response[i] = m::io::response{session[i].server, pb};
		ret += !tab[i].error;
		++in;

		log.debug("%s received txn %ld (size: %zu) error: %d",
		          string(net::remote{session[i].server}),
		          tab[i].txnid,
		          size(tab[i].buf),
		          bool(tab[i].error));
	}
	catch(const http::error &e)
	{
		tab[i].error = std::make_exception_ptr(e);
		log.error("request to sync txn %ld failed: %s: %s",
		          tab[i].txnid,
		          e.what(),
		          e.content);
	}
	catch(const std::exception &e)
	{
		tab[i].error = std::make_exception_ptr(e);
		log.error("request to sync txn %ld failed: %s",
		          tab[i].txnid,
		          e.what());
	}

	return ret;
}

//
// mass acquire suite
//

size_t
ircd::m::io::acquire(vector_view<room::state::fetch> tab)
{
	const auto count
	{
		tab.size()
	};

	std::string url[count];
	std::string query[count];
	m::io::request request[count];
	struct session session[count];
	for(size_t i(0); i < count; ++i) try
	{
		if(!tab[i].event_id)
			continue;

		tab[i].local_result = acquire_local(tab[i]);
		if(tab[i].local_result)
			continue;

		static char tmp[768];
		url[i] = fmt::snstringf
		{
			1024, "_matrix/federation/v1/state/%s/", url::encode(tab[i].room_id, tmp)
		};

		query[i] = fmt::snstringf
		{
			1024, "event_id=%s", url::encode(tab[i].event_id, tmp)
		};

		request[i] =
		{
			"GET", url[i], query[i], json::object{}
		};

		session[i] =
		{
			tab[i].opts->hint? tab[i].opts->hint : tab[i].event_id.hostname()
		};

		request[i].destination = session[i].destination;
		request[i](session[i].server);
	}
	catch(const std::exception &e)
	{
		tab[i].error = std::make_exception_ptr(e);
		log.warning("request for event_id %s in room_id %s will not succeed: %s",
		            string_view{tab[i].event_id},
		            string_view{tab[i].room_id},
		            e.what());
	}

	size_t ret(0);
	json::object response[count];
	for(size_t i(0); i < count; ++i) try
	{
		if(!tab[i].event_id)
			continue;

		if(tab[i].local_result || bool(tab[i].error))
			continue;

		ircd::parse::buffer pb{tab[i].buf};
		response[i] = m::io::response(session[i].server, pb);
		tab[i].auth_chain = response[i]["auth_chain"];
		tab[i].pdus = response[i]["pdus"];
		//TODO: check event id
		//TODO: check hashes
		//TODO: check signatures
		ret += !tab[i].error;

		log.debug("%s sent us event %s in room %s pdus: %zu (size: %zu) error: %d",
		          string(net::remote{session[i].server}),
		          string_view{tab[i].event_id},
		          string_view{tab[i].room_id},
		          json::array{response[i]["pdus"]}.count(),
		          string_view{response[i]}.size(),
		          bool(tab[i].error));
	}
	catch(const std::exception &e)
	{
		tab[i].error = std::make_exception_ptr(e);
		log.warning("request for event_id %s failed: %s",
		            string_view{tab[i].event_id},
		            e.what());
	}

	return ret;
}

size_t
ircd::m::io::acquire(vector_view<room::fetch> tab)
{
	const auto count
	{
		tab.size()
	};

	std::string url[count];
	std::string query[count];
	m::io::request request[count];
	struct session session[count];
	for(size_t i(0); i < count; ++i) try
	{
		if(!tab[i].event_id)
			continue;

		tab[i].local_result = acquire_local(tab[i]);
		if(tab[i].local_result)
			continue;

		static char tmp[768];
		url[i] = fmt::snstringf
		{
			1024, "_matrix/federation/v1/backfill/%s/", url::encode(tab[i].room_id, tmp)
		};

		query[i] = fmt::snstringf
		{
			1024, "limit=%zu&v=%s", tab[i].opts->limit, url::encode(tab[i].event_id, tmp)
		};

		session[i] =
		{
			tab[i].opts->hint? tab[i].opts->hint : tab[i].event_id.hostname()
		};

		request[i] =
		{
			"GET", url[i], query[i], {}
		};

		request[i].destination = session[i].destination;
		request[i](session[i].server);
	}
	catch(const std::exception &e)
	{
		tab[i].error = std::make_exception_ptr(e);
		log.warning("request for event_id %s in room_id %s will not succeed: %s",
		            string_view{tab[i].event_id},
		            string_view{tab[i].room_id},
		            e.what());
	}

	size_t ret(0);
	json::object response[count];
	for(size_t i(0); i < count; ++i) try
	{
		if(!tab[i].event_id)
			continue;

		if(tab[i].local_result || bool(tab[i].error))
			continue;

		ircd::parse::buffer pb{tab[i].buf};
		response[i] = m::io::response(session[i].server, pb);
		tab[i].auth_chain = response[i]["auth_chain"];
		tab[i].pdus = response[i]["pdus"];
		//TODO: check event id
		//TODO: check hashes
		//TODO: check signatures
		ret += !tab[i].error;

		log.debug("%s sent us event %s in room %s pdus: %zu (size: %zu) error: %d",
		          string(net::remote{session[i].server}),
		          string_view{tab[i].event_id},
		          string_view{tab[i].room_id},
		          json::array{response[i]["pdus"]}.count(),
		          string_view{response[i]}.size(),
		          bool(tab[i].error));
	}
	catch(const std::exception &e)
	{
		tab[i].error = std::make_exception_ptr(e);
		log.warning("request for event_id %s failed: %s",
		            string_view{tab[i].event_id},
		            e.what());
	}

	return ret;
}

size_t
ircd::m::io::acquire(vector_view<event::fetch> tab)
{
	const auto count
	{
		tab.size()
	};

	std::string url[count];
	m::io::request request[count];
	struct session session[count];
	for(size_t i(0); i < count; ++i) try
	{
		if(!tab[i].event_id)
			continue;

		tab[i].local_result = acquire_local(tab[i]);
		if(tab[i].local_result)
			continue;

		static char tmp[768];
		url[i] = fmt::snstringf
		{
			1024, "_matrix/federation/v1/event/%s/", url::encode(tab[i].event_id, tmp)
		};

		session[i] =
		{
			tab[i].opts->hint? tab[i].opts->hint : tab[i].event_id.hostname()
		};

		request[i] =
		{
			"GET", url[i], {}, {}
		};

		request[i].destination = session[i].destination;
		request[i](session[i].server);
	}
	catch(const std::exception &e)
	{
		tab[i].error = std::make_exception_ptr(e);
		log.warning("request for event_id %s will not succeed: %s",
		            string_view{tab[i].event_id},
		            e.what());
	}

	size_t ret(0);
	json::object response[count];
	for(size_t i(0); i < count; ++i) try
	{
		if(!tab[i].event_id)
			continue;

		if(tab[i].local_result || bool(tab[i].error))
			continue;

		ircd::parse::buffer pb{tab[i].buf};
		response[i] = m::io::response{session[i].server, pb};
		tab[i].pdu = json::array{response[i]["pdus"]}[0];
		//TODO: check event id
		//TODO: check hashes
		//TODO: check signatures
		ret += !tab[i].error;

		log.debug("%s sent us event %s pdus: %zu (size: %zu) error: %d",
		          string(net::remote{session[i].server}),
		          string_view{tab[i].event_id},
		          json::array{response[i]["pdus"]}.count(),
		          string_view{response[i]}.size(),
		          bool(tab[i].error));
	}
	catch(const std::exception &e)
	{
		tab[i].error = std::make_exception_ptr(e);
		log.warning("request for event_id %s failed: %s",
		            string_view{tab[i].event_id},
		            e.what());
	}

	return ret;
}

//
// acquire_local suite.
//

bool
ircd::m::io::acquire_local(room::state::fetch &tab)
try
{
	return false;
}
catch(const std::exception &e)
{
	tab.error = std::make_exception_ptr(e);
	return false;
}

bool
ircd::m::io::acquire_local(room::fetch &tab)
try
{
	return false;
}
catch(const std::exception &e)
{
	tab.error = std::make_exception_ptr(e);
	return false;
}

bool
ircd::m::io::acquire_local(event::fetch &tab)
try
{
/*
	const m::vm::query<m::vm::where::equal> query
	{
		{ "event_id", tab.event_id },
	};

	const auto test{[&tab](const auto &event)
	{
		tab.pdu = stringify(mutable_buffer{tab.buf}, event);
		return true;
	}};

	return m::vm::test(query, test);
*/
	return false;
}
catch(const std::exception &e)
{
	tab.error = std::make_exception_ptr(e);
	return false;
}
