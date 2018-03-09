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
	bool ready;         // like an /etc/nologin to prevent actions when false.
	ctx::dock dock;     // internal semaphore

	// Internal util
	template<class F> size_t accumulate_peers(F&&);
	template<class F> size_t accumulate_links(F&&);
	template<class F> size_t accumulate_tags(F&&);

	// Internal control
	std::shared_ptr<peer> create(const net::hostport &);
	void interrupt_all();
	void close_all();
}

decltype(ircd::server::log)
ircd::server::log
{
	"server", 'S'
};

decltype(ircd::server::peers)
ircd::server::peers
{};

ircd::server::peer &
ircd::server::get(const net::hostport &hostport)
{
	auto it(peers.lower_bound(host(hostport)));
	if(it == peers.end() || it->first != host(hostport))
	{
		auto peer{create(hostport)};
		log.debug("peer(%p) for %s created; adding...",
		          peer.get(),
		          string(hostport));

		const string_view key{peer->remote.hostname};
		it = peers.emplace_hint(it, key, std::move(peer));
	}

	return *it->second;
}

std::shared_ptr<ircd::server::peer>
ircd::server::create(const net::hostport &hostport)
{
	auto peer(std::make_shared<peer>());
	peer->remote.hostname = std::string{host(hostport)};
	peer->resolve(hostport);
	return peer;
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

ircd::conf::item<ircd::seconds>
close_all_timeout
{
	{ "name",     "ircd.server.close_all_timeout" },
	{ "default",  2L                              },
};

void
ircd::server::close_all()
{
	log.debug("Closing all %zu peers",
	          peer_count());

	net::close_opts opts;
	opts.timeout = seconds(close_all_timeout);

	for(auto &peer : peers)
		peer.second->close(opts);

	log.debug("Waiting for %zu tags on %zu links on %zu peers to close...",
	          tag_count(),
	          link_count(),
	          peer_count());

	while(link_count())
		dock.wait();
}

void
ircd::server::interrupt_all()
{
	log.debug("Interrupting %zu tags on %zu links on %zu peers",
	          tag_count(),
	          link_count(),
	          peer_count());

	for(auto &peer : peers)
		peer.second->interrupt();
}

//
// init
//

ircd::server::init::init()
{
	ready = true;
}

ircd::server::init::~init()
noexcept
{
	ready = false;
	close_all();
	peers.clear();
}

void
ircd::server::init::interrupt()
{
	interrupt_all();
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
	log.debug("cancel request(%p) tag(%p) commit:%d w:%zu hr:%zu cr:%zu",
	          &request,
	          &tag,
	          tag.committed(),
	          tag.written,
	          tag.head_read,
	          tag.content_read);
*/

	// We got off easy... The link's write loop won't start an abandoned
	// request. All that has to be done is indicate a full cancellation
	// immediately and the user will know nothing was revealed to the remote.
	if(!tag.committed())
	{
		tag.set_exception(canceled
		{
			"Request canceled"
		});

		return true;
	}

	// Now things aren't so easy. More complicated logic happens inside...
	cancel(request, tag);
	return true;
}

void
ircd::server::submit(const hostport &hostport,
                     request &request)
{
	if(unlikely(!server::ready))
		throw unavailable
		{
			"Unable to fulfill requests at this time."
		};

	assert(request.tag == nullptr);
	auto &peer(server::get(hostport));
	peer.submit(request);
}

//
// peer
//

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

ircd::server::peer::peer()
{
}

ircd::server::peer::~peer()
noexcept
{
	assert(links.empty());
}

void
ircd::server::peer::close(const net::close_opts &opts)
{
	ready = false;
	std::vector<link *> links(this->links.size());
	pointers(this->links, links);
	for(const auto &link : links)
		link->close(opts);
}

void
ircd::server::peer::interrupt()
{
	for(auto &link : this->links)
		link.cancel_all(std::make_exception_ptr(canceled
		{
			"Request was aborted due to interruption."
		}));
}

void
ircd::server::peer::err_clear()
{
	e.reset(nullptr);
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
	if(!ready)
		return false;

	if(!e)
		return true;

	//TODO: The specific error type should be switched and finer
	//TODO: timeouts should be used depending on the error: i.e
	//TODO: NXDOMAIN vs. temporary conn timeout, etc.
	if(e->etime + seconds(error_clear_default) > now<steady_point>())
		return false;

	err_clear();
	return true;
}

void
ircd::server::peer::submit(request &request)
try
{
	if(!err_check() || unlikely(!server::ready))
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
			"No link to peer %s available", remote.hostname
		};
	else
		request.tag->set_exception(unavailable
		{
			"No link to peer %s available", remote.hostname
		});
}
catch(const std::exception &e)
{
	if(request.tag)
		request.tag->set_exception(std::current_exception());
	else
		throw;
}

/// Dispatch algorithm here; finds the best link to place this request on,
/// or creates a new link entirely. There are a number of factors: foremost
/// if any special needs are indicated,
//
ircd::server::link *
ircd::server::peer::link_get(const request &request)
{
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
	if(e)
		std::rethrow_exception(e->eptr);

	links.emplace_back(*this);
	auto &link{links.back()};

	if(remote.resolved())
		link.open(remote);

	return link;
}

void
ircd::server::peer::handle_open(link &link,
                                std::exception_ptr eptr)
try
{
	if(eptr && links.size() == 1)
		err_set(eptr);

	if(eptr)
		std::rethrow_exception(eptr);
}
catch(const std::exception &e)
{
	log.error("peer(%p) link(%p) [%s]: open: %s",
	          this,
	          &link,
	          string(remote),
	          e.what());

	link.close(net::dc::RST);
}

void
ircd::server::peer::handle_close(link &link,
                                 std::exception_ptr eptr)
{
	if(eptr)
		log.error("peer(%p) link(%p) [%s]: close: %s",
		          this,
		          &link,
		          string(remote),
		          what(eptr));

	handle_finished(link);
}

void
ircd::server::peer::handle_error(link &link,
                                 std::exception_ptr eptr)
{
	assert(bool(eptr));
	link.cancel_committed(eptr);
	log.error("peer(%p) link(%p): %s",
	          this,
	          &link,
	          what(eptr));

	link.close(net::dc::RST);
}

void
ircd::server::peer::handle_error(link &link,
                                 const boost::system::system_error &e)
{
	using namespace boost::system::errc;
	using boost::system::system_category;
	using boost::asio::error::get_misc_category;

	const auto &ec{e.code()};
	if(ec.category() == system_category()) switch(ec.value())
	{
		case success:
			assert(0);
			break;

		default:
			break;
	}
	else if(ec.category() == get_misc_category()) switch(ec.value())
	{
		case asio::error::eof:
			log.debug("peer(%p) link(%p) [%s]: %s",
			          this,
			          &link,
			          string(remote),
			          e.what());

			link.close(net::close_opts_default);
			return;

		default:
			break;
	}

	log.error("peer(%p) link(%p) [%s]: error: %s",
	          this,
	          &link,
	          string(remote),
	          e.what());

	link.cancel_committed(std::make_exception_ptr(e));
	link.close(net::dc::RST);
}

void
ircd::server::peer::handle_finished(link &link)
{
	assert(link.op_fini);
	assert(!link.op_init);
	assert(!link.op_read);
	assert(!link.op_write);
	this->del(link);
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
	if(link.tag_committed() >= link.tag_commit_max())
		link.wait_writable();
}
catch(const std::exception &e)
{
	log.critical("peer(%p) link(%p) tag(%p) done; error: %s",
	             this,
	             &link,
	             &tag,
	             e.what());
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
		log.debug("peer(%p) learned %s is '%s'",
		          this,
		          string(remote),
		          server_name);
	}
}

void
ircd::server::peer::disperse(link &link)
{
	disperse_uncommitted(link);
	link.cancel_committed(std::make_exception_ptr(canceled
	{
		"Request was aborted; though it was partially completed"
	}));

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
		log.warning("peer(%p) failed to resubmit tag(%p): %s",
		            this,
		            &tag,
		            e.what());

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
	log.debug("peer(%p) removing link(%p) %zu of %zu to %s",
	          this,
	          &link,
	          std::distance(begin(links), it),
	          links.size(),
	          string(remote));

	links.erase(it);

	// Right now this is what the server:: ~init sequence needs
	// to wait for all links to close on IRCd shutdown.
	server::dock.notify_all();
}

void
ircd::server::peer::resolve(const hostport &hostport)
{
	auto handler
	{
		std::bind(&peer::handle_resolve, this, weak_from(*this), ph::_1, ph::_2)
	};

	net::dns(hostport, std::move(handler));
}

void
ircd::server::peer::handle_resolve(std::weak_ptr<peer> wp,
                                   std::exception_ptr eptr,
                                   const ipport &ipport)
try
{
	const life_guard<peer> lg(wp);

	if(eptr)
	{
		err_set(eptr);
		std::rethrow_exception(eptr);
	}

	static_cast<net::ipport &>(this->remote) = ipport;
	for(auto &link : links)
		link.open(this->remote);
}
catch(const std::bad_weak_ptr &)
{
	return;
}
catch(const std::exception &e)
{
	assert(!wp.expired());
	log.error("peer(%p): during name resolution: %s",
	          this,
	          e.what());

	close();
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
ircd::server::peer::read_total()
const
{
	return accumulate_links([](const auto &link)
	{
		return link.read_total();
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
ircd::server::peer::write_total()
const
{
	return accumulate_links([](const auto &link)
	{
		return link.write_total();
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
:peer{shared_from(peer)}
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

	if(peer)
		peer->handle_close(*this, std::move(eptr));
}

void
ircd::server::link::wait_writable()
{
	if(op_write)
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
	using namespace boost::system::errc;
	using boost::system::system_category;

	op_write = false;
	if(ec.category() == system_category()) switch(ec.value())
	{
		case success:
			handle_writable_success();
			return;

		case operation_canceled:
			return;

		default:
			break;
	}

	throw boost::system::system_error{ec};
}
catch(const boost::system::system_error &e)
{
	if(peer)
		peer->handle_error(*this, e);
}
catch(const std::exception &e)
{
	if(peer)
	{
		peer->handle_error(*this, std::current_exception());
		return;
	}

	throw assertive
	{
		"link::handle_writable(): %s", e.what()
	};
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
			log.debug("link(%p) discarding canceled:%d abandoned:%d uncommitted tag %zu of %zu",
			          this,
			          tag.canceled(),
			          tag.abandoned(),
			          tag_committed(),
			          tag_count());

			it = queue.erase(it);
			continue;
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
		log.debug("link(%p) starting on tag %zu of %zu: wt:%zu",
		          this,
		          tag_committed(),
		          tag_count(),
		          tag.write_total());

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

	return written;
}

void
ircd::server::link::wait_readable()
{
	if(op_read)
		return;

	auto handler
	{
		std::bind(&link::handle_readable, this, ph::_1)
	};

	assert(ready());
	op_read = true;
	const unwind::exceptional unhandled{[this]
	{
		op_read = false;
	}};

	net::wait(*socket, net::ready::READ, std::move(handler));
}

void
ircd::server::link::handle_readable(const error_code &ec)
try
{
	using namespace boost::system::errc;
	using boost::system::system_category;

	op_read = false;
	if(ec.category() == system_category()) switch(ec.value())
	{
		case success:
			handle_readable_success();
			return;

		case operation_canceled:
			return;

		default:
			break;
	}

	throw boost::system::system_error{ec};
}
catch(const boost::system::system_error &e)
{
	if(peer)
		peer->handle_error(*this, e);
}
catch(const std::exception &e)
{
	if(peer)
	{
		peer->handle_error(*this, std::current_exception());
		return;
	}

	throw assertive
	{
		"link::handle_readable(): %s", e.what()
	};
}

/// Process as many read operations from as many tags as possible
void
ircd::server::link::handle_readable_success()
{
	if(queue.empty())
		return discard_read();

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
		return false;
	}

	bool done{false}; do
	{
		overrun = process_read_next(overrun, tag, done);
	}
	while(!done);

	assert(peer);
	peer->handle_tag_done(*this, queue.front());
	queue.pop_front();
	return true;
}
catch(const buffer_overrun &e)
{
	queue.pop_front();
	throw;
}
catch(const boost::system::system_error &e)
{
	using namespace boost::system::errc;

	switch(e.code().value())
	{
		case resource_unavailable_try_again:
			return false;

		case success:
			assert(0);
			return true;

		default:
			throw;
	}
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

	const size_t received
	{
		read_one(*socket, remaining)
	};

	const const_buffer view
	{
		data(buffer), copied + received
	};

	const const_buffer overrun
	{
		tag.read_buffer(view, done, *this)
	};

	assert(done || empty(overrun));
	return overrun;
}
catch(const buffer_overrun &e)
{
	tag.set_exception(e);
	throw;
}

void
ircd::server::link::discard_read()
{
	const size_t discard
	{
		available(*socket)
	};

	const size_t discarded
	{
		discard_any(*socket, discard)
	};

	// Shouldn't ever be hit because the read() within discard() throws
	// the pending error like an eof.
	log.warning("Link discarded %zu of %zu unexpected bytes",
	            discard,
	            discarded);

	// just in case so this doesn't get loopy with discarding zero with
	// an empty queue...
	if(unlikely(!discard || !discarded))
		throw assertive
		{
			"Queue is empty and nothing to discard."
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
ircd::server::link::read_total()
const
{
	return accumulate_tags([](const auto &tag)
	{
		return tag.read_total();
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
ircd::server::link::write_total()
const
{
	return accumulate_tags([](const auto &tag)
	{
		return tag.write_total();
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
	assert(request.tag == &tag);
	assert(tag.request == &request);

	// Disassociate the user's request and add our dummy request in its place.

	request.tag = nullptr;
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
	tag.request->in.content = in_content;
	ptr += size(in_content);

	assert(size_t(std::distance(tag.cancellation.get(), ptr)) == cancellation_size);

	// If the head is not completely written we have to copy the remainder from where
	// the socket left off.
	if(tag.written < size(request.out.head))
	{
		const const_buffer src
		{
			data(request.out.head) + tag.written, size(request.out.head) - tag.written
		};

		const mutable_buffer dst
		{
			data(out_head) + tag.written, size(src)
		};

		copy(dst, src);
	}

	// If the content is not completely written we have to copy the remainder from where
	// the socket left off.
	const size_t content_written
	{
		tag.written > size(request.out.head)? tag.written - size(request.out.head) : 0
	};

	if(content_written < size(request.out.content))
	{
		const const_buffer src
		{
			data(request.out.content) + content_written, size(request.out.content) - content_written
		};

		const mutable_buffer dst
		{
			data(out_content) + content_written, size(src)
		};

		copy(dst, src);
	}

	// If the head is not completely read we have to copy what's been received so far so
	// we can parse a coherent head.
	if(tag.head_read > 0 && tag.head_read < size(request.in.head))
	{
		const const_buffer src
		{
			data(request.in.head), tag.head_read
		};

		const mutable_buffer dst
		{
			data(in_head), size(src)
		};

		copy(dst, src);
	}

	// No received content is copied.
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

	request.tag = nullptr;
	tag.request = nullptr;

	// If the original request was canceled a new request was attached in its
	// place in addition to an cancellation buffer. The existence of this
	// cancellation buffer indicates that we must delete the request here.
	// This is a little hacky but it gets the job done.
	if(bool(tag.cancellation))
		delete &request;
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

	return
		head_read < size(request->in.head)?
			read_head(buffer, done, link):

		read_content(buffer, done);
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

	return
		head_read < size(request->in.head)?
			make_read_head_buffer():

		content_read >= size(request->in.content)?
			make_read_discard_buffer():

		make_read_content_buffer();
}

void
ircd::server::tag::wrote_buffer(const const_buffer &buffer)
{
	assert(request);
	const auto &req{*request};
	written += size(buffer);

	if(written <= size(req.out.head))
	{
		assert(data(buffer) >= begin(req.out.head));
		assert(data(buffer) < end(req.out.head));
	}
	else if(written <= size(req.out.head) + size(req.out.content))
	{
		assert(data(buffer) >= begin(req.out.content));
		assert(data(buffer) < end(req.out.content));
		assert(written <= write_total());

		// Invoke the user's optional progress callback; this function
		// should be marked noexcept and has no reason to throw yet.
		if(req.out.progress)
			req.out.progress(buffer, const_buffer{data(req.out.content), written});
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
		written < size(req.out.head)?
			make_write_head_buffer():

		written < size(req.out.head) + size(req.out.content)?
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
		size(req.out.head) - written
	};

	const const_buffer window
	{
		data(req.out.head) + written, remain
	};

	return window;
}

ircd::const_buffer
ircd::server::tag::make_write_content_buffer()
const
{
	assert(request);
	const auto &req{*request};
	assert(written >= size(req.out.head));

	const size_t content_offset
	{
		written - size(req.out.head)
	};

	const size_t remain
	{
		size(req.out.head) + size(req.out.content) - written
	};

	const const_buffer window
	{
		data(req.out.content) + content_offset, remain
	};

	return window;
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
		string_view{data(buffer), size(buffer)}.find(terminator)
	};

	// No terminator found; account for what was received in this buffer
	// for the next call to make_head_buffer() preparing for the subsequent
	// invocation of this function with more data.
	if(pos == string_view::npos)
	{
		this->head_read += size(buffer);

		// Check that the user hasn't run out of head buffer space without
		// seeing a terminator. If so, we have to throw out of here and then
		// abort this user's request.
		if(unlikely(this->head_read >= size(req.in.head)))
			throw buffer_overrun
			{
				"Supplied buffer of %zu too small for HTTP head", size(req.in.head)
			};

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
	this->head_read += addl_head_bytes;
	const size_t &head_read{this->head_read};
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

	// Resize the user's head buffer tight to the head; this is how we convey
	// the size of the dome back to the user.
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
	this->status = http::status(head.status);

	// Proffer the HTTP head to the peer so it can learn from any data
	assert(link.peer);
	link.peer->handle_head_recv(link, *this, head);

	// Now we know how much content was received beyond the head
	const size_t &content_read
	{
		std::min(head.content_length, beyond_head_len)
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

	// We branch for a feature that allows dynamic allocation of the content
	// buffer if the user did not specify any buffer.
	const bool dynamic
	{
		!contiguous && empty(req.in.content)
	};

	if(dynamic)
	{
		assert(req.opt);
		const size_t alloc_size
		{
			std::min(head.content_length, req.opt->content_length_maxalloc)
		};

		req.in.dynamic = unique_buffer<mutable_buffer>{alloc_size};
		req.in.content = req.in.dynamic;
	}

	// Reduce the user's content buffer to the content-length. This is sort of
	// how we convey the content-length back to the user. The buffer size will
	// eventually reflect how much content was actually received; the user can
	// find the given content-length by parsing the header.
	req.in.content = mutable_buffer
	{
		data(req.in.content), std::min(head.content_length, size(req.in.content))
	};

	// If the supplied content buffer is too small this must indicate how much
	// content will have to be discarded later to not mess up the pipeline.
	if(head.content_length > size(req.in.content))
		content_over = head.content_length - size(req.in.content);

	// Any partial content was written to the head buffer by accident,
	// that may have to be copied over to the content buffer.
	if(!empty(partial_content) && !contiguous)
		copy(req.in.content, partial_content);

	// Invoke the read_content() routine which will increment this->content_read
	read_content(partial_content, done);
	assert(this->content_read == size(partial_content));

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
	assert(size(content) + content_over >= content_read);
	const size_t remaining
	{
		size(content) + content_over - content_read
	};

	// The amount of content read in this buffer only.
	const size_t addl_content_read
	{
		std::min(size(buffer), remaining)
	};

	content_read += addl_content_read;
	assert(size(buffer) - addl_content_read == 0);
	assert(content_read <= size(content) + content_over);

	// Invoke the user's optional progress callback; this function
	// should be marked noexcept for the time being.
	if(req.in.progress)
		req.in.progress(buffer, const_buffer{data(content), content_read});

	if(content_read == size(content) + content_over)
	{
		done = true;
		set_value(status);
	}

	return {};
}

ircd::mutable_buffer
ircd::server::tag::make_read_head_buffer()
const
{
	assert(request);
	const auto &req{*request};
	const auto &head{req.in.head};
	const auto &content{req.in.content};
	if(head_read >= size(head))
		return {};

	const size_t remaining
	{
		size(head) - head_read
	};

	assert(remaining <= size(head));
	const mutable_buffer buffer
	{
		data(head) + head_read, remaining
	};

	return buffer;
}

ircd::mutable_buffer
ircd::server::tag::make_read_content_buffer()
const
{
	assert(request);
	const auto &req{*request};
	const auto &content{req.in.content};

	// The amount of bytes we still have to read to for the response
	assert(size(content) >= content_read);
	const size_t remaining
	{
		size(content) - content_read
	};

	return
	{
		data(content) + content_read, remaining
	};
}

ircd::mutable_buffer
ircd::server::tag::make_read_discard_buffer()
const
{
	assert(request);
	assert(content_over > 0);
	assert(content_over <= content_read);
	assert(content_read >= size(request->in.content));
	const size_t remaining
	{
		content_over - content_read
	};

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

		set_exception(http::error{code, std::string{content}});
		return;
	}

	assert(p.valid());
	p.set_value(code);
}

template<class... args>
void
ircd::server::tag::set_exception(args&&... a)
{
	set_exception(std::make_exception_ptr(std::forward<args>(a)...));
}

void
ircd::server::tag::set_exception(std::exception_ptr eptr)
{
	if(abandoned())
		return;

	assert(p.valid());
	p.set_exception(std::move(eptr));
}

bool
ircd::server::tag::abandoned()
const
{
	return p.finished();
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
	return read_total() - read_completed();
}

size_t
ircd::server::tag::read_completed()
const
{
	return head_read + content_read;
}

size_t
ircd::server::tag::read_total()
const
{
	return request? size(request->in) : 0;
}

size_t
ircd::server::tag::write_remaining()
const
{
	return write_total() - write_completed();
}

size_t
ircd::server::tag::write_completed()
const
{
	return written;
}

size_t
ircd::server::tag::write_total()
const
{
	return request? size(request->out) : 0;
}
