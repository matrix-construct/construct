// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "net_dns.h"

decltype(ircd::net::dns::resolver_instance)
ircd::net::dns::resolver_instance;

decltype(ircd::net::dns::resolver::servers)
ircd::net::dns::resolver::servers
{
	{
		{ "name",     "ircd.net.dns.resolver.servers"                    },
		{ "default",   "4.2.2.1 4.2.2.2 4.2.2.3 4.2.2.4 4.2.2.5 4.2.2.6" },
	}, []
	{
		if(bool(ircd::net::dns::resolver_instance))
			ircd::net::dns::resolver_instance->set_servers();
	}
};

decltype(ircd::net::dns::resolver::timeout)
ircd::net::dns::resolver::timeout
{
	{ "name",     "ircd.net.dns.resolver.timeout" },
	{ "default",   10000L                         },
};

decltype(ircd::net::dns::resolver::send_rate)
ircd::net::dns::resolver::send_rate
{
	{ "name",     "ircd.net.dns.resolver.send_rate" },
	{ "default",   60L                              },
};

decltype(ircd::net::dns::resolver::send_burst)
ircd::net::dns::resolver::send_burst
{
	{ "name",     "ircd.net.dns.resolver.send_burst" },
	{ "default",   8L                                },
};

decltype(ircd::net::dns::resolver::retry_max)
ircd::net::dns::resolver::retry_max
{
	{ "name",     "ircd.net.dns.resolver.retry_max" },
	{ "default",   4L                               },
};

//
// interface
//

void
ircd::net::dns::resolver_init(answers_callback callback)
{
	assert(!ircd::net::dns::resolver_instance);

	ircd::net::dns::resolver_instance = new resolver
	{
		std::move(callback)
	};
}

void
ircd::net::dns::resolver_fini()
{
	delete ircd::net::dns::resolver_instance;
	ircd::net::dns::resolver_instance = nullptr;
}

uint16_t
ircd::net::dns::resolver_call(const hostport &hp,
                              const opts &opts)
{
	if(unlikely(!resolver_instance))
		throw error
		{
			"Cannot resolve '%s': resolver unavailable.",
			host(hp)
		};

	auto &resolver{*dns::resolver_instance};
	if(unlikely(!resolver.ns.is_open()))
		throw error
		{
			"Cannot resolve '%s': resolver is closed.",
			host(hp)
		};

	return resolver(hp, opts);
}

//
// resolver::resolver
//

ircd::net::dns::resolver::resolver(answers_callback callback)
:callback
{
	std::move(callback)
}
,ns
{
	ios::get()
}
,recv_context
{
	"dnsres R", 1_MiB, std::bind(&resolver::recv_worker, this), context::POST
}
,timeout_context
{
	"dnsres T", 64_KiB, std::bind(&resolver::timeout_worker, this), context::POST
}
,sendq_context
{
	"dnsres S", 64_KiB, std::bind(&resolver::sendq_worker, this), context::POST
}
{
	ns.open(ip::udp::v4());
	ns.non_blocking(true);
	set_servers();
}

ircd::net::dns::resolver::~resolver()
noexcept
{
	if(ns.is_open())
		ns.close();

	done.wait([this]
	{
		const bool ret(tags.empty());
		if(!ret)
			log::warning
			{
				log, "Waiting for %zu unfinished DNS resolutions",
				tags.size()
			};

		return ret;
	});

	timeout_context.terminate();
	sendq_context.terminate();
	recv_context.terminate();

	assert(tags.empty());
}

/// Internal resolver entry interface.
uint16_t
ircd::net::dns::resolver::operator()(const hostport &hp,
                                     const opts &opts)
{
	auto &tag(set_tag(hp, opts)); try
	{
		tag.question = make_query(tag.qbuf, tag);
		submit(tag);
	}
	catch(...)
	{
		remove(tag);
		throw;
	}

	return tag.id;
}

ircd::const_buffer
ircd::net::dns::resolver::make_query(const mutable_buffer &buf,
                                     tag &tag)
{
	thread_local char hostbuf[rfc1035::NAME_BUFSIZE * 2];
	string_view hoststr;
	switch(tag.opts.qtype)
	{
		case 0: throw error
		{
			"A query type is required to form a question."
		};

		case 33: // SRV
			hoststr = make_SRV_key(hostbuf, host(tag.hp), tag.opts);
			break;

		default:
			hoststr = host(tag.hp);
			break;
	}

	assert(hoststr);
	assert(tag.opts.qtype);
	const rfc1035::question question
	{
		hoststr, tag.opts.qtype
	};

	return rfc1035::make_query(buf, tag.id, question);
}

template<class... A>
ircd::net::dns::tag &
ircd::net::dns::resolver::set_tag(A&&... args)
{
	while(tags.size() < 65535)
	{
		auto id
		{
			ircd::rand::integer(1, 65535)
		};

		auto it{tags.lower_bound(id)};
		if(it != end(tags) && it->first == id)
			continue;

		it = tags.emplace_hint(it,
		                       std::piecewise_construct,
		                       std::forward_as_tuple(id),
		                       std::forward_as_tuple(std::forward<A>(args)...));
		it->second.id = id;
		return it->second;
	}

	throw panic
	{
		"Too many DNS queries"
	};
}

void
__attribute__((noreturn))
ircd::net::dns::resolver::sendq_worker()
{
	while(1)
	{
		dock.wait([this]
		{
			assert(sendq.empty() || !tags.empty());
			return !sendq.empty() && !server.empty();
		});

		if(tags.size() > size_t(send_burst))
			ctx::sleep(milliseconds(send_rate));

		sendq_work();
	}
}

void
ircd::net::dns::resolver::sendq_work()
{
	const std::lock_guard lock
	{
		mutex
	};

	assert(!sendq.empty());
	assert(sendq.size() < 65535);
	assert(sendq.size() <= tags.size());
	const uint16_t next(sendq.front());
	sendq.pop_front();
	flush(next);
}

void
ircd::net::dns::resolver::flush(const uint16_t &next)
try
{
	auto &tag
	{
		tags.at(next)
	};

	submit(tag);
}
catch(const std::out_of_range &e)
{
	log::error
	{
		log, "Queued tag id[%u] is no longer mapped", next
	};
}

void
ircd::net::dns::resolver::timeout_worker()
try
{
	while(1)
	{
		dock.wait([this]
		{
			return !tags.empty();
		});

		ctx::sleep(milliseconds(timeout) / 2);
		check_timeouts(milliseconds(timeout));
	}
}
catch(const ctx::terminated &)
{
	const ctx::exception_handler eh;
	cancel_all();
}

void
ircd::net::dns::resolver::check_timeouts(const milliseconds &timeout)
{
	const std::lock_guard lock
	{
		mutex
	};

	const auto cutoff
	{
		now<steady_point>() - timeout
	};

	auto it(begin(tags));
	while(it != end(tags))
	{
		const auto &id(it->first);
		auto &tag(it->second);
		if(check_timeout(id, tag, cutoff))
			it = remove(tag, it);
		else
			++it;
	}
}

bool
ircd::net::dns::resolver::check_timeout(const uint16_t &id,
                                        tag &tag,
                                        const steady_point &cutoff)
{
	if(tag.last == steady_point::min())
		return false;

	if(tag.last > cutoff)
		return false;

	log::warning
	{
		log, "DNS timeout id:%u on attempt %u of %u '%s'",
		id,
		tag.tries,
		size_t(retry_max),
		host(tag.hp)
	};

	tag.last = steady_point::min();
	if(tag.tries < size_t(retry_max))
	{
		submit(tag);
		return false;
	}

	static const std::system_error ec
	{
		make_error_code(std::errc::timed_out)
	};

	error_one(tag, ec, false);
	return true;
}

//
// submit
//

void
ircd::net::dns::resolver::submit(tag &tag)
{
	if(!ns.is_open() || server.empty())
	{
		log::warning
		{
			log, "dns tag:%u submit queued because no nameserver is available.",
			tag.id
		};

		queue_query(tag);
		return;
	}

	assert(!server.empty());
	const auto rate(milliseconds(send_rate) / server.size());
	const auto elapsed(now<steady_point>() - send_last);
	if(elapsed >= rate || tags.size() <= size_t(send_burst))
		send_query(tag);
	else
		queue_query(tag);

	dock.notify_all();
}

void
ircd::net::dns::resolver::send_query(tag &tag)
try
{
	assert(!server.empty());
	++server_next %= server.size();
	const auto &ep
	{
		server.at(server_next)
	};

	send_query(ep, tag);

	#ifdef RB_DEBUG
	thread_local char buf[128];
	log::debug
	{
		log, "dns %s send tag:%u t:%u qtype:%u `%s'",
		string(buf, make_ipport(ep)),
		tag.id,
		tag.tries,
		tag.opts.qtype,
		host(tag.hp)
	};
	#endif
}
catch(const std::out_of_range &)
{
	throw error
	{
		"No DNS servers available for query"
	};
}

void
ircd::net::dns::resolver::queue_query(tag &tag)
{
	assert(sendq.size() <= tags.size());
	if(std::find(begin(sendq), end(sendq), tag.id) != end(sendq))
		return;

	sendq.emplace_back(tag.id);

	log::debug
	{
		log, "dns tag:%u t:%u qtype:%u added to sendq (tags:%zu sendq:%zu)",
		tag.id,
		tag.tries,
		tag.opts.qtype,
		tags.size(),
		sendq.size()
	};
}

void
ircd::net::dns::resolver::send_query(const ip::udp::endpoint &ep,
                                     tag &tag)
{
	assert(ns.is_open());
	assert(ns.non_blocking());
	assert(!empty(tag.question));
	const const_buffer &buf{tag.question};
	const auto sent
	{
		ns.send_to(asio::const_buffers_1(buf), ep)
	};

	send_last = now<steady_point>();
	tag.last = send_last;
	tag.server = make_ipport(ep);
	tag.tries++;
}

//
// recv
//

void
ircd::net::dns::resolver::recv_worker()
try
{
	const unique_buffer<mutable_buffer> buf
	{
		64_KiB
	};

	const auto interruption{[this]
	(ctx::ctx *const &)
	{
		if(this->ns.is_open())
			this->ns.cancel();
	}};

	const asio::mutable_buffers_1 bufs{buf};
	ip::udp::endpoint ep;
	while(ns.is_open()) try
	{
		size_t recv; continuation
		{
			continuation::asio_predicate, interruption, [this, &bufs, &recv, &ep]
			(auto &yield)
			{
				recv = ns.async_receive_from(bufs, ep, yield);
			}
		};

		const mutable_buffer &reply
		{
			data(buf), recv
		};

		const net::ipport &from
		{
			make_ipport(ep)
		};

		handle(from, reply);
	}
	catch(const boost::system::system_error &e)
	{
		switch(make_error_code(e).value())
		{
			case int(std::errc::operation_canceled):
				break;

			default:
				throw;
		}
	}
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "%s", e.what()
	};
}

void
ircd::net::dns::resolver::handle(const ipport &from,
                                 const mutable_buffer &buf)
try
{
	if(unlikely(size(buf) < sizeof(rfc1035::header)))
		throw rfc1035::error
		{
			"Got back %zu bytes < rfc1035 %zu byte header",
			size(buf),
			sizeof(rfc1035::header)
		};

	rfc1035::header &header
	{
		*reinterpret_cast<rfc1035::header *>(data(buf))
	};

	ntoh(&header.qdcount);
	ntoh(&header.ancount);
	ntoh(&header.nscount);
	ntoh(&header.arcount);

	const const_buffer body
	{
		data(buf) + sizeof(header), size(buf) - sizeof(header)
	};

	handle_reply(from, header, body);
}
catch(const std::exception &e)
{
	log::error
	{
		log, "%s", e.what()
	};
}

void
ircd::net::dns::resolver::handle_reply(const ipport &from,
                                       const header &header,
                                       const const_buffer &body)
{
	thread_local char addr_strbuf[2][128];
	const std::lock_guard lock
	{
		// The primary mutex is locked here while this result is
		// processed. This locks out the sendq and timeout worker.
		mutex
	};

	const auto it
	{
		tags.find(header.id)
	};

	if(it == end(tags))
		throw error
		{
			"DNS reply from %s for unrecognized tag id:%u",
			string(addr_strbuf[0], from),
			header.id
		};

	auto &tag{it->second};
	if(from != tag.server)
		throw error
		{
			"DNS reply from %s for tag:%u which we sent to %s",
			string(addr_strbuf[0], from),
			header.id,
			string(addr_strbuf[1], tag.server)
		};

	log::debug
	{
		log, "dns %s recv tag:%u t:%u qtype:%u qd:%u an:%u ns:%u ar:%u",
		string(addr_strbuf[0], from),
		tag.id,
		tag.tries,
		tag.opts.qtype,
		header.qdcount,
		header.ancount,
		header.nscount,
		header.arcount,
	};

	// Handle ServFail as a special case here. We can try again without
	// handling this tag or propagating this error any further yet.
	if(header.rcode == 2 && tag.tries < size_t(retry_max))
	{
		log::error
		{
			log, "dns %s recv tag:%u t:%u qtype:%u protocol error #%u :%s",
			string(addr_strbuf[0], from),
			tag.id,
			tag.tries,
			tag.opts.qtype,
			header.rcode,
			rfc1035::rcode.at(header.rcode)
		};

		assert(tag.tries > 0);
		tag.last = steady_point::min();
		submit(tag);
		return;
	}

	// The tag is committed to being handled after this point. It will be
	// removed from the tags map and any retry must c
	const unwind untag{[this, &tag, &it]
	{
		remove(tag, it);
	}};

	assert(tag.tries > 0);
	tag.last = steady_point::min();
	tag.rcode = header.rcode;
	handle_reply(header, body, tag);
}

void
ircd::net::dns::resolver::handle_reply(const header &header,
                                       const const_buffer &body,
                                       tag &tag)
try
{
	if(unlikely(header.qr != 1))
		throw rfc1035::error
		{
			"Response header is marked as 'Query' and not 'Response'"
		};

	if(header.qdcount > MAX_COUNT || header.ancount > MAX_COUNT)
		throw error
		{
			"Response contains too many sections..."
		};

	if(header.qdcount < 1)
		throw error
		{
			"Response does not contain the question."
		};

	if(!handle_error(header, tag))
		throw rfc1035::error
		{
			"protocol error #%u :%s",
			header.rcode,
			rfc1035::rcode.at(header.rcode)
		};

	const_buffer buffer
	{
		body
	};

	// Questions are regurgitated back to us so they must be parsed first
	thread_local std::array<rfc1035::question, MAX_COUNT> qd;
	for(size_t i(0); i < header.qdcount; ++i)
		consume(buffer, size(qd.at(i).parse(buffer)));

	// Answers are parsed into this buffer
	thread_local std::array<rfc1035::answer, MAX_COUNT> an;
	for(size_t i(0); i < header.ancount; ++i)
		consume(buffer, size(an.at(i).parse(buffer)));

	const vector_view<const rfc1035::answer> answers
	{
		an.data(), header.ancount
	};

	callback({}, tag, answers);
}
catch(const std::exception &e)
{
	// There's no need to flash red to the log for NXDOMAIN which is
	// common in this system when probing SRV.
	const auto level
	{
		header.rcode != 3? log::ERROR : log::DERROR
	};

	log::logf
	{
		log, level, "resolver tag:%u: %s",
		tag.id,
		e.what()
	};

	const auto eptr(std::current_exception());
	const ctx::exception_handler eh;
	callback(eptr, tag, answers{});
}

bool
ircd::net::dns::resolver::handle_error(const header &header,
                                       tag &tag)
{
	switch(header.rcode)
	{
		case 0: // NoError; continue
			return true;

		case 3: // NXDomain; exception
			if(!tag.opts.nxdomain_exceptions)
				return true;

			return false;

		default: // Unhandled error; exception
			return false;
	}
}

//
// removal
//
// This whole stack must be called under lock
//

void
ircd::net::dns::resolver::cancel_all(const bool &remove)
{
	static const std::error_code &ec
	{
		make_error_code(std::errc::operation_canceled)
	};

	error_all(ec, remove);
}

void
ircd::net::dns::resolver::error_all(const std::error_code &ec,
                                    const bool &remove)
{
	if(tags.empty())
		return;

	log::dwarning
	{
		log, "Attempting to cancel all %zu pending tags.", tags.size()
	};

	const auto eptr
	{
		make_system_eptr(ec)
	};

	for(auto &p : tags)
		error_one(p.second, eptr, false);

	if(remove)
		for(auto it(begin(tags)); it != end(tags); it = this->remove(it->second, it));
}

void
ircd::net::dns::resolver::error_one(tag &tag,
                                    const std::system_error &se,
                                    const bool &remove)
{
	error_one(tag, std::make_exception_ptr(se), remove);
}

void
ircd::net::dns::resolver::error_one(tag &tag,
                                    const std::exception_ptr &eptr,
                                    const bool &remove)
{
	thread_local char hpbuf[128];
	log::error
	{
		log, "DNS error id:%u for '%s' :%s",
		tag.id,
		string(hpbuf, tag.hp),
		what(eptr)
	};

	static const answers empty;
	callback(eptr, tag, empty);

	if(remove)
		this->remove(tag);
}

void
ircd::net::dns::resolver::remove(tag &tag)
{
	remove(tag, tags.find(tag.id));
}

decltype(ircd::net::dns::resolver::tags)::iterator
ircd::net::dns::resolver::remove(tag &tag,
                                 const decltype(tags)::iterator &it)
{
	log::debug
	{
		log, "dns tag:%u t:%u qtype:%u removing (tags:%zu sendq:%zu)",
		tag.id,
		tag.tries,
		tag.opts.qtype,
		tags.size(),
		sendq.size()
	};

	if(it != end(tags))
		unqueue(tag);

	const auto ret
	{
		it != end(tags)?
			tags.erase(it):
			it
	};

	if(ret != end(tags))
		done.notify_all();

	return it;
}

void
ircd::net::dns::resolver::unqueue(tag &tag)
{
	const auto it
	{
		std::find(begin(sendq), end(sendq), tag.id)
	};

	if(it != end(sendq))
		sendq.erase(it);
}

//
// util
//

void
ircd::net::dns::resolver::set_servers()
try
{
	const std::string &list(resolver::servers);
	set_servers(list);
	dock.notify_all();
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Erroneous configuration; falling back to defaults :%s",
		e.what()
	};

	resolver::servers.fault();
	if(!ircd::net::dns::resolver_instance)
		set_servers();
}

void
ircd::net::dns::resolver::set_servers(const string_view &list)
{
	server.clear();
	server_next = 0;
	tokens(list, ' ', [this]
	(const string_view &hp)
	{
		add_server(hp);
	});

	if(!empty(list) && server.empty())
		throw error
		{
			"Failed to set any valid DNS servers from a non-empty list."
		};
}

void
ircd::net::dns::resolver::add_server(const string_view &str)
try
{
	const hostport hp
	{
		str
	};

	const auto &port
	{
		net::port(hp) != canon_port? net::port(hp) : uint16_t(53)
	};

	const ipport ipp
	{
		host(hp), port
	};

	add_server(ipp);
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Failed to add server '%s' :%s",
		str,
		e.what()
	};
}

void
ircd::net::dns::resolver::add_server(const ipport &ipp)
{
	server.emplace_back(make_endpoint_udp(ipp));

	log::debug
	{
		log, "Adding [%s] as DNS server #%zu",
		string(ipp),
		server.size()
	};
}
