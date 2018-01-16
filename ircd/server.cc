// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include <ircd/server/server.h>
#include <ircd/asio.h>

namespace ircd::server
{
	// Coarse maximum number of connections to a server
	const size_t LINK_MAX_DEFAULT
	{
		2
	};

	// Coarse minimum number of connections to a server
	const size_t LINK_MIN_DEFAULT
	{
		1
	};

	// Maximum number of requests "in flight" in the pipe at at time
	const size_t TAG_COMMIT_MAX_DEFAULT
	{
		1
	};

	ctx::dock dock;

	template<class F> size_t accumulate_nodes(F&&);
	template<class F> size_t accumulate_links(F&&);
	template<class F> size_t accumulate_tags(F&&);

	std::shared_ptr<node> create(const net::hostport &);
	void interrupt_all();
	void close_all();
}

decltype(ircd::server::log)
ircd::server::log
{
	"server", 'S'
};

decltype(ircd::server::nodes)
ircd::server::nodes
{};

ircd::server::node &
ircd::server::get(const net::hostport &hostport)
{
	auto it(nodes.lower_bound(host(hostport)));
	if(it == nodes.end() || it->first != host(hostport))
	{
		auto node{create(hostport)};
		log.debug("node(%p) for %s created; adding...",
		          node.get(),
		          string(hostport));

		const string_view key{node->remote.hostname};
		it = nodes.emplace_hint(it, key, std::move(node));
	}

	return *it->second;
}

std::shared_ptr<ircd::server::node>
ircd::server::create(const net::hostport &hostport)
{
	auto node(std::make_shared<node>());
	node->remote.hostname = std::string{host(hostport)};
	node->resolve(hostport);
	return node;
}

ircd::server::node &
ircd::server::find(const net::hostport &hostport)
{
	return *nodes.at(host(hostport));
}

bool
ircd::server::exists(const net::hostport &hostport)
{
	return nodes.find(host(hostport)) != end(nodes);
}

size_t
ircd::server::node_count()
{
	return nodes.size();
}

size_t
ircd::server::link_count()
{
	return accumulate_nodes([]
	(const auto &node)
	{
		return node.link_count();
	});
}

size_t
ircd::server::tag_count()
{
	return accumulate_nodes([]
	(const auto &node)
	{
		return node.tag_count();
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
	return accumulate_nodes([&closure]
	(const auto &node)
	{
		return node.accumulate_links(std::forward<F>(closure));
	});
}

template<class F>
size_t
ircd::server::accumulate_nodes(F&& closure)
{
	return std::accumulate(begin(nodes), end(nodes), size_t(0), [&closure]
	(auto ret, const auto &pair)
	{
		const auto &node{*pair.second};
		return ret += closure(node);
	});
}

//
// init
//

ircd::server::init::init()
{
}

ircd::server::init::~init()
noexcept
{
	close_all();
	nodes.clear();
}

void
ircd::server::init::interrupt()
{
	interrupt_all();
}

void
ircd::server::close_all()
{
	log.debug("Closing all %zu nodes",
	          node_count());

	for(auto &node : nodes)
		node.second->close();

	log.debug("Waiting for %zu tags on %zu links on %zu nodes to close...",
	          tag_count(),
	          link_count(),
	          node_count());

	while(link_count())
		dock.wait();
}

void
ircd::server::interrupt_all()
{
	log.debug("Interrupting %zu tags on %zu links on %zu nodes",
	          tag_count(),
	          link_count(),
	          node_count());

	for(auto &node : nodes)
		node.second->interrupt();
}

///
// request
//

void
ircd::server::submit(const hostport &hostport,
                     request &request)
{
	assert(request.tag == nullptr);
	auto &node(server::get(hostport));
	node.submit(request);
}

//
// node
//

ircd::server::node::node()
{
}

ircd::server::node::~node()
noexcept
{
	assert(links.empty());
}

void
ircd::server::node::close()
{
	for(auto &link : links)
		link.close(net::close_opts_default);
}

void
ircd::server::node::interrupt()
{
	//TODO: not a close
	//TODO: interrupt = killing requests but still setting tag promises
	for(auto &link : links)
		link.close(net::close_opts_default);
}

void
ircd::server::node::submit(request &request)
{
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
			"No link to node %s available", remote.hostname
		};
	else
		request.tag->set_exception(std::make_exception_ptr(unavailable
		{
			"No link to node %s available", remote.hostname
		}));
}

void
ircd::server::node::cancel(request &request)
{
}

/// Dispatch algorithm here; finds the best link to place this request on,
/// or creates a new link entirely. There are a number of factors: foremost
/// if any special needs are indicated,
//
ircd::server::link *
ircd::server::node::link_get(const request &request)
{
	if(links.empty())
		return &link_add(1);

	// Indicates that we can't add anymore links for this node and the rest
	// of the algorithm should consider this.
	const bool links_maxed
	{
		links.size() >= link_max()
	};

	link *best{nullptr};
	for(auto &cand : links)
	{
		// Don't want a link that's shutting down or marked for exclusion
		if(cand.fini || cand.exclude)
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

	if(best->tag_uncommitted() < best->tag_commit_max())
		return best;

	best = &link_add();
	return best;
}

ircd::server::link &
ircd::server::node::link_add(const size_t &num)
{
	links.emplace_back(*this);
	auto &link{links.back()};

	if(remote.resolved())
		link.open(remote);

	return link;
}

void
ircd::server::node::handle_open(link &link,
                                std::exception_ptr eptr)
try
{
	if(eptr)
		std::rethrow_exception(eptr);
}
catch(const std::exception &e)
{
	log.error("node(%p) link(%p) [%s]: open: %s",
	          this,
	          &link,
	          string(remote),
	          e.what());
}

void
ircd::server::node::handle_close(link &link,
                                 std::exception_ptr eptr)
try
{
	const unwind remove{[this, &link]
	{
		this->del(link);
	}};

	if(eptr)
		std::rethrow_exception(eptr);
}
catch(const std::exception &e)
{
	log.error("node(%p) link(%p) [%s]: close: %s",
	          this,
	          &link,
	          string(remote),
	          e.what());
}

void
ircd::server::node::handle_error(link &link,
                                 const std::exception &e)
{
	log.error("node(%p) link(%p): %s",
	          this,
	          &link,
	          e.what());

	cancel_committed(link, e);
	link.close(net::dc::RST);
}

void
ircd::server::node::handle_error(link &link,
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
			log.debug("node(%p) link(%p) [%s]: %s",
			          this,
			          &link,
			          string(remote),
			          e.what());

			link.close(net::close_opts_default);
			return;

		default:
			break;
	}

	log.error("node(%p) link(%p) [%s]: error: %s",
	          this,
	          &link,
	          string(remote),
	          e.what());

	link.close(net::dc::RST);
}

/// This is where we're notified a tag has been completed either to start the
/// next request when the link has too many requests in flight or perhaps to
/// reschedule the queues in various links to diffuse the pending requests.
/// This can't throw because the link still has to remove this tag from its
/// queue.
void
ircd::server::node::handle_tag_done(link &link,
                                    tag &tag)
noexcept try
{
	if(link.tag_committed() >= link.tag_commit_max())
		link.wait_writable();
}
catch(const std::exception &e)
{
	log.critical("node(%p) link(%p) tag(%p) done; error: %s",
	             this,
	             &link,
	             &tag,
	             e.what());
}

/// This is where we're notified a link has processed its queue and has no
/// more work. We can choose whether to close the link or keep it open and
/// reinstate the read poll; reschedule other work to this link, etc.
void
ircd::server::node::handle_link_done(link &link)
{
	assert(link.tag_count() == 0);

	if(link_ready() > link_min())
	{
		link.close();
		return;
	}

	link.wait_readable();
}

void
ircd::server::node::disperse(link &link)
{
	// the easy part
	disperse_uncommitted(link);
	cancel_committed(link, canceled
	{
		"Request was partially completed when fatal error occurred"
	});

	assert(link.queue.empty());
}

void
ircd::server::node::cancel_committed(link &link,
                                     const std::exception &e)
{
	auto &queue(link.queue);

	// Find the first tag in the queue which hasn't revealed any data to
	// the remote; this is uncommitted.
	const auto it
	{
		std::find_if(begin(queue), end(queue), [](const auto &tag)
		{
			return !tag.write_completed();
		})
	};

	std::for_each(begin(queue), it, [this, &e](auto &tag)
	{
		if(!tag.request)
			return;

		assert(tag.write_completed());
		auto &request{*tag.request};
		disassociate(request, tag);
		tag.set_exception(std::make_exception_ptr(e));
	});

	log.debug("node(%p) link(%p) errored %zu of %zu of its tags",
	          this,
	          &link,
	          std::distance(begin(queue), it),
	          queue.size());

	const auto remaining
	{
		std::distance(begin(queue), queue.erase(begin(queue), it))
	};

	queue.resize(remaining);
}

void
ircd::server::node::disperse_uncommitted(link &link)
{
	auto &queue(link.queue);

	// Find the first tag in the queue which hasn't revealed any data to
	// the remote; this is uncommitted.
	const auto it
	{
		std::find_if(begin(queue), end(queue), [](const auto &tag)
		{
			return !tag.write_completed();
		})
	};

	std::for_each(it, end(queue), [this](auto &tag)
	{
		if(!tag.request)
			return;

		assert(!tag.write_completed());
		auto &request{*tag.request};
		disassociate(request, tag);

		auto *const link
		{
			this->link_get(request)
		};

		if(likely(link))
		{
			link->submit(request);
			return;
		}

		tag.set_exception(std::make_exception_ptr(unavailable
		{
			"No link to node %s available", remote.hostname
		}));
	});

	log.debug("node(%p) link(%p) dispersed %zu of %zu of its tags",
	          this,
	          &link,
	          std::distance(it, end(queue)),
	          queue.size());

	const auto remaining
	{
		std::distance(begin(queue), queue.erase(it, end(queue)))
	};

	queue.resize(remaining);
}

/// This *cannot* be called unless a link's socket is closed and its queue
/// is empty. It is usually only called by a disconnect handler because
/// the proper way to remove a link is asynchronously through link.close();
void
ircd::server::node::del(link &link)
{
	assert(!link.tag_count());
	assert(!link.connected());
	const auto it(std::find_if(begin(links), end(links), [&link]
	(const auto &link_)
	{
		return &link_ == &link;
	}));

	assert(it != end(links));
	log.debug("node(%p) removing link(%p) %zu of %zu to %s",
	          this,
	          &link,
	          std::distance(it, end(links)),
	          links.size(),
	          string(remote));

	links.erase(it);

	// Right now this is what the server:: ~init sequence needs
	// to wait for all links to close on IRCd shutdown.
	server::dock.notify_all();
}

void
ircd::server::node::resolve(const hostport &hostport)
{
	auto handler
	{
		std::bind(&node::handle_resolve, this, weak_from(*this), ph::_1, ph::_2)
	};

	net::resolve(hostport, std::move(handler));
}

void
ircd::server::node::handle_resolve(std::weak_ptr<node> wp,
                                   std::exception_ptr eptr,
                                   const ipport &ipport)
try
{
	const life_guard<node> lg(wp);

	if(eptr)
		std::rethrow_exception(std::move(eptr));

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
	if(wp.expired())
		return;

	for(auto &link : links)
		for(auto &tag : link.queue)
			tag.set_exception(std::make_exception_ptr(e));

	nodes.erase(remote.hostname);
}

size_t
ircd::server::node::read_remaining()
const
{
	return accumulate_links([](const auto &link)
	{
		return link.read_remaining();
	});
}

size_t
ircd::server::node::read_completed()
const
{
	return accumulate_links([](const auto &link)
	{
		return link.read_completed();
	});
}

size_t
ircd::server::node::read_total()
const
{
	return accumulate_links([](const auto &link)
	{
		return link.read_total();
	});
}

size_t
ircd::server::node::write_remaining()
const
{
	return accumulate_links([](const auto &link)
	{
		return link.write_remaining();
	});
}

size_t
ircd::server::node::write_completed()
const
{
	return accumulate_links([](const auto &link)
	{
		return link.write_completed();
	});
}

size_t
ircd::server::node::write_total()
const
{
	return accumulate_links([](const auto &link)
	{
		return link.write_total();
	});
}

size_t
ircd::server::node::tag_uncommitted()
const
{
	return accumulate_links([](const auto &link)
	{
		return link.tag_uncommitted();
	});
}

size_t
ircd::server::node::tag_committed()
const
{
	return accumulate_links([](const auto &link)
	{
		return link.tag_committed();
	});
}

size_t
ircd::server::node::tag_count()
const
{
	return accumulate_links([](const auto &link)
	{
		return link.tag_count();
	});
}

size_t
ircd::server::node::link_ready()
const
{
	return accumulate_links([](const auto &link)
	{
		return link.ready();
	});
}

size_t
ircd::server::node::link_busy()
const
{
	return accumulate_links([](const auto &link)
	{
		return link.busy();
	});
}

size_t
ircd::server::node::link_count()
const
{
	return links.size();
}

size_t
ircd::server::node::link_min()
const
{
	//TODO: conf
	return LINK_MIN_DEFAULT;
}

size_t
ircd::server::node::link_max()
const
{
	//TODO: conf
	return LINK_MAX_DEFAULT;
}

template<class F>
size_t
ircd::server::node::accumulate_tags(F&& closure)
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
ircd::server::node::accumulate_links(F&& closure)
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

ircd::server::link::link(server::node &node)
:node{shared_from(node)}
{
}

ircd::server::link::~link()
noexcept
{
	assert(!busy());
	assert(!connected());
}

void
ircd::server::link::submit(request &request)
{
	const auto it
	{
		queue.emplace(end(queue), request)
	};

	if(ready())
		wait_writable();
}

/// Cancel a request queued in the link. This is an expensive operation, use
/// with care.
ircd::server::tag
ircd::server::link::cancel(request &request)
{
	const auto it
	{
		std::find_if(begin(queue), end(queue), [&request]
		(const auto &tag)
		{
			return &tag == request.tag;
		})
	};

	if(it == end(queue))
		throw std::out_of_range
		{
			"Request has no tag queued in this link"
		};

	tag ret{std::move(*it)};
	queue.erase(it);
	return ret;
}

bool
ircd::server::link::open(const net::open_opts &open_opts)
{
	if(init)
		return false;

	auto handler
	{
		std::bind(&link::handle_open, this, ph::_1)
	};

	init = true;
	fini = false;
	socket = net::open(open_opts, std::move(handler));
	return true;
}

void
ircd::server::link::handle_open(std::exception_ptr eptr)
{
	init = false;

	if(!eptr)
	{
		wait_readable();
		wait_writable();
	}

	if(node)
		node->handle_open(*this, std::move(eptr));
}

bool
ircd::server::link::close(const net::close_opts &close_opts)
{
	if(!socket)
		return false;

	if(fini)
		return false;

	auto handler
	{
		std::bind(&link::handle_close, this, ph::_1)
	};

	init = false;
	fini = true;

	// Tell the node to ditch everything in the queue; fini has been set so
	// the tags won't get assigned back to this link.
	if(tag_count() && node)
		node->disperse(*this);

	net::close(*socket, close_opts, std::move(handler));
	return true;
}

void
ircd::server::link::handle_close(std::exception_ptr eptr)
{
	fini = false;

	if(node)
		node->handle_close(*this, std::move(eptr));
}

void
ircd::server::link::wait_writable()
{
	auto handler
	{
		std::bind(&link::handle_writable, this, ph::_1)
	};

	assert(ready());
	net::wait(*socket, net::ready::WRITE, std::move(handler));
}

void
ircd::server::link::handle_writable(const error_code &ec)
{
	using namespace boost::system::errc;
	using boost::system::system_category;

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

void
ircd::server::link::handle_writable_success()
{
	for(auto &tag : queue)
	{
		if(!process_write(tag))
		{
			wait_writable();
			break;
		}

		// Limits the amount of requests in the pipe.
		if(tag_committed() >= tag_commit_max())
			break;
	}
}

bool
ircd::server::link::process_write(tag &tag)
{
	if(tag.write_remaining() >= tag.write_total())
	{
		log.debug("link(%p) starting on tag %zu of %zu: wt:%zu",
		          this,
		          tag_committed(),
		          tag_count(),
		          tag.write_total());
	}

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
	auto handler
	{
		std::bind(&link::handle_readable, this, ph::_1)
	};

	assert(ready());
	net::wait(*socket, net::ready::READ, std::move(handler));
}

void
ircd::server::link::handle_readable(const error_code &ec)
try
{
	using namespace boost::system::errc;
	using boost::system::system_category;

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
	if(node)
		node->handle_error(*this, e);
}
catch(const std::exception &e)
{
	if(node)
	{
		node->handle_error(*this, e);
		return;
	}

	log.critical("link::handle_readable(): %s", e.what());
	assert(0);
	throw;
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

	assert(node);
	node->handle_link_done(*this);
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

	bool done{false}; do
	{
		overrun = process_read_next(overrun, tag, done);
	}
	while(!done);

	assert(node);
	node->handle_tag_done(*this, queue.front());
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
		tag.read_buffer(view, done)
	};

	assert(done || empty(overrun));
	return overrun;
}
catch(const buffer_overrun &e)
{
	tag.p.set_exception(std::make_exception_ptr(e));
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
		return tag.write_completed() > 0;
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
	return connected() && !init && !fini;
}

bool
ircd::server::link::connected()
const noexcept
{
	return bool(socket) && net::connected(*socket);
}

size_t
ircd::server::link::tag_commit_max()
const
{
	//TODO: config
	return TAG_COMMIT_MAX_DEFAULT;
}

size_t
ircd::server::link::tag_max()
const
{
	//TODO: config
	return -1;
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
ircd::const_buffer
ircd::server::tag::read_buffer(const const_buffer &buffer,
                               bool &done)
{
	return
		!request?
			const_buffer{}:

		head_read < size(request->in.head)?
			read_head(buffer, done):

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
	return
		!request?
			mutable_buffer{}:

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
		assert(data(buffer) == data(req.out.head));
		assert(written <= size(req.out.head));
	}
	else if(written <= size(req.out.head) + size(req.out.content))
	{
		assert(data(buffer) == data(req.out.content));
		assert(written <= write_total());
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
                             bool &done)
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

	// Now we know how much content was received beyond the head
	const size_t &content_read
	{
		std::min(head.content_length, beyond_head_len)
	};

	const const_buffer partial_content
	{
		data(req.in.head) + head_read, content_read
	};

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
	// that has to be copied over to the content buffer.
	this->content_read += copy(req.in.content, partial_content);

	// Anything remaining is not our response and must be given back
	assert(beyond_head_len >= content_read);
	const const_buffer overrun
	{
		data(beyond_head) + size(partial_content), beyond_head_len - content_read
	};

	assert(this->content_read + content_over <= head.content_length);
	if(this->content_read + content_over == head.content_length)
	{
		done = true;
		p.set_value(status);
	}

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
	if(content_read == size(content) + content_over)
	{
		done = true;
		p.set_value(status);
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

void
ircd::server::tag::set_exception(const error_code &ec)
{
	boost::system::system_error error
	{
		ec
	};

	auto eptr
	{
		std::make_exception_ptr(std::move(error))
	};

	set_exception(std::move(eptr));
}

void
ircd::server::tag::set_exception(std::exception_ptr eptr)
{
	p.set_exception(std::move(eptr));
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
