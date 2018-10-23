// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/asio.h>
#include "s_dns.h"
#include "s_dns_resolver.h"

decltype(ircd::net::dns::resolver)
ircd::net::dns::resolver;

decltype(ircd::net::dns::resolver::servers)
ircd::net::dns::resolver::servers
{
	{
		{ "name",     "ircd.net.dns.resolver.servers"                    },
		{ "default",   "4.2.2.1;4.2.2.2;4.2.2.3;4.2.2.4;4.2.2.5;4.2.2.6" },
	}, []
	{
		if(bool(ircd::net::dns::resolver))
			ircd::net::dns::resolver->set_servers();
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
ircd::net::dns::resolver_init()
{
	assert(!ircd::net::dns::resolver);
	ircd::net::dns::resolver = new typename ircd::net::dns::resolver{};
}

void
ircd::net::dns::resolver_fini()
{
	delete ircd::net::dns::resolver;
	ircd::net::dns::resolver = nullptr;
}

void
ircd::net::dns::resolver_call(const hostport &hp,
                              const opts &opts,
                              callback &&cb)
{
	if(unlikely(!resolver))
		throw error
		{
			"Cannot resolve '%s': resolver unavailable.",
			host(hp)
		};

	auto &resolver{*dns::resolver};
	if(unlikely(!resolver.ns.is_open()))
		throw error
		{
			"Cannot resolve '%s': resolver is closed.",
			host(hp)
		};

	resolver(hp, opts, std::move(cb));
}

//
// resolver::resolver
//

ircd::net::dns::resolver::resolver()
:ns{ios::get()}
,reply
{
	64_KiB // worst-case UDP datagram size
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
	set_handle();
}

ircd::net::dns::resolver::~resolver()
noexcept
{
	ns.close();
	sendq_context.terminate();
	timeout_context.interrupt();
	while(!tags.empty())
	{
		log::warning
		{
			log, "Waiting for %zu unfinished DNS resolutions", tags.size()
		};

		ctx::sleep(3);
	}

	assert(tags.empty());
}

__attribute__((noreturn))
void
ircd::net::dns::resolver::sendq_worker()
{
	while(1)
	{
		dock.wait([this]
		{
			assert(sendq.empty() || !tags.empty());
			return !sendq.empty();
		});

		if(tags.size() > size_t(send_burst))
			ctx::sleep(milliseconds(send_rate));

		sendq_work();
	}
}

void
ircd::net::dns::resolver::sendq_work()
{
	assert(!sendq.empty());
	assert(sendq.size() < 65535);
	assert(sendq.size() <= tags.size());
	const unwind::nominal::assertion na;
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

	send_query(tag);
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

		ctx::sleep(milliseconds(timeout));
		check_timeouts(milliseconds(timeout));
	}
}
catch(const ctx::interrupted &)
{
	cancel_all_tags();
	return;
}

void
ircd::net::dns::resolver::cancel_all_tags()
{
	static const std::system_error ec
	{
		make_error_code(std::errc::operation_canceled)
	};

	if(!tags.empty())
		log::dwarning
		{
			log, "Attempting to cancel all %zu pending tags.", tags.size()
		};

	for(auto &p : tags)
		post_error(p.second, ec);
}

void
ircd::net::dns::resolver::check_timeouts(const milliseconds &timeout)
{
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
			it = tags.erase(it);
		else
			++it;
	}
}

bool
ircd::net::dns::resolver::check_timeout(const uint16_t &id,
                                        tag &tag,
                                        const steady_point &cutoff)
{
	if(tag.last == steady_point{})
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

	tag.last = steady_point{};
	if(tag.tries < size_t(retry_max))
	{
		submit(tag);
		return false;
	}

	static const std::system_error ec
	{
		make_error_code(std::errc::timed_out)
	};

	post_error(tag, ec);
	return true;
}

void
ircd::net::dns::resolver::post_error(tag &tag,
                                     const std::system_error &ec)
{
	// We ignore this request to post an error here if the tag has no callback
	// function set. Nulling the callback is used as hack-hoc state to indicate
	// that something else has called back the user and will unmap this tag so
	// there's no reason for us to post this.
	if(!tag.cb)
		return;

	auto handler
	{
		std::bind(&resolver::handle_post_error, this, tag.id, std::ref(tag), std::cref(ec))
	};

	// Callback gets a fresh stack off this timeout worker ctx's stack.
	ircd::post(std::move(handler));
}

void
ircd::net::dns::resolver::handle_post_error(const uint16_t id,
                                            tag &tag,
                                            const std::system_error &ec)
{
	// Have to check if the tag is still mapped at this point. It may
	// have been removed if a belated reply came in while this closure
	// was posting. If so, that's good news and we bail on the timeout.
	if(!tags.count(id))
		return;

	log::error
	{
		log, "DNS error id:%u for '%s' :%s",
		id,
		string(tag.hp),
		string(ec)
	};

	assert(tag.cb);
	tag.cb(std::make_exception_ptr(ec), tag.hp, {});
	const auto erased(tags.erase(tag.id));
	assert(erased == 1);
}

/// Internal resolver entry interface.
void
ircd::net::dns::resolver::operator()(const hostport &hp,
                                     const opts &opts,
                                     callback &&callback)
{
	auto &tag
	{
		set_tag(hp, opts, std::move(callback))
	};

	// Escape trunk
	const unwind::exceptional untag{[this, &tag]
	{
		tags.erase(tag.id);
	}};

	tag.question = make_query(tag.qbuf, tag);
	submit(tag);
}

ircd::const_buffer
ircd::net::dns::resolver::make_query(const mutable_buffer &buf,
                                     const tag &tag)
{
	thread_local char hostbuf[512];
	string_view hoststr;
	switch(tag.opts.qtype)
	{
		case 0: throw error
		{
			"A query type is required to form a question."
		};

		case 33: // SRV
		{
			hoststr = make_SRV_key(hostbuf, host(tag.hp), tag.opts);
			break;
		}

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
ircd::net::dns::resolver::tag &
ircd::net::dns::resolver::set_tag(A&&... args)
{
	while(tags.size() < 65535)
	{
		auto id(ircd::rand::integer(1, 65535));
		auto it{tags.lower_bound(id)};
		if(it != end(tags) && it->first == id)
			continue;

		it = tags.emplace_hint(it,
		                       std::piecewise_construct,
		                       std::forward_as_tuple(id),
		                       std::forward_as_tuple(std::forward<A>(args)...));
		it->second.id = id;
		dock.notify_one();
		return it->second;
	}

	throw assertive
	{
		"Too many DNS queries"
	};
}

void
ircd::net::dns::resolver::queue_query(tag &tag)
{
	assert(sendq.size() <= tags.size());
	sendq.emplace_back(tag.id);
	dock.notify_one();
}

void
ircd::net::dns::resolver::submit(tag &tag)
{
	assert(ns.is_open());
	const auto rate(milliseconds(send_rate) / server.size());
	const auto elapsed(now<steady_point>() - send_last);
	if(elapsed >= rate || tags.size() < size_t(send_burst))
		send_query(tag);
	else
		queue_query(tag);
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
}
catch(const std::out_of_range &)
{
	throw error
	{
		"No DNS servers available for query"
	};
}

void
ircd::net::dns::resolver::send_query(const ip::udp::endpoint &ep,
                                     tag &tag)
{
	assert(ns.non_blocking());
	assert(!empty(tag.question));
	const const_buffer &buf{tag.question};
	ns.send_to(asio::const_buffers_1(buf), ep);
	send_last = now<steady_point>();
	tag.last = send_last;
	tag.tries++;
}

void
ircd::net::dns::resolver::set_handle()
{
	auto handler
	{
		std::bind(&resolver::handle, this, ph::_1, ph::_2)
	};

	const asio::mutable_buffers_1 bufs{reply};
	ns.async_receive_from(bufs, reply_from, std::move(handler));
}

void
ircd::net::dns::resolver::handle(const error_code &ec,
                                 const size_t &bytes)
noexcept try
{
	if(!handle_error(ec))
		return;

	const unwind reset{[this]
	{
		set_handle();
	}};

	if(unlikely(bytes < sizeof(rfc1035::header)))
		throw rfc1035::error
		{
			"Got back %zu bytes < rfc1035 %zu byte header",
			bytes,
			sizeof(rfc1035::header)
		};

	char *const reply
	{
		data(this->reply)
	};

	rfc1035::header &header
	{
		*reinterpret_cast<rfc1035::header *>(reply)
	};

	bswap(&header.qdcount);
	bswap(&header.ancount);
	bswap(&header.nscount);
	bswap(&header.arcount);

	const const_buffer body
	{
		reply + sizeof(header), bytes - sizeof(header)
	};

	handle_reply(header, body);
}
catch(const std::exception &e)
{
	throw assertive
	{
		"resolver::handle_reply(): %s", e.what()
	};
}

void
ircd::net::dns::resolver::handle_reply(const header &header,
                                       const const_buffer &body)
try
{
	const auto &id{header.id};
	const auto it{tags.find(id)};
	if(it == end(tags))
		throw error
		{
			"DNS reply from %s for unrecognized tag id:%u",
			string(reply_from),
			id
		};

	auto &tag{it->second};
	const unwind untag{[this, &it]
	{
		tags.erase(it);
	}};

	assert(tag.tries > 0);
	tag.last = steady_point{};
	handle_reply(header, body, tag);
}
catch(const std::exception &e)
{
	log::error
	{
		log, "%s", e.what()
	};

	return;
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

	const_buffer buffer
	{
		body
	};

	// Questions are regurgitated back to us so they must be parsed first
	thread_local std::array<rfc1035::question, MAX_COUNT> qd;
	for(size_t i(0); i < header.qdcount; ++i)
		consume(buffer, size(qd.at(i).parse(buffer)));

	if(!handle_error(header, qd.at(0), tag))
		throw rfc1035::error
		{
			"protocol error #%u :%s", header.rcode, rfc1035::rcode.at(header.rcode)
		};

	// Answers are parsed into this buffer
	thread_local std::array<rfc1035::answer, MAX_COUNT> an;
	for(size_t i(0); i < header.ancount; ++i)
		consume(buffer, size(an[i].parse(buffer)));

	if(tag.opts.cache_result)
	{
		// We convert all TTL values in the answers to absolute epoch time
		// indicating when they expire. This makes more sense for our caches.
		const auto &now{ircd::time()};
		for(size_t i(0); i < header.ancount; ++i)
		{
			const uint &min_ttl(seconds(cache::min_ttl).count());
			an[i].ttl = now + std::max(an[i].ttl, min_ttl);
		}
	}

	// The callback to the user will be passed a vector_view of pointers
	// to this array. The actual record instances will either be located
	// in the cache map or placement-newed to the buffer below.
	thread_local const rfc1035::record *record[MAX_COUNT];

	// This will be where we place the record instances which are dynamically
	// laid out and sized types. 512 bytes is assumed as a soft maximum for
	// each RR instance.
	thread_local uint8_t recbuf[MAX_COUNT * 512];

	size_t i(0);
	uint8_t *pos{recbuf};
	for(; i < header.ancount; ++i) switch(an[i].qtype)
	{
		case 1: // A records are inserted into cache
		{
			if(!tag.opts.cache_result)
			{
				record[i] = new (pos) rfc1035::record::A(an[i]);
				pos += sizeof(rfc1035::record::A);
				continue;
			}

			record[i] = cache::put(qd.at(0), an[i]);
			continue;
		}

		case 5:
		{
			record[i] = new (pos) rfc1035::record::CNAME(an[i]);
			pos += sizeof(rfc1035::record::CNAME);
			continue;
		}

		case 33:
		{
			if(!tag.opts.cache_result)
			{
				record[i] = new (pos) rfc1035::record::SRV(an[i]);
				pos += sizeof(rfc1035::record::SRV);
				continue;
			}

			record[i] = cache::put(qd.at(0), an[i]);
			continue;
		}

		default:
		{
			record[i] = new (pos) rfc1035::record(an[i]);
			pos += sizeof(rfc1035::record);
			continue;
		}
	}

	// Cache no answers here.
	if(!header.ancount && tag.opts.cache_result)
		cache::put_error(qd.at(0), header.rcode);

	if(tag.cb)
	{
		const vector_view<const rfc1035::record *> records(record, i);
		tag.cb(std::exception_ptr{}, tag.hp, records);
	}
}
catch(const std::exception &e)
{
	// There's no need to flash red to the log for NXDOMAIN which is
	// common in this system when probing SRV.
	if(unlikely(header.rcode != 3))
		log::error
		{
			log, "resolver tag:%u: %s",
			tag.id,
			e.what()
		};

	if(tag.cb)
	{
		assert(header.rcode != 3 || tag.opts.nxdomain_exceptions);
		tag.cb(std::current_exception(), tag.hp, {});
	}
}

bool
ircd::net::dns::resolver::handle_error(const header &header,
                                       const rfc1035::question &question,
                                       tag &tag)
{
	switch(header.rcode)
	{
		case 0: // NoError; continue
			return true;

		case 3: // NXDomain; exception
		{
			if(!tag.opts.cache_result)
				return false;

			const auto *record
			{
				cache::put_error(question, header.rcode)
			};

			// When the user doesn't want an eptr for nxdomain we just make
			// their callback here and then null the cb pointer so it's not
			// called again. It is done here because we have a reference to
			// the cached error record readily accessible.
			if(!tag.opts.nxdomain_exceptions && tag.cb)
			{
				assert(record);
				tag.cb({}, tag.hp, vector_view<const rfc1035::record *>(&record, 1));
				tag.cb = {};
			}

			return false;
		}

		default: // Unhandled error; exception
			return false;
	}
}

bool
ircd::net::dns::resolver::handle_error(const error_code &ec)
const
{
	using namespace boost::system::errc;

	switch(ec.value())
	{
		case operation_canceled:
			return false;

		case success:
			return true;

		default:
			throw boost::system::system_error(ec);
	}
}

void
ircd::net::dns::resolver::set_servers()
{
	const std::string &list(resolver::servers);
	set_servers(list);
}

void
ircd::net::dns::resolver::set_servers(const string_view &list)
{
	server.clear();
	server_next = 0;
	tokens(list, ';', [this]
	(const hostport &hp)
	{
		const auto &port
		{
			net::port(hp) != canon_port? net::port(hp) : uint16_t(53)
		};

		const ipport ipp
		{
			host(hp), port
		};

		add_server(ipp);
	});
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
