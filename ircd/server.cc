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

namespace ircd::server
{
	// Internal state
	ctx::dock dock;     // internal semaphore

	// Internal util
	template<class F> size_t accumulate_peers(F&&);
	template<class F> size_t accumulate_links(F&&);
	template<class F> size_t accumulate_tags(F&&);

	// Internal control
	static decltype(ircd::server::peers)::iterator
	create(const net::hostport &, decltype(peers)::iterator &);
}

decltype(ircd::server::log)
ircd::server::log
{
	"server", 'S'
};

ircd::conf::item<ircd::seconds>
close_all_timeout
{
	{ "name",     "ircd.server.close_all_timeout" },
	{ "default",  2L                              },
};

//
// init
//

ircd::server::init::init()
{
}

ircd::server::init::~init()
noexcept
{
	interrupt_all();
	close_all();
	wait_all();
	log::debug
	{
		log, "All server peers, connections, and requests are clear."
	};
}

//
// server
//

void
ircd::server::wait_all()
{
	while(peer_unfinished())
	{
		if(dock.wait_for(seconds(2)))
			continue;

		log::warning
		{
			log, "Waiting for %zu tags on %zu links on %zu of %zu peers to close...",
			tag_count(),
			link_count(),
			peer_unfinished(),
			peer_count()
		};
	}

	peers.clear();
}

void
ircd::server::close_all()
{
	log::debug
	{
		log, "Closing all %zu peers",
		peer_count()
	};

	net::close_opts opts;
	opts.timeout = seconds(close_all_timeout);
	for(auto &peer : peers)
		peer.second->close(opts);
}

void
ircd::server::interrupt_all()
{
	log::debug
	{
		log, "Interrupting %zu tags on %zu links on %zu peers",
		tag_count(),
		link_count(),
		peer_count()
	};

	for(auto &peer : peers)
		peer.second->cancel();
}

ircd::server::peer &
ircd::server::get(const net::hostport &hostport)
{
	char canonbuf[256];
	const auto canonized
	{
		net::canonize(canonbuf, hostport)
	};

	auto it(peers.lower_bound(canonized));
	if(it == peers.end() || it->first != canonized)
		it = create(hostport, it);

	return *it->second;
}

decltype(ircd::server::peers)::iterator
ircd::server::create(const net::hostport &hostport,
                     decltype(peers)::iterator &it)
{
	auto peer
	{
		std::make_unique<server::peer>(hostport)
	};

	log::debug
	{
		log, "peer(%p) for %s created; adding...",
		peer.get(),
		peer->hostcanon,
	};

	assert(bool(peer));
	assert(!empty(peer->hostcanon));
	const string_view key{peer->hostcanon};
	it = peers.emplace_hint(it, key, std::move(peer));

	assert(it->second->hostcanon.data() == it->first.data());
	it->second->resolve(it->second->open_opts.hostport);
	return it;
}

ircd::server::peer &
ircd::server::find(const net::hostport &hostport)
{
	return *peers.at(host(hostport));
}

bool
ircd::server::exists(const net::hostport &hostport)
{
	return peers.find(host(hostport)) != end(peers);
}

bool
ircd::server::errclear(const net::hostport &hostport)
{
	const auto it
	{
		peers.find(host(hostport))
	};

	if(it == end(peers))
		return false;

	auto &peer(*it->second);
	return peer.err_clear();
}

ircd::string_view
ircd::server::errmsg(const net::hostport &hostport)
{
	const auto it
	{
		peers.find(host(hostport))
	};

	if(it == end(peers))
		return {};

	return it->second->err_msg();
}

size_t
ircd::server::peer_unfinished()
{
	return accumulate_peers([]
	(const auto &peer)
	{
		return !peer.finished();
	});
}

size_t
ircd::server::peer_count()
{
	return peers.size();
}

size_t
ircd::server::link_count()
{
	return accumulate_peers([]
	(const auto &peer)
	{
		return peer.link_count();
	});
}

size_t
ircd::server::tag_count()
{
	return accumulate_peers([]
	(const auto &peer)
	{
		return peer.tag_count();
	});
}

template<class F>
size_t
ircd::server::accumulate_tags(F&& closure)
{
	return accumulate_links([&closure]
	(const auto &link)
	{
		return link.accumulate_tags(std::forward<F>(closure));
	});
}

template<class F>
size_t
ircd::server::accumulate_links(F&& closure)
{
	return accumulate_peers([&closure]
	(const auto &peer)
	{
		return peer.accumulate_links(std::forward<F>(closure));
	});
}

template<class F>
size_t
ircd::server::accumulate_peers(F&& closure)
{
	return std::accumulate(begin(peers), end(peers), size_t(0), [&closure]
	(auto ret, const auto &pair)
	{
		const auto &peer{*pair.second};
		return ret += closure(peer);
	});
}

///
// request
//

decltype(ircd::server::request::opts_default)
ircd::server::request::opts_default
{};

/// Canceling a request is tricky. This allows a robust way to let the user's
/// request go out of scope at virtually any time without disrupting the
/// pipeline and other requests.
bool
ircd::server::cancel(request &request)
{
	if(!request.tag)
		return false;

	if(request.tag->canceled())
		return false;

	if(request.tag->abandoned())
		return false;

	auto &tag
	{
		*request.tag
	};

/*
	log::debug
	{
		log, "cancel request(%p) tag(%p) commit:%d w:%zu hr:%zu cr:%zu",
		&request,
		&tag,
		tag.committed(),
		tag.state.written,
		tag.state.head_read,
		tag.state.content_read
	};
*/

	tag.set_exception<canceled>("Request canceled");

	// We got off easy... The link's write loop won't start an abandoned
	// request. All that has to be done is indicate a full cancellation
	// immediately and the user will know nothing was revealed to the remote.
	if(!tag.committed())
		return true;

	// Now things aren't so easy. More complicated logic happens inside...
	cancel(request, tag);
	return true;
}

void
ircd::server::submit(const hostport &hostport,
                     request &request)
{
	if(unlikely(ircd::run::level != ircd::run::level::RUN))
		throw unavailable
		{
			"Unable to fulfill requests at this time."
		};

	assert(request.tag == nullptr);
	auto &peer(server::get(hostport));
	peer.submit(request);
}

ircd::string_view
ircd::server::loghead(const mutable_buffer &buf,
                      const request &request)
try
{
	if(empty(request.in.head))
		return "<no header>";

	parse::buffer pb{request.out.head};
	parse::capstan pc{pb};
	pc.read += size(request.out.head);
	const http::request::head head{pc};
	return fmt::sprintf
	{
		buf, "%s %s", head.method, head.path
	};
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "server::loghead(): %s", e.what()
	};

	return "<loghead error>";
}

ircd::string_view
ircd::server::loghead(const request &request)
{
	thread_local char buf[256];
	return loghead(buf, request);
}

///////////////////////////////////////////////////////////////////////////////
//
// server/peer.h
//

decltype(ircd::server::peers)
ircd::server::peers
{};

decltype(ircd::server::peer::link_min_default)
ircd::server::peer::link_min_default
{
	{ "name",     "ircd.server.peer.link_min" },
	{ "default",  1L                          }
};

decltype(ircd::server::peer::link_max_default)
ircd::server::peer::link_max_default
{
	{ "name",     "ircd.server.peer.link_max" },
	{ "default",  2L                          }
};

//
// peer::peer
//

ircd::server::peer::peer(const net::hostport &hostport,
                         const net::open_opts &open_opts)
:hostcanon
{
	net::canonize(hostport)
}
,service
{
	net::service(hostport)
}
,open_opts
{
	std::move(open_opts)
}
{
	const net::hostport canon{this->hostcanon};
	this->open_opts.hostport.host = net::host(canon);
	this->open_opts.hostport.port = net::port(canon);
	this->open_opts.hostport.service = this->service;
	this->open_opts.ipport = this->remote;
}

ircd::server::peer::~peer()
noexcept
{
	assert(links.empty());
}

void
ircd::server::peer::close(const net::close_opts &opts)
{
	op_fini = true;
	link *links[LINK_MAX];
	const auto end(pointers(this->links, links));
	for(link **link(links); link != end; ++link)
		(*link)->close(opts);

	if(finished())
		return handle_finished();
}

void
ircd::server::peer::cancel()
{
	for(auto &link : this->links)
		link.cancel_all(make_exception_ptr<canceled>
		(
			"Request was aborted due to interruption."
		));
}

bool
ircd::server::peer::err_clear()
{
	const auto ret{bool(e)};
	e.reset(nullptr);
	op_fini = false;
	return ret;
}

template<class... A>
void
ircd::server::peer::err_set(A&&... args)
{
	this->e = std::make_unique<err>(std::forward<A>(args)...);
}

ircd::string_view
ircd::server::peer::err_msg()
const
{
	return bool(e)? what(e->eptr) : string_view{};
}

bool
ircd::server::peer::err_has()
const
{
	return bool(e);
}

decltype(ircd::server::peer::error_clear_default)
ircd::server::peer::error_clear_default
{
	{ "name",     "ircd.server.peer.error.clear_default" },
	{ "default",  305L                                   }
};

bool
ircd::server::peer::err_check()
{
	if(op_fini)
		return false;

	if(!err_has())
		return true;

	//TODO: The specific error type should be switched and finer
	//TODO: timeouts should be used depending on the error: i.e
	//TODO: NXDOMAIN vs. temporary conn timeout, etc.
	if(e->etime + seconds(error_clear_default) > now<system_point>())
		return false;

	err_clear();
	return true;
}

void
ircd::server::peer::submit(request &request)
try
{
	if(!err_check() || unlikely(ircd::run::level != ircd::run::level::RUN))
		throw unavailable
		{
			"Peer is unable to take any requests: %s", err_msg()
		};

	link *const ret
	{
		link_get(request)
	};

	if(likely(ret))
	{
		ret->submit(request);
		return;
	}

	if(!request.tag)
		throw unavailable
		{
			"No link to peer %s available", hostcanon
		};
	else
		request.tag->set_exception<unavailable>
		(
			"No link to peer %s available", hostcanon
		);
}
catch(const std::exception &e)
{
	if(!request.tag)
		throw;

	const auto eptr(std::current_exception());
	const ctx::exception_handler eh;
	request.tag->set_exception(eptr);
}

/// Dispatch algorithm here; finds the best link to place this request on,
/// or creates a new link entirely. There are a number of factors: foremost
/// if any special needs are indicated,
//
ircd::server::link *
ircd::server::peer::link_get(const request &request)
{
	assert(request.opt);
	const auto &prio(request.opt->priority);

	if(links.empty())
		return &link_add(1);

	// Indicates that we can't add anymore links for this peer and the rest
	// of the algorithm should consider this.
	const bool links_maxed
	{
		links.size() >= link_max()
	};

	link *best{nullptr};
	for(auto &cand : links)
	{
		// Don't want a link that's shutting down or marked for exclusion
		if(cand.op_fini || cand.exclude)
			continue;

		if(!best)
		{
			best = &cand;
			continue;
		}

		// Indicates that the best candidate has its pipe saturated which can
		// be factored into the comparisons here.
		const bool best_maxed
		{
			best->tag_committed() >= best->tag_commit_max()
		};

		const bool cand_maxed
		{
			cand.tag_committed() >= cand.tag_commit_max()
		};

		if(best_maxed && !cand_maxed)
		{
			best = &cand;
			continue;
		}

		if(!best_maxed && cand_maxed)
			continue;

		// Candidates's queue has less or same backlog of unsent requests, but
		// now measure if candidate will take longer to process at least the
		// write-side of those requests.
		if(cand.write_remaining() > best->write_remaining())
			continue;

		// Candidate might be working through really large content; though
		// this is a very sketchy measurement right now since we only *might*
		// know about content-length for the *one* active tag occupying the
		// socket.
		if(cand.read_remaining() > best->read_remaining())
			continue;

		// Coarse distribution based on who has more work; this is weak, should
		// be replaced.
		if(cand.tag_count() > best->tag_count())
			continue;

		best = &cand;
	}

	// Even though the prio is set to the super special value we allow the
	// normal loop to first come up with a best link which already is open
	// rather than unconditionally opening a new connection.
	if(prio == std::numeric_limits<std::remove_reference<decltype(prio)>::type>::min())
	{
		if(!best)
			return &link_add(1);

		if(best->tag_committed())
			return &link_add(1);

		return best;
	}

	if(links_maxed)
		return best;

	// best might not be good enough, we could try another connection. If best
	// has a backlog or is working on a large download or slow request.
	if(!best)
	{
		best = &link_add();
		return best;
	}

	if(best->tag_uncommitted() < best->tag_commit_max())
		return best;

	best = &link_add();
	return best;
}

ircd::server::link &
ircd::server::peer::link_add(const size_t &num)
{
	assert(!finished());

	if(e)
	{
		std::rethrow_exception(e->eptr);
		__builtin_unreachable();
	}

	assert(!op_fini);
	links.emplace_back(*this);
	auto &link{links.back()};

	if(remote)
		link.open(open_opts);

	return link;
}

void
ircd::server::peer::handle_open(link &link,
                                std::exception_ptr eptr)
{
	if(eptr)
	{
		if(links.size() == 1)
			err_set(eptr);

		log::derror
		{
			log, "peer(%p) link(%p) [%s]: open: %s",
			this,
			&link,
			string(remote),
			what(eptr)
		};

		if(op_fini)
		{
			if(link.finished())
				handle_finished(link);

			return;
		}

		link.close(net::dc::RST);
		return;
	}
}

void
ircd::server::peer::handle_close(link &link,
                                 std::exception_ptr eptr)
{
	if(eptr)
		log::derror
		{
			log, "peer(%p) link(%p) [%s]: close: %s",
			this,
			&link,
			string(remote),
			what(eptr)
		};

	if(link.finished())
		handle_finished(link);
}

void
ircd::server::peer::handle_error(link &link,
                                 std::exception_ptr eptr)
{
	assert(bool(eptr));
	link.cancel_committed(eptr);
	log::derror
	{
		log, "peer(%p) link(%p): %s",
		this,
		&link,
		what(eptr)
	};

	link.close(net::dc::RST);
}

void
ircd::server::peer::handle_error(link &link,
                                 const std::system_error &e)
{
	using std::errc;
	using boost::asio::error::get_misc_category;

	const auto &ec{e.code()};
	if(system_category(ec)) switch(ec.value())
	{
		case 0:
			assert(0);
			break;

		default:
			break;
	}
	else if(ec.category() == get_misc_category()) switch(ec.value())
	{
		case asio::error::eof:
			log::debug
			{
				log, "peer(%p) link(%p) [%s]: %s",
				this,
				&link,
				string(remote),
				e.what()
			};

			link.close(net::close_opts_default);
			return;

		default:
			break;
	}

	log::derror
	{
		log, "peer(%p) link(%p) [%s]: %s",
		this,
		&link,
		string(remote),
		e.what()
	};

	link.cancel_committed(std::make_exception_ptr(e));
	link.close(net::dc::RST);
}

void
ircd::server::peer::handle_finished(link &link)
{
	assert(link.finished());
	del(link);

	if(finished())
		handle_finished();
}

/// This is where we're notified a tag has been completed either to start the
/// next request when the link has too many requests in flight or perhaps to
/// reschedule the queues in various links to diffuse the pending requests.
/// This can't throw because the link still has to remove this tag from its
/// queue.
void
ircd::server::peer::handle_tag_done(link &link,
                                    tag &tag)
noexcept try
{
	log::debug
	{
		log, "peer(%p) link(%p) tag(%p) [%s] done wt:%zu rt:%zu hr:%zu cr:%zu cl:%zu; %zu more in queue",
		this,
		&link,
		&tag,
		tag.request?
			loghead(*tag.request):
			"<no request>"_sv,
		tag.write_size(),
		tag.read_size(),
		tag.state.head_read,
		tag.state.content_read,
		tag.state.content_length,
		link.tag_count() - 1
	};

	if(link.tag_committed() >= link.tag_commit_max())
		link.wait_writable();
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "peer(%p) link(%p) tag(%p) done; %s",
		this,
		&link,
		&tag,
		e.what()
	};
}

/// This is where we're notified a link has processed its queue and has no
/// more work. We can choose whether to close the link or keep it open and
/// reinstate the read poll; reschedule other work to this link, etc.
void
ircd::server::peer::handle_link_done(link &link)
{
	assert(link.tag_count() == 0);

	if(link_ready() > link_min())
	{
		link.close();
		return;
	}

	link.wait_readable();
}

/// This is called when a tag on a link receives an HTTP response head.
/// We can use this to learn information from the tag's request and the
/// response head etc.
void
ircd::server::peer::handle_head_recv(const link &link,
                                     const tag &tag,
                                     const http::response::head &head)
{
	// Learn the software version of the remote peer so we can shape
	// requests more effectively.
	if(!server_name && head.server)
	{
		server_name = std::string{head.server};
		log::debug
		{
			log, "peer(%p) learned %s is '%s'",
			this,
			string(remote),
			server_name
		};
	}
}

void
ircd::server::peer::disperse(link &link)
{
	disperse_uncommitted(link);
	link.cancel_committed(make_exception_ptr<canceled>
	(
		"Request was aborted; though it was partially completed"
	));

	assert(link.queue.empty());
}

void
ircd::server::peer::disperse_uncommitted(link &link)
{
	auto &queue(link.queue);
	auto it(begin(queue));
	while(it != end(queue)) try
	{
		auto &tag{*it};
		if(!tag.request || tag.committed())
		{
			++it;
			continue;
		}

		submit(*tag.request);
		it = queue.erase(it);
	}
	catch(const std::exception &e)
	{
		const auto &tag{*it};
		log::warning
		{
			log, "peer(%p) failed to resubmit tag(%p): %s",
			this,
			&tag,
			e.what()
		};

		it = queue.erase(it);
	}
}

/// This *cannot* be called unless a link's socket is closed and its queue
/// is empty. It is usually only called by a disconnect handler because
/// the proper way to remove a link is asynchronously through link.close();
void
ircd::server::peer::del(link &link)
{
	assert(!link.tag_count());
	assert(!link.opened());
	const auto it(std::find_if(begin(links), end(links), [&link]
	(const auto &link_)
	{
		return &link_ == &link;
	}));

	assert(it != end(links));
	log::debug
	{
		log, "peer(%p) removing link(%p) %zu of %zu to %s",
		this,
		&link,
		std::distance(begin(links), it),
		links.size(),
		string(remote)
	};

	links.erase(it);
}

void
ircd::server::peer::resolve(const hostport &hostport)
{
	net::dns::opts opts;
	opts.qtype = 33; // start with SRV.
	opts.nxdomain_exceptions = false;
	resolve(hostport, opts);
}

void
ircd::server::peer::resolve(const hostport &hostport,
                            const net::dns::opts &opts)
{
	if(op_resolve || op_fini)
		return;

	if(unlikely(opts.qtype != 33 && opts.qtype != 28 && opts.qtype != 1))
		throw error
		{
			"Unsupported DNS question type '%u' for resolve", opts.qtype
		};

	auto handler
	{
		opts.qtype == 33?
			net::dns::callback(std::bind(&peer::handle_resolve_SRV, this, ph::_1, ph::_2)):
			net::dns::callback(std::bind(&peer::handle_resolve_A, this, ph::_1, ph::_2))
	};

	op_resolve = true;
	assert(ctx::current); // sorry, ircd::ctx required for now.
	net::dns::resolve(hostport, opts, std::move(handler));
}

void
ircd::server::peer::handle_resolve_SRV(const hostport &hp,
                                       const json::array &rrs)
try
{
	assert(op_resolve);
	op_resolve = false;

	if(unlikely(ircd::run::level != ircd::run::level::RUN))
		op_fini = true;

	if(unlikely(finished()))
		return handle_finished();

	if(op_fini)
		return;

	const json::object &rr
	{
		net::dns::random_choice(rrs)
	};

	if(net::dns::is_error(rr))
	{
		const json::string &error(rr.get("error"));
		err_set(make_exception_ptr<rfc1035::error>("%s", error));
		assert(this->e && this->e->eptr);
		std::rethrow_exception(this->e->eptr);
		__builtin_unreachable();
	}

	// Target for the next address record query.
    const hostport &target
    {
        rr.has("tgt")?
            rstrip(unquote(rr.at("tgt")), '.'):
            host(hp),

        rr.has("port")?
            rr.get<uint16_t>("port"):
            port(hp)
    };

	// Save the port from the SRV record to a class member because it won't
	// get carried through the next A/AAAA query.
	port(remote) = port(target);

	// Setup the address record query off this SRV response.
	net::dns::opts opts;
	opts.qtype = 1;

	log::debug
	{
		log, "peer(%p) resolved %s SRV rrs:%zu resolving %s %s",
		this,
		host(hp),
		rrs.size(),
		host(target),
		rfc1035::rqtype.at(opts.qtype)
	};

	resolve(target, opts);
}
catch(const std::exception &e)
{
	log::derror
	{
		log, "peer(%p) resolve SRV: %s",
		this,
		e.what()
	};

	const ctx::exception_handler eh;
	close();
}

void
ircd::server::peer::handle_resolve_A(const hostport &,
                                     const json::array &rrs)
try
{
	const ctx::critical_assertion ca;
	assert(op_resolve);
	op_resolve = false;

	if(unlikely(ircd::run::level != ircd::run::level::RUN))
		op_fini = true;

	if(unlikely(finished()))
		return handle_finished();

	if(op_fini)
		return;

	if(net::dns::is_empty(rrs))
	{
		err_set(make_exception_ptr<unavailable>("Host has no address record."));
		assert(this->e && this->e->eptr);
		std::rethrow_exception(this->e->eptr);
		__builtin_unreachable();
	}

	const json::object &rr
	{
		net::dns::random_choice(rrs)
	};

	if(net::dns::is_error(rr))
	{
		const json::string &error(rr.get("error"));
		err_set(make_exception_ptr<rfc1035::error>("%s", error));
		assert(this->e && this->e->eptr);
		std::rethrow_exception(this->e->eptr);
		__builtin_unreachable();
	}

	// Save the results of the query to this object instance.
	this->remote = net::ipport
	{
		unquote(rr.at("ip")), net::port(this->remote)
	};

	open_opts.ipport = this->remote;
	port(open_opts.hostport) = port(this->remote);
	open_links();
}
catch(const std::exception &e)
{
	log::derror
	{
		log, "peer(%p) resolve A/AAAA: %s",
		this,
		e.what()
	};

	const ctx::exception_handler eh;
	close();
}

void
ircd::server::peer::open_links()
try
{
	// The hostname in open_opts should still reference this object's string.
	assert(host(open_opts.hostport).data() == this->hostcanon.data());

	link *links[LINK_MAX];
	const auto end(pointers(this->links, links));
	for(link **link(links); link != end; ++link)
		(*link)->open(open_opts);
}
catch(const std::exception &e)
{
	log::derror
	{
		log, "peer(%p) open links: %s",
		this,
		e.what()
	};

	const ctx::exception_handler eh;
	close();
}

void
ircd::server::peer::handle_finished()
{
	assert(finished());

	// Right now this is what the server:: ~init sequence needs
	// to wait for all links to close on IRCd shutdown.
	server::dock.notify_all();
}

size_t
ircd::server::peer::read_total()
const
{
	return read_bytes;
}

size_t
ircd::server::peer::write_total()
const
{
	return write_bytes;
}

size_t
ircd::server::peer::read_remaining()
const
{
	return accumulate_links([](const auto &link)
	{
		return link.read_remaining();
	});
}

size_t
ircd::server::peer::read_completed()
const
{
	return accumulate_links([](const auto &link)
	{
		return link.read_completed();
	});
}

size_t
ircd::server::peer::read_size()
const
{
	return accumulate_links([](const auto &link)
	{
		return link.read_size();
	});
}

size_t
ircd::server::peer::write_remaining()
const
{
	return accumulate_links([](const auto &link)
	{
		return link.write_remaining();
	});
}

size_t
ircd::server::peer::write_completed()
const
{
	return accumulate_links([](const auto &link)
	{
		return link.write_completed();
	});
}

size_t
ircd::server::peer::write_size()
const
{
	return accumulate_links([](const auto &link)
	{
		return link.write_size();
	});
}

size_t
ircd::server::peer::tag_uncommitted()
const
{
	return accumulate_links([](const auto &link)
	{
		return link.tag_uncommitted();
	});
}

size_t
ircd::server::peer::tag_committed()
const
{
	return accumulate_links([](const auto &link)
	{
		return link.tag_committed();
	});
}

size_t
ircd::server::peer::tag_count()
const
{
	return accumulate_links([](const auto &link)
	{
		return link.tag_count();
	});
}

size_t
ircd::server::peer::link_ready()
const
{
	return accumulate_links([](const auto &link)
	{
		return link.ready();
	});
}

size_t
ircd::server::peer::link_busy()
const
{
	return accumulate_links([](const auto &link)
	{
		return link.busy();
	});
}

size_t
ircd::server::peer::link_count()
const
{
	return links.size();
}

size_t
ircd::server::peer::link_min()
const
{
	return link_min_default;
}

size_t
ircd::server::peer::link_max()
const
{
	return link_max_default;
}

bool
ircd::server::peer::finished()
const
{
	return links.empty() && !op_resolve && op_fini;
}

template<class F>
size_t
ircd::server::peer::accumulate_tags(F&& closure)
const
{
	return accumulate_links([&closure](const auto &link)
	{
		return link.accumulate([&closure](const auto &tag)
		{
			return closure(tag);
		});
	});
}

template<class F>
size_t
ircd::server::peer::accumulate_links(F&& closure)
const
{
	return std::accumulate(begin(links), end(links), size_t(0), [&closure]
	(auto ret, const auto &tag)
	{
		return ret += closure(tag);
	});
}

//
// peer::err
//

ircd::server::peer::err::err(std::exception_ptr eptr)
:eptr{std::move(eptr)}
,etime{now<system_point>()}
{
}

ircd::server::peer::err::~err()
noexcept
{
}

//
// link
//

decltype(ircd::server::link::tag_max_default)
ircd::server::link::tag_max_default
{
	{ "name",     "ircd.server.link.tag_max" },
	{ "default",  -1L                        }
};

decltype(ircd::server::link::tag_commit_max_default)
ircd::server::link::tag_commit_max_default
{
	{ "name",     "ircd.server.link.tag_commit_max" },
	{ "default",  3L                                }
};

//
// link::link
//

ircd::server::link::link(server::peer &peer)
:peer{&peer}
{
}

ircd::server::link::~link()
noexcept
{
	assert(!busy());
	assert(!opened());
}

void
ircd::server::link::submit(request &request)
{
	assert(!request.tag || !request.tag->committed());

	const auto it
	{
		request.tag? queue.emplace(end(queue), std::move(*request.tag)):
		             queue.emplace(end(queue), request)
	};
/*
	log::debug
	{
		log, "tag(%p) submitted to link(%p) queue: %zu",
		&(*it),
		this,
		tag_count()
	};
*/
	if(ready())
		wait_writable();
}

void
ircd::server::link::cancel_all(std::exception_ptr eptr)
{
	for(auto it(begin(queue)); it != end(queue); it = queue.erase(it))
	{
		auto &tag{*it};
		if(!tag.request)
			continue;

		tag.set_exception(eptr);
	}
}

void
ircd::server::link::cancel_committed(std::exception_ptr eptr)
{
	for(auto it(begin(queue)); it != end(queue); it = queue.erase(it))
	{
		auto &tag{*it};
		if(!tag.request)
			continue;

		if(!tag.committed())
			break;

		tag.set_exception(eptr);
	}
}

void
ircd::server::link::cancel_uncommitted(std::exception_ptr eptr)
{
	auto it(begin(queue));
	while(it != end(queue))
	{
		auto &tag{*it};
		if(!tag.request || tag.committed())
		{
			++it;
			continue;
		}

		tag.set_exception(eptr);
		it = queue.erase(it);
	}
}

bool
ircd::server::link::open(const net::open_opts &open_opts)
{
	assert(ircd::run::level == ircd::run::level::RUN);

	if(op_init)
		return false;

	auto handler
	{
		std::bind(&link::handle_open, this, ph::_1)
	};

	op_init = true;
	const unwind::exceptional unhandled{[this]
	{
		op_init = false;
	}};

	socket = net::open(open_opts, std::move(handler));
	return true;
}

void
ircd::server::link::handle_open(std::exception_ptr eptr)
{
	assert(op_init);
	op_init = false;

	if(!eptr && !op_fini)
		wait_writable();

	if(peer)
		peer->handle_open(*this, std::move(eptr));
}

bool
ircd::server::link::close(const net::close_opts &close_opts)
{
	if(op_fini)
		return false;

	op_fini = true;

	// Tell the peer to ditch everything in the queue; op_fini has been set so
	// the tags won't get assigned back to this link.
	if(tag_count() && peer)
		peer->disperse(*this);

	auto handler
	{
		std::bind(&link::handle_close, this, ph::_1)
	};

	if(!socket)
	{
		handler(std::exception_ptr{});
		return true;
	}

	net::close(*socket, close_opts, std::move(handler));
	return true;
}

void
ircd::server::link::handle_close(std::exception_ptr eptr)
{
	assert(op_fini);

	if(op_init)
	{
		assert(bool(eptr));
	}

	if(peer)
		peer->handle_close(*this, std::move(eptr));
}

void
ircd::server::link::wait_writable()
{
	if(op_write || unlikely(op_fini))
		return;

	auto handler
	{
		std::bind(&link::handle_writable, this, ph::_1)
	};

	assert(ready());
	op_write = true;
	const unwind::exceptional unhandled{[this]
	{
		op_write = false;
	}};

	net::wait(*socket, net::ready::WRITE, std::move(handler));
}

void
ircd::server::link::handle_writable(const error_code &ec)
try
{
	using std::errc;

	op_write = false;

	if(unlikely(finished()))
	{
		assert(peer);
		return peer->handle_finished(*this);
	}

	if(system_category(ec)) switch(ec.value())
	{
		case 0:
			handle_writable_success();
			return;

		case int(errc::operation_canceled):
			return;

		default:
			break;
	}

	throw std::system_error{ec};
}
catch(const std::system_error &e)
{
	assert(peer);
	peer->handle_error(*this, e);
}
catch(...)
{
	assert(peer);
	peer->handle_error(*this, std::current_exception());
}

void
ircd::server::link::handle_writable_success()
{
	auto it(begin(queue));
	while(it != end(queue))
	{
		auto &tag{*it};
		if((tag.abandoned() || tag.canceled()) && !tag.committed())
		{
			log::debug
			{
				log, "link(%p) discarding canceled:%d abandoned:%d uncommitted tag %zu of %zu",
				this,
				tag.canceled(),
				tag.abandoned(),
				tag_committed(),
				tag_count()
			};

			it = queue.erase(it);
			continue;
		}

		if(tag.canceled() && tag.committed() && tag_committed() <= 1)
		{
			log::debug
			{
				log, "link(%p) closing to interrupt canceled committed tag(%p) of %zu",
				this,
				&tag,
				tag_count()
			};

			close();
			break;
		}

		if(tag_committed() == 0)
			wait_readable();

		if(!process_write(tag))
		{
			wait_writable();
			break;
		}

		// Limits the amount of requests in the pipe.
		if(tag_committed() >= tag_commit_max())
			break;

		++it;
	}
}

bool
ircd::server::link::process_write(tag &tag)
{
	if(!tag.committed())
		log::debug
		{
			log, "peer(%p) link(%p) starting on tag(%p) %zu of %zu: wt:%zu [%s]",
			peer,
			this,
			&tag,
			tag_committed(),
			tag_count(),
			tag.write_size(),
			tag.request?
				loghead(*tag.request):
				"<no attached request>"_sv
		};

	while(tag.write_remaining())
	{
		const const_buffer buffer
		{
			tag.make_write_buffer()
		};

		assert(!empty(buffer));
		const const_buffer written
		{
			process_write_next(buffer)
		};

		tag.wrote_buffer(written);
		assert(tag_committed() <= tag_commit_max());
		if(size(written) < size(buffer))
			return false;
	}

	return true;
}

ircd::const_buffer
ircd::server::link::process_write_next(const const_buffer &buffer)
{
	const size_t bytes
	{
		write_any(*socket, buffer)
	};

	const const_buffer written
	{
		data(buffer), bytes
	};

	assert(peer);
	peer->write_bytes += bytes;
	return written;
}

void
ircd::server::link::wait_readable()
{
	if(op_read || op_fini)
		return;

	assert(ready());
	op_read = true;
	const unwind::exceptional unhandled{[this]
	{
		op_read = false;
	}};

	auto handler
	{
		std::bind(&link::handle_readable, this, ph::_1)
	};

	net::wait(*socket, net::ready::READ, std::move(handler));
}

void
ircd::server::link::handle_readable(const error_code &ec)
try
{
	using std::errc;

	op_read = false;

	if(unlikely(finished()))
	{
		assert(peer);
		return peer->handle_finished(*this);
	}

	if(system_category(ec)) switch(ec.value())
	{
		case 0:
			handle_readable_success();
			return;

		case int(errc::operation_canceled):
			return;

		default:
			break;
	}

	throw std::system_error{ec};
}
catch(const std::system_error &e)
{
	assert(peer);
	peer->handle_error(*this, e);
}
catch(...)
{
	assert(peer);
	peer->handle_error(*this, std::current_exception());
}

/// Process as many read operations from as many tags as possible
void
ircd::server::link::handle_readable_success()
{
	if(queue.empty())
	{
		discard_read();
		wait_readable();
		return;
	}

	// Data pointed to by overrun will remain intact between iterations
	// because this loop isn't executing in any ircd::ctx.
	const_buffer overrun; do
	{
		if(!process_read(overrun))
		{
			wait_readable();
			return;
		}
	}
	while(!queue.empty());

	assert(peer);
	peer->handle_link_done(*this);
}

/// Process as many read operations for one tag as possible
bool
ircd::server::link::process_read(const_buffer &overrun)
try
{
	auto &tag
	{
		queue.front()
	};

	if(!tag.committed())
	{
		// Tag hasn't sent its data yet, we shouldn't have anything for it
		assert(empty(overrun));
		discard_read(); // Should stumble on a socket error.
		return false;
	}

	if(tag.canceled() && tag_committed() <= 1)
	{
		log::debug
		{
			log, "link(%p) closing to interrupt canceled committed tag(%p) of %zu",
			this,
			&tag,
			tag_count()
		};

		close();
		return false;
	}

	bool done{false}; do
	{
		overrun = process_read_next(overrun, tag, done);
	}
	while(!done);

	assert(peer);
	peer->handle_tag_done(*this, tag);
	assert(!queue.empty());
	queue.pop_front();
	return true;
}
catch(const buffer_overrun &e)
{
	queue.pop_front();
	throw;
}
catch(const std::system_error &e)
{
	using std::errc;

	if(system_category(e.code())) switch(e.code().value())
	{
		case 0:
			assert(0);
			return true;

		case int(errc::resource_unavailable_try_again):
			return false;

		default:
			break;
	}

	throw;
}

/// Process one read operation for one tag
ircd::const_buffer
ircd::server::link::process_read_next(const const_buffer &underrun,
                                      tag &tag,
                                      bool &done)
try
{
	const mutable_buffer buffer
	{
		tag.make_read_buffer()
	};

	const size_t copied
	{
		copy(buffer, underrun)
	};

	const mutable_buffer remaining
	{
		data(buffer) + copied, size(buffer) - copied
	};

	const const_buffer view
	{
		read(remaining)
	};

	const const_buffer overrun
	{
		tag.read_buffer(view, done, *this)
	};

	assert(done || empty(overrun));
	return overrun;
}
catch(const buffer_overrun &)
{
	tag.set_exception(std::current_exception());
	throw;
}

/// Read directly off the link's socket into buf
ircd::const_buffer
ircd::server::link::read(const mutable_buffer &buf)
{
	assert(!empty(buf));
	const size_t received
	{
		read_one(*socket, buf)
	};

	assert(peer);
	peer->read_bytes += received;

	assert(received);
	return const_buffer
	{
		data(buf), received
	};
}

void
ircd::server::link::discard_read()
{
	assert(socket);
	ssize_t discard
	{
		SSL_pending(socket->ssl.native_handle())
	};

	if(discard <= 0 && queue.empty())
		discard = available(*socket);

	if(discard <= 0 && !queue.empty())
		discard = 1;

	const size_t discarded
	{
		discard_any(*socket, size_t(discard))
	};

	assert(peer);
	peer->read_bytes += discarded;

	// Shouldn't ever be hit because the read() within discard() throws
	// the pending error like an eof.
	const fmt::snstringf msg
	{
		512, "peer(%p %s) link(%p q:%zu) socket(%s) discarded %zu of %zd unexpected bytes",
		peer,
		peer?
			peer->hostcanon:
			std::string{},
		this,
		queue.size(),
		likely(peer)?
			string(peer->remote):
		socket?
			string(remote_ipport(*socket)):
			std::string{},
		discarded,
		discard
	};

	log::warning
	{
		log, "%s", string_view{msg}
	};

	// just in case so this doesn't get loopy with discarding zero with
	// an empty queue...
	if(unlikely(!discard && !discarded))
		throw panic
		{
			"%s", string_view{msg}
		};
}

size_t
ircd::server::link::tag_uncommitted()
const
{
	return tag_count() - tag_committed();
}

size_t
ircd::server::link::tag_committed()
const
{
	return accumulate_tags([](const auto &tag)
	{
		return tag.committed();
	});
}

size_t
ircd::server::link::tag_count()
const
{
	return queue.size();
}

size_t
ircd::server::link::read_total()
const
{
	return socket? socket->in.bytes : 0;
}

size_t
ircd::server::link::write_total()
const
{
	return socket? socket->out.bytes : 0;
}

size_t
ircd::server::link::read_remaining()
const
{
	return accumulate_tags([](const auto &tag)
	{
		return tag.read_remaining();
	});
}

size_t
ircd::server::link::read_completed()
const
{
	return accumulate_tags([](const auto &tag)
	{
		return tag.read_completed();
	});
}

size_t
ircd::server::link::read_size()
const
{
	return accumulate_tags([](const auto &tag)
	{
		return tag.read_size();
	});
}

size_t
ircd::server::link::write_remaining()
const
{
	return accumulate_tags([](const auto &tag)
	{
		return tag.write_remaining();
	});
}

size_t
ircd::server::link::write_completed()
const
{
	return accumulate_tags([](const auto &tag)
	{
		return tag.write_completed();
	});
}

size_t
ircd::server::link::write_size()
const
{
	return accumulate_tags([](const auto &tag)
	{
		return tag.write_size();
	});
}

bool
ircd::server::link::busy()
const
{
	return !queue.empty();
}

bool
ircd::server::link::ready()
const
{
	return opened() && !op_init && !op_fini;
}

bool
ircd::server::link::opened()
const noexcept
{
	return bool(socket) && net::opened(*socket);
}

bool
ircd::server::link::finished()
const
{
	if(!bool(socket))
		return true;

	return !opened() && op_fini && !op_init && !op_write && !op_read;
}

size_t
ircd::server::link::tag_commit_max()
const
{
	return tag_commit_max_default;
}

size_t
ircd::server::link::tag_max()
const
{
	return tag_max_default;
}

template<class F>
size_t
ircd::server::link::accumulate_tags(F&& closure)
const
{
	return std::accumulate(begin(queue), end(queue), size_t(0), [&closure]
	(auto ret, const auto &tag)
	{
		return ret += closure(tag);
	});
}

//
// tag
//

/// This is tricky. When a user cancels a request which has committed some
/// writes to the remote we have to continue to service it through to
/// completion without disrupting the linearity of the link's pipeline
/// and causing trouble with other requests. This all depends on what phase
/// the request is currently in.
///
/// In any case, the goal here is to swap out the user's request buffers
/// and replace them with cancellation buffers which will be transparent
/// to the link as it completes the request.
void
ircd::server::cancel(request &request,
                     tag &tag)
noexcept
{
	// Must have a fully associated request/tag which has committed some
	// data to the wire to enter this routine.
	assert(tag.committed());
	assert(!tag.canceled());
	assert(request.tag == &tag);
	assert(tag.request == &request);

	// Disassociate the user's request and add our dummy request in its place.
	disassociate(request, tag);

	assert(tag.request == nullptr);
	tag.request = new server::request{};
	tag.request->tag = &tag;

	// Setup the cancellation buffers by mirroring the current state of the
	// user's buffers.

	const size_t cancellation_size
	{
		size(request.out) + size(request.in)
	};

	tag.cancellation = std::make_unique<char[]>(cancellation_size);
	char *ptr{tag.cancellation.get()};

	const mutable_buffer out_head{ptr, size(request.out.head)};
	tag.request->out.head = out_head;
	ptr += size(out_head);

	const mutable_buffer out_content{ptr, size(request.out.content)};
	tag.request->out.content = out_content;
	ptr += size(out_content);

	const mutable_buffer in_head{ptr, size(request.in.head)};
	tag.request->in.head = in_head;
	ptr += size(in_head);

	const mutable_buffer in_content{ptr, size(request.in.content)};
	// The nullity (btw that's a real word) of in.content has to be preserved
	// between the user's tag and the cancellation tag. This is important for
	// a dynamic chunked encoded response which has null in.content until done.
	if(!null(request.in.content))
	{
		tag.request->in.content = in_content;
		ptr += size(in_content);
	}
	else tag.request->in.content = request.in.content;

	assert(size_t(std::distance(tag.cancellation.get(), ptr)) == cancellation_size);

	// If the head is not completely written we have to copy the remainder from where
	// the socket left off.
	if(tag.state.written < size(request.out.head))
	{
		const const_buffer src
		{
			data(request.out.head) + tag.state.written, size(request.out.head) - tag.state.written
		};

		const mutable_buffer dst
		{
			out_head + tag.state.written
		};

		copy(dst, src);
	}

	// If the content is not completely written we have to copy the remainder from where
	// the socket left off.
	const size_t content_written
	{
		tag.state.written > size(request.out.head)? tag.state.written - size(request.out.head) : 0
	};

	if(content_written < size(request.out.content))
	{
		const const_buffer src
		{
			data(request.out.content) + content_written, size(request.out.content) - content_written
		};

		const mutable_buffer dst
		{
			out_content + content_written
		};

		copy(dst, src);
	}

	// If the head is not completely read we have to copy what's been received so far so
	// we can parse a coherent head.
	if(tag.state.head_read > 0 && tag.state.head_read < size(request.in.head))
	{
		const const_buffer src
		{
			data(request.in.head), tag.state.head_read
		};

		const mutable_buffer dst
		{
			data(in_head), size(in_head)
		};

		copy(dst, src);
	}

	// Normally we have no reason to copy content, but there is one exception:
	// If the content is chunked encoding and the tag is in the phase of
	// receiving the chunk head we have to copy what's been received of that
	// head so far so the grammar can parse a coherent head to continue.
	if(tag.state.chunk_length == size_t(-1) && !null(request.in.content))
	{
		const const_buffer src
		{
			data(request.in.content) + tag.state.content_length,
			tag.state.content_read - tag.state.content_length
		};

		const mutable_buffer dst
		{
			in_content + tag.state.content_length
		};

		copy(dst, src);
	}

	// Moving the dynamic buffer should have no real effect because the
	// cancellation buffer already took over for it. We could do it anyway
	// to prevent regressions but at the cost of maintaining twice the memory
	// allocated. For now it's commented to let it die with the user's req.
	//tag.request->in.dynamic = std::move(request.in.dynamic);

	// Moving the chunk vector is important to maintain the state of dynamic
	// chunk transfers through this cancel. There is no condition here for if
	// this is not a dynamic chunk transfer because it's trivial.
	tag.request->in.chunks = std::move(request.in.chunks);
}

void
ircd::server::associate(request &request,
                        tag &tag)
{
	assert(request.tag == nullptr);
	assert(tag.request == nullptr);

	auto &future
	{
		static_cast<ctx::future<http::code> &>(request)
	};

	future = tag.p;
	request.tag = &tag;
	tag.request = &request;
}

void
ircd::server::associate(request &request,
                        tag &cur,
                        tag &&old)
noexcept
{
	assert(request.tag == &old);         // ctor moved
	assert(cur.request == &request);     // ctor moved
	assert(old.request == &request);     // ctor didn't trash old

	cur.request = &request;
	old.request = nullptr;
	request.tag = &cur;
}

void
ircd::server::associate(request &cur,
                        tag &tag,
                        request &&old)
noexcept
{
	assert(tag.request == &old);   // ctor already moved
	assert(cur.tag == &tag);       // ctor already moved
	assert(old.tag == &tag);       // ctor didn't trash old

	cur.tag = &tag;
	tag.request = &cur;
	old.tag = nullptr;
}

void
ircd::server::disassociate(request &request,
                           tag &tag)
{
	assert(request.tag == &tag);
	assert(tag.request == &request);
	assert(tag.abandoned());

	request.tag = nullptr;
	tag.request = nullptr;

	// If the original request was canceled a new request was attached in its
	// place in addition to an cancellation buffer. The existence of this
	// cancellation buffer indicates that we must delete the request here.
	// This is a little hacky but it gets the job done.
	if(bool(tag.cancellation))
		delete &request;
}

void
ircd::server::tag::wrote_buffer(const const_buffer &buffer)
{
	assert(request);
	const auto &req{*request};
	state.written += size(buffer);

	if(state.written <= size(req.out.head))
	{
		assert(data(buffer) >= begin(req.out.head));
		assert(data(buffer) < end(req.out.head));
	}
	else if(state.written <= size(req.out.head) + size(req.out.content))
	{
		assert(data(buffer) >= begin(req.out.content));
		assert(data(buffer) < end(req.out.content));
		assert(state.written <= write_size());

		// Invoke the user's optional progress callback; this function
		// should be marked noexcept and has no reason to throw yet.
		if(req.out.progress)
			req.out.progress(buffer, const_buffer{data(req.out.content), state.written});
	}
	else
	{
		assert(0);
	}
}

ircd::const_buffer
ircd::server::tag::make_write_buffer()
const
{
	assert(request);
	const auto &req{*request};

	return
		state.written < size(req.out.head)?
			make_write_head_buffer():

		state.written < size(req.out.head) + size(req.out.content)?
			make_write_content_buffer():

		const_buffer{};
}

ircd::const_buffer
ircd::server::tag::make_write_head_buffer()
const
{
	assert(request);
	const auto &req{*request};

	const size_t remain
	{
		size(req.out.head) - state.written
	};

	const const_buffer window
	{
		data(req.out.head) + state.written, remain
	};

	return window;
}

ircd::const_buffer
ircd::server::tag::make_write_content_buffer()
const
{
	assert(request);
	const auto &req{*request};
	assert(state.written >= size(req.out.head));

	const size_t content_offset
	{
		state.written - size(req.out.head)
	};

	const size_t remain
	{
		size(req.out.head) + size(req.out.content) - state.written
	};

	const const_buffer window
	{
		data(req.out.content) + content_offset, remain
	};

	return window;
}

/// Called by the controller of the socket with a view of the data received by
/// the socket. The location and size of `buffer` is the same or smaller than
/// the buffer previously supplied by make_read_buffer().
///
/// Sometimes make_read_buffer() supplies a buffer that is too large, and some
/// data read off the socket does not belong to this tag. In that case, This
/// function returns a const_buffer viewing the portion of `buffer` which is
/// considered the "overrun," and the socket controller will copy that over to
/// the next tag.
///
/// The tag indicates it is entirely finished with receiving its data by
/// setting the value of `done` to true. Otherwise it is assumed false.
///
/// The link argument is not to be used to control/modify the link from the
/// tag; it's only a backreference to flash information to the link/peer
/// through specific callbacks so the peer can learn information.
///
ircd::const_buffer
ircd::server::tag::read_buffer(const const_buffer &buffer,
                               bool &done,
                               link &link)
{
	assert(request);

	if(state.status == (http::code)0)
		return read_head(buffer, done, link);

	if(state.chunk_length == size_t(-1) && null(request->in.content))
		return read_chunk_dynamic_head(buffer, done);

	if(state.chunk_length == size_t(-1))
		return read_chunk_head(buffer, done);

	if(state.chunk_length && null(request->in.content))
		return read_chunk_dynamic_content(buffer, done);

	if(state.chunk_length)
		return read_chunk_content(buffer, done);

	return read_content(buffer, done);
}

ircd::const_buffer
ircd::server::tag::read_head(const const_buffer &buffer,
                             bool &done,
                             link &link)
{
	assert(request);
	auto &req{*request};

	// informal search for head terminator
	static const string_view terminator{"\r\n\r\n"};
	const auto pos
	{
		string_view{buffer}.find(terminator)
	};

	// No terminator found; account for what was received in this buffer
	// for the next call to make_head_buffer() preparing for the subsequent
	// invocation of this function with more data.
	if(pos == string_view::npos)
	{
		state.head_read += size(buffer);
		return {};
	}

	// This indicates how much head was just received from this buffer only,
	// including the terminator which is considered part of the dome.
	const size_t addl_head_bytes
	{
		pos + size(terminator)
	};

	// The received buffer may go past the end of the head.
	assert(addl_head_bytes <= size(buffer));
	const size_t beyond_head_len
	{
		size(buffer) - addl_head_bytes
	};

	// The final update for the confirmed length of the head.
	state.head_read += addl_head_bytes;
	const size_t &head_read{state.head_read};
	assert(head_read + beyond_head_len <= size(req.in.head));

	// Window on any data in the buffer after the head.
	const const_buffer beyond_head
	{
		data(req.in.head) + head_read, beyond_head_len
	};

	// Before changing the user's head buffer, we branch for a feature that
	// allows the user to receive head and content into a single contiguous
	// buffer by assigning in.content = in.head.
	const bool contiguous
	{
		data(req.in.content) == data(req.in.head)
	};

	// Alternatively branch for a feature that allows dynamic allocation of
	// the content buffer if the user did not specify any buffer.
	const bool dynamic
	{
		!contiguous && empty(req.in.content)
	};

	// Resize the user's head buffer tight to the head; this is how we convey
	// the size of the dome back to the user.
	state.head_rem = size(req.in.head) - head_read;
	req.in.head = mutable_buffer
	{
		data(req.in.head), head_read
	};

	// Setup the capstan and mark the end of the tape
	parse::buffer pb{req.in.head};
	parse::capstan pc{pb};
	pc.read += size(req.in.head);

	// Play the tape through the formal grammar.
	const http::response::head head{pc};
	assert(pb.completed() == head_read);
	state.status = http::status(head.status);
	state.content_length = head.content_length;

	// Proffer the HTTP head to the peer instance which owns the link working
	// this tag so it can learn from any header data.
	assert(link.peer);
	link.peer->handle_head_recv(link, *this, head);

	if(contiguous)
	{
		const auto content_max
		{
			std::max(ssize_t(size(req.in.content) - head_read), ssize_t(0))
		};

		req.in.content = mutable_buffer
		{
			data(req.in.head) + head_read, size_t(content_max)
		};
	}

	// Branch for starting chunked encoding. We feed it whatever we have from
	// beyond the head as whole or part (or none) of the first chunk. Similar
	// to the non-chunked routine below, beyond_head may include all of the
	// chunk content and then part of the next message too: read_chunk_head
	// will return anything beyond this message as overrun and indicate done.
	if(head.transfer_encoding == "chunked")
	{
		if(dynamic)
		{
			assert(req.opt);
			req.in.chunks.reserve(req.opt->chunks_reserve);
		}

		const const_buffer chunk
		{
			!dynamic?
				const_buffer{data(req.in.content), move(req.in.content, beyond_head)}:
				beyond_head
		};

		state.chunk_length = -1;
		const const_buffer overrun
		{
			!dynamic?
				read_chunk_head(chunk, done):
				read_chunk_dynamic_head(chunk, done)
		};

		assert(empty(overrun) || done == true);
		return overrun;
	}

	// If no branch taken the rest of this function expects a content length
	// to be known from the received head.
	if(head.transfer_encoding)
		throw error
		{
			"Unsupported transfer-encoding '%s'", head.transfer_encoding
		};

	if(dynamic)
	{
		assert(req.opt);
		const size_t alloc_size
		{
			std::min(state.content_length, req.opt->content_length_maxalloc)
		};

		req.in.dynamic = unique_buffer<mutable_buffer>{alloc_size};
		req.in.content = req.in.dynamic;
	}

	// Now we check how much content was received beyond the head
	const size_t &content_read
	{
		std::min(state.content_length, beyond_head_len)
	};

	// Now we know how much bleed into the next message was also received
	assert(beyond_head_len >= content_read);
	const size_t beyond_content_len
	{
		beyond_head_len - content_read
	};

	const const_buffer partial_content
	{
		data(req.in.head) + head_read, content_read
	};

	// Anything remaining is not our response and must be given back.
	const const_buffer overrun
	{
		data(beyond_head) + size(partial_content), beyond_content_len
	};

	// Reduce the user's content buffer to the content-length. This is sort of
	// how we convey the content-length back to the user. The buffer size will
	// eventually reflect how much content was actually received; the user can
	// find the given content-length by parsing the header.
	req.in.content = mutable_buffer
	{
		data(req.in.content), std::min(state.content_length, size(req.in.content))
	};

	// Any partial content was written to the head buffer by accident,
	// that may have to be copied over to the content buffer.
	if(!empty(partial_content) && !contiguous)
		copy(req.in.content, partial_content);

	// Invoke the read_content() routine which will increment this->content_read
	read_content(partial_content, done);
	assert(state.content_read == size(partial_content));
	assert(state.content_read == state.content_length || !done);

	return overrun;
}

ircd::const_buffer
ircd::server::tag::read_content(const const_buffer &buffer,
                                bool &done)
{
	assert(request);
	auto &req{*request};
	const auto &content{req.in.content};

	// The amount of remaining content for the response sequence
	assert(size(content) + content_overflow() >= state.content_read);
	assert(size(content) + content_overflow() == state.content_length);
	const size_t remaining
	{
		size(content) + content_overflow() - state.content_read
	};

	// The amount of content read in this buffer only.
	const size_t addl_content_read
	{
		std::min(size(buffer), remaining)
	};

	state.content_read += addl_content_read;
	assert(size(buffer) - addl_content_read == 0);
	assert(state.content_read <= size(content) + content_overflow());
	assert(state.content_read <= state.content_length);

	// Invoke the user's optional progress callback; this function
	// should be marked noexcept for the time being.
	if(req.in.progress)
		req.in.progress(buffer, const_buffer{data(content), state.content_read});

	if(state.content_read == size(content) + content_overflow())
	{
		assert(!done);
		done = true;

		assert(req.opt);
		assert(state.content_read == state.content_length);
		if(content_overflow() && !req.opt->truncate_content)
		{
			assert(state.content_read > size(content));
			set_exception<buffer_overrun>
			(
				"buffer of %zu bytes too small for content-length %zu bytes by %zu bytes",
				size(content),
				state.content_length,
				content_overflow()
			);
		}
		else set_value(state.status);
	}

	return {};
}

ircd::const_buffer
ircd::server::tag::read_chunk_head(const const_buffer &buffer,
                                   bool &done,
                                   const uint8_t recursion_level)
{
	assert(request);
	auto &req{*request};
	const auto &content{req.in.content};

	// informal search for head terminator
	static const string_view terminator{"\r\n"};
	const auto pos
	{
		string_view{buffer}.find(terminator)
	};

	if(pos == string_view::npos)
	{
		state.content_read += size(buffer);
		return {};
	}

	// This indicates how much head was just received from this buffer only.
	const size_t addl_head_bytes
	{
		pos + size(terminator)
	};

	// The received buffer may go past the end of the head.
	assert(addl_head_bytes <= size(buffer));
	const size_t beyond_head_length
	{
		size(buffer) - addl_head_bytes
	};

	// The total head length is found from the end of the last chunk content
	state.content_read += addl_head_bytes;
	assert(state.content_read > state.content_length);
	const size_t head_length
	{
		state.content_read - state.content_length
	};

	// Window on any data in the buffer after the head.
	const const_buffer beyond_head
	{
		data(content) + state.content_length + head_length, beyond_head_length
	};

	// Setup the capstan and mark the end of the tape
	parse::buffer pb
	{
		mutable_buffer
		{
			data(content) + state.content_length, head_length
		}
	};
	parse::capstan pc{pb};
	pc.read += head_length;

	// Play the tape through the formal grammar.
	const http::response::chunk chunk{pc};
	state.chunk_length = chunk.size + size(terminator);

	// Now we check how much chunk was received beyond the head
	const auto &chunk_read
	{
		std::min(state.chunk_length, beyond_head_length)
	};

	// Now we know how much bleed into the next message was also received
	assert(beyond_head_length >= chunk_read);
	const size_t beyond_chunk_length
	{
		beyond_head_length - chunk_read
	};

	// Finally we erase the chunk head by replacing it with everything received
	// after it.
	const mutable_buffer target
	{
		data(content) + state.content_length, beyond_head_length
	};

	move(target, beyond_head);

	// Increment the content_length to now include this chunk
	state.content_length += state.chunk_length;

	// Adjust the content_read to erase the chunk head.
	state.content_read -= head_length;

	const const_buffer partial_chunk
	{
		data(target), chunk_read
	};

	const const_buffer overrun
	{
		data(target) + chunk_read, beyond_chunk_length
	};

	assert(state.chunk_length >= 2);
	read_chunk_content(partial_chunk, done);

	if(done)
		return overrun;

	// Prevent stack overflow from lots of tiny chunks nagled together.
	if(unlikely(recursion_level >= 32))
		throw error
		{
			"Chunking recursion limit exceeded"
		};

	return read_chunk_head(overrun, done, recursion_level + 1);
}

ircd::const_buffer
ircd::server::tag::read_chunk_content(const const_buffer &buffer,
                                      bool &done)
{
	assert(request);
	auto &req{*request};
	const auto &content{req.in.content};

	// The amount of remaining content for the response sequence
	const size_t remaining
	{
		content_remaining()
	};

	// The amount of content read in this buffer only.
	const size_t addl_content_read
	{
		std::min(size(buffer), remaining)
	};

	// Increment the read counters for this chunk and all chunks.
	state.chunk_read += addl_content_read;
	state.content_read += addl_content_read;
	assert(state.chunk_read <= state.content_read);

	if(state.content_read == state.content_length)
	{
		// This branch is taken at the completion of a chunk. The size
		// all the buffers is rolled back to hide the terminator so it's
		// either ignored or overwritten so it doesn't leak to the user.
		static const string_view terminator{"\r\n"};
		assert(state.content_length >= size(terminator));
		state.content_length -= size(terminator);
		state.content_read -= size(terminator);

		assert(state.chunk_length >= 2);
		assert(state.chunk_read == state.chunk_length);
		state.chunk_length -= size(terminator);
		state.chunk_read -= size(terminator);

		if(state.chunk_length == 0)
		{
			assert(state.chunk_read == 0);
			assert(!done);
			done = true;
			req.in.content = mutable_buffer{data(req.in.content), state.content_length};
			set_value(state.status);
		}
	}

	// Invoke the user's optional progress callback; this function
	// should be marked noexcept for the time being.
	if(req.in.progress && !done)
		req.in.progress(buffer, const_buffer{data(content), state.content_read});

	if(state.content_read == state.content_length)
	{
		assert(state.chunk_read == state.chunk_length);
		assert(state.chunk_read <= state.content_read);
		state.chunk_length = size_t(-1);
		state.chunk_read = 0;
	}

	return {};
}

ircd::const_buffer
ircd::server::tag::read_chunk_dynamic_head(const const_buffer &buffer,
                                           bool &done,
                                           const uint8_t recursion_level)
{
	assert(request);
	auto &req{*request};

	// informal search for head terminator
	static const string_view terminator{"\r\n"};
	const auto pos
	{
		string_view{buffer}.find(terminator)
	};

	if(pos == string_view::npos)
	{
		state.chunk_read += size(buffer);
		state.content_read += size(buffer);
		return {};
	}

	// This indicates how much head was just received from this buffer only.
	const size_t addl_head_bytes
	{
		pos + size(terminator)
	};

	// The received buffer may go past the end of the head.
	assert(addl_head_bytes <= size(buffer));
	const size_t beyond_head_length
	{
		size(buffer) - addl_head_bytes
	};

	state.chunk_read += addl_head_bytes;
	const auto head_length{state.chunk_read};
	state.chunk_read = 0;

	// Window on any data in the buffer after the head.
	const const_buffer beyond_head
	{
		data(buffer) + addl_head_bytes, beyond_head_length
	};

	// Setup the capstan and mark the end of the tape
	parse::buffer pb
	{
		mutable_buffer
		{
			data(req.in.head) + state.head_read, head_length
		}
	};
	parse::capstan pc{pb};
	pc.read += head_length;

	// Play the tape through the formal grammar.
	const http::response::chunk chunk{pc};
	assert(state.chunk_length == size_t(-1));
	state.chunk_length = chunk.size + size(terminator);

	// Increment the content_length to now include this chunk
	state.content_length += state.chunk_length;

	// Allocate the chunk content on the vector.
	//TODO: maxalloc
	req.in.chunks.emplace_back(state.chunk_length);

	// Now we check how much chunk was received beyond the head
	// state.chunk_head is still 0 here because that's only incremented
	// in the content read function.
	const auto &chunk_read
	{
		std::min(state.chunk_length, beyond_head_length)
	};

	// Now we know how much bleed into the next message was also received
	assert(beyond_head_length >= chunk_read);
	const size_t beyond_chunk_length
	{
		beyond_head_length - chunk_read
	};

	const const_buffer partial_chunk
	{
		data(beyond_head), chunk_read
	};

	const size_t copied
	{
		copy(req.in.chunks.back(), partial_chunk)
	};

	const const_buffer overrun
	{
		data(beyond_head) + chunk_read, beyond_chunk_length
	};

	assert(state.chunk_length >= 2);
	read_chunk_dynamic_content(partial_chunk, done);

	if(done)
		return overrun;

	// Prevent stack overflow from lots of tiny chunks nagled together.
	if(unlikely(recursion_level >= 32))
		throw error
		{
			"Chunking recursion limit exceeded"
		};

	return read_chunk_dynamic_head(overrun, done, recursion_level + 1);
}

ircd::const_buffer
ircd::server::tag::read_chunk_dynamic_content(const const_buffer &buffer,
                                              bool &done)
{
	assert(request);
	auto &req{*request};

	assert(state.chunk_length != size_t(-1));
	assert(null(req.in.content));
	assert(!req.in.chunks.empty());
	const auto &chunk
	{
		req.in.chunks.back()
	};

	// The amount of remaining content for the response sequence
	assert(state.chunk_read <= size(chunk));
	const size_t remaining
	{
		size(chunk) - state.chunk_read
	};

	// The amount of content read in this buffer only.
	const size_t addl_content_read
	{
		std::min(size(buffer), remaining)
	};

	// Increment the read counters for this chunk and all chunks.
	state.chunk_read += addl_content_read;
	state.content_read += addl_content_read;
	assert(state.chunk_read <= state.content_read);

	if(state.chunk_read == state.chunk_length)
	{
		static const string_view terminator{"\r\n"};
		state.content_length -= size(terminator);
		state.content_read -= size(terminator);

		assert(state.chunk_length >= 2);
		assert(state.chunk_read == state.chunk_length);
		state.chunk_length -= size(terminator);
		state.chunk_read -= size(terminator);

		auto &chunk{req.in.chunks.back()};
		std::get<1>(chunk) -= size(terminator);
		assert(size(chunk) == state.chunk_length);
		assert(std::get<0>(chunk) <= std::get<1>(chunk));

		if(state.chunk_length == 0)
		{
			assert(state.chunk_read == 0);
			assert(!done);
			done = true;

			assert(req.opt);
			if(req.opt->contiguous_content)
			{
				assert(state.content_length == size_chunks(req.in));
				assert(req.in.chunks.size() >= 1);
				assert(empty(req.in.chunks.back()));
				req.in.chunks.pop_back();

				if(req.in.chunks.size() > 1)
				{
					req.in.dynamic = size_chunks(req.in);
					req.in.content = req.in.dynamic;

					size_t copied{0};
					for(const auto &buffer : req.in.chunks)
						copied += copy(req.in.content + copied, buffer);

					assert(copied == size(req.in.content));
					assert(copied == state.content_length);
				}
				else if(req.in.chunks.size() == 1)
				{
					req.in.dynamic = std::move(req.in.chunks.front());
					req.in.content = req.in.dynamic;
					assert(size(req.in.content) == state.content_length);
				}

				req.in.chunks.clear();
			}

			set_value(state.status);
		}
	}

	// Invoke the user's optional progress callback; this function
	// should be marked noexcept for the time being.
	if(req.in.progress && !done)
		req.in.progress(buffer, const_buffer{data(chunk), state.chunk_read});

	if(state.chunk_read == state.chunk_length)
	{
		assert(state.chunk_read == state.chunk_length);
		assert(state.chunk_read <= state.content_read);
		state.chunk_length = size_t(-1);
		state.chunk_read = 0;
	}

	return {};
}

/// An idempotent operation that provides the location of where the socket
/// should place the next received data. The tag figures this out based on
/// whether it receiving HTTP head data or whether it is in content mode.
///
ircd::mutable_buffer
ircd::server::tag::make_read_buffer()
const
{
	assert(request);
	assert(state.head_read <= size(request->in.head));
	assert(state.content_read <= state.content_length);

	if(state.status == (http::code)0)
		return make_read_head_buffer();

	if(state.chunk_length == size_t(-1) && null(request->in.content))
		return make_read_chunk_dynamic_head_buffer();

	if(state.chunk_length == size_t(-1))
		return make_read_chunk_head_buffer();

	if(state.chunk_length && null(request->in.content))
		return make_read_chunk_dynamic_content_buffer();

	if(state.chunk_length)
		return make_read_chunk_content_buffer();

	if(state.content_read >= size(request->in.content))
		return make_read_discard_buffer();

	return make_read_content_buffer();
}

ircd::mutable_buffer
ircd::server::tag::make_read_head_buffer()
const
{
	assert(request);
	const auto &req{*request};
	const auto &head{req.in.head};
	if(unlikely(size(req.in.head) <= state.head_read))
		throw buffer_overrun
		{
			"Supplied buffer of %zu too small for HTTP head", size(req.in.head)
		};

	const size_t remaining
	{
		size(head) - state.head_read
	};

	const mutable_buffer buffer
	{
		data(head) + state.head_read, remaining
	};

	assert(size(buffer) <= size(head));
	assert(size(buffer) > 0);
	return buffer;
}

ircd::mutable_buffer
ircd::server::tag::make_read_content_buffer()
const
{
	assert(request);
	const auto &req{*request};
	const auto &content{req.in.content};
	if(unlikely(size(content) <= state.content_read))
		throw buffer_overrun
		{
			"Content buffer of %zu bytes too small to read %zu bytes of content",
			size(content),
			state.content_length
		};

	// The amount of bytes we still have to read to for the response
	const size_t remaining
	{
		size(content) - state.content_read
	};

	assert(remaining > 0);
	return
	{
		data(content) + state.content_read, remaining
	};
}

/// The chunk head buffer starts after the last chunk ended and has a size of
/// the rest of the available content buffer (hopefully much less will be
/// needed). If only part of the chunk head was received previously this
/// function accounts for that by returning a buffer which starts at the
/// content_read offset (which is at the end of that previous read).
///
ircd::mutable_buffer
ircd::server::tag::make_read_chunk_head_buffer()
const
{
	assert(request);
	assert(state.chunk_length == size_t(-1));
	assert(state.content_read >= state.content_length);

	const auto &req{*request};
	const auto &content{req.in.content};

	if(unlikely(size(content) <= state.content_read))
		throw buffer_overrun
		{
			"Content buffer of %zu bytes too small to read next chunk header",
			size(content)
		};

	const size_t remaining
	{
		size(content) - state.content_read
	};

	const mutable_buffer buffer
	{
		data(content) + state.content_read, remaining
	};

	assert(size(buffer) > 0);
	return buffer;
}

ircd::mutable_buffer
ircd::server::tag::make_read_chunk_content_buffer()
const
{
	assert(request);
	assert(state.chunk_length > 0);
	assert(state.content_read <= state.content_length);

	const auto &req{*request};
	const auto &content{req.in.content};

	assert(size(content) >= state.content_read);
	const size_t buffer_remaining
	{
		size(content) - state.content_read
	};

	const size_t chunk_remaining
	{
		content_remaining()
	};

	assert(chunk_remaining <= state.chunk_length);
	assert(chunk_remaining == state.content_length - state.content_read);
	const size_t buffer_size
	{
		std::min(buffer_remaining, chunk_remaining)
	};

	if(unlikely(buffer_size < chunk_remaining))
		throw buffer_overrun
		{
			"Content buffer of %zu bytes too small to read remaining %zu of chunk",
			size(content),
			chunk_remaining
		};

	const mutable_buffer buffer
	{
		data(content) + state.content_read, buffer_size
	};

	assert(size(buffer) > 0);
	return buffer;
}

/// The dynamic chunk head buffer starts after the main head and has a size
/// of the remaining main head buffer. This area is overwritten for each
/// chunk head.
///
ircd::mutable_buffer
ircd::server::tag::make_read_chunk_dynamic_head_buffer()
const
{
	assert(request);
	const auto &req{*request};

	assert(state.chunk_length == size_t(-1));
	assert(null(req.in.content));
	assert(size(req.in.head) >= state.head_read);

	const size_t head_max
	{
		size(req.in.head) + state.head_rem
	};

	// The total offset in the head buffer is the message head plus the
	// amount of chunk head received so far, which is kept in chunk_read.
	const size_t head_offset
	{
		state.head_read + state.chunk_read
	};

	assert(head_max >= head_offset);
	if(unlikely(head_max - head_offset <= 16))
		throw buffer_overrun
		{
			"Remaining head buffer of %zu bytes too small to read next chunk header",
			head_max - state.head_read
		};

	const size_t remaining
	{
		head_max - head_offset
	};

	const mutable_buffer buffer
	{
		data(req.in.head) + state.head_read + state.chunk_read, remaining
	};

	assert(size(buffer) > 0);
	return buffer;
}

ircd::mutable_buffer
ircd::server::tag::make_read_chunk_dynamic_content_buffer()
const
{
	assert(request);
	const auto &req{*request};

	assert(state.chunk_length > 0);
	assert(state.content_read <= state.content_length);
	assert(null(req.in.content));
	assert(!req.in.chunks.empty());
	const auto &buffer
	{
		req.in.chunks.back()
	};

	assert(size(buffer) == state.chunk_length);
	assert(state.chunk_read <= size(buffer));
	const size_t buffer_remaining
	{
		size(buffer) - state.chunk_read
	};

	const mutable_buffer ret
	{
		data(buffer) + state.chunk_read, buffer_remaining
	};

	assert(size(ret) > 0);
	return ret;
}

ircd::mutable_buffer
ircd::server::tag::make_read_discard_buffer()
const
{
	assert(request);
	assert(content_overflow() > 0);
	assert(content_overflow() <= state.content_length);
	assert(state.content_read >= size(request->in.content));
	assert(state.content_length >= state.content_read);
	const size_t remaining
	{
		state.content_length - state.content_read
	};

	assert(remaining <= state.content_length);
	static char buffer[512];
	const size_t buffer_max
	{
		std::min(remaining, sizeof(buffer))
	};

	return
	{
		buffer, buffer_max
	};
}

size_t
ircd::server::tag::content_remaining()
const
{
	assert(state.content_length >= state.content_read);
	return state.content_length - state.content_read;
}

size_t
ircd::server::tag::content_overflow()
const
{
	assert(request);
	const auto &req{*request};
	const ssize_t diff(state.content_length - size(req.in.content));
	return std::max(diff, ssize_t(0));
}

template<class... args>
void
ircd::server::tag::set_value(args&&... a)
{
	if(abandoned())
		return;

	const http::code &code
	{
		std::forward<args>(a)...
	};

	assert(request->opt);
	if(request->opt->http_exceptions && code >= http::code(300))
	{
		const string_view content
		{
			data(request->in.content), size(request->in.content)
		};

		set_exception<http::error>(code, std::string{content});
		return;
	}

	p.set_value(code);
}

template<class E,
         class... args>
void
ircd::server::tag::set_exception(args&&... a)
try
{
	if(abandoned())
		return;

	throw E
	{
		std::forward<args>(a)...
	};
}
catch(const std::exception &e)
{
	set_exception(std::current_exception());
}

void
ircd::server::tag::set_exception(std::exception_ptr eptr)
{
	if(abandoned())
		return;

	p.set_exception(std::move(eptr));
}

bool
ircd::server::tag::abandoned()
const
{
	return !p.valid();
}

bool
ircd::server::tag::canceled()
const
{
	return bool(cancellation);
}

bool
ircd::server::tag::committed()
const
{
	return write_completed() > 0;
}

size_t
ircd::server::tag::read_remaining()
const
{
	return read_size() - read_completed();
}

size_t
ircd::server::tag::read_completed()
const
{
	return state.head_read + state.content_read;
}

size_t
ircd::server::tag::read_size()
const
{
	return state.head_read + state.content_length;
}

size_t
ircd::server::tag::write_remaining()
const
{
	return write_size() - write_completed();
}

size_t
ircd::server::tag::write_completed()
const
{
	return state.written;
}

size_t
ircd::server::tag::write_size()
const
{
	return request? size(request->out) : 0;
}
