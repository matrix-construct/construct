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
	for(auto &node : nodes)
		node.second->close();
}

void
ircd::server::interrupt_all()
{
	for(auto &node : nodes)
		node.second->interrupt();
}

///
// request
//

ircd::server::request::request(const net::hostport &hostport,
                               server::out out,
                               server::in in)
:tag{nullptr}
,out{std::move(out)}
,in{std::move(in)}
{
	auto &node(server::get(hostport));
	node.submit(*this);
}

ircd::server::request::request(request &&o)
noexcept
:ctx::future<http::code>{std::move(o)}
,tag{std::move(o.tag)}
,out{std::move(o.out)}
,in{std::move(o.in)}
{
}

ircd::server::request &
ircd::server::request::operator=(request &&o)
noexcept
{
	ctx::future<http::code>::operator=(std::move(o));
	tag = std::move(o.tag);
	out = std::move(o.out);
	in = std::move(o.in);

	assert(tag->request == &o);
	tag->request = this;

	return *this;
}

ircd::server::request::~request()
noexcept
{
}

//
// tag
//

ircd::server::tag::tag(server::request &request)
:request{&request}
{
	static_cast<ctx::future<http::code> &>(request) = p;
}

ircd::server::tag::tag(tag &&o)
noexcept
:request{std::move(o.request)}
{
	assert(o.request->tag == &o);
	o.request = nullptr;
	request->tag = this;
}

struct ircd::server::tag &
ircd::server::tag::operator=(tag &&o)
noexcept
{
	request = std::move(o.request);

	assert(o.request->tag == &o);
	o.request = nullptr;
	request->tag = this;

	return *this;
}

ircd::server::tag::~tag()
noexcept
{
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
	return !request?                 const_buffer{}:
	       !request->in.head.status? read_head(buffer, done):
	                                 read_content(buffer, done);
}

/// An idempotent operation that provides the location of where the socket
/// should place the next received data. The tag figures this out based on
/// whether it receiving HTTP head data or whether it is in content mode.
///
/// In head mode, portions of the user's buffer are returned starting with
/// the entire buffer. After a call read_buffer() supplying more data from
/// the socket, the subsequent invocation of make_read_buffer() will return
/// smaller windows for the remaining free space. Head mode is completed when
/// an \r\n\r\n terminator is received.
///
/// In content mode, portions of the user's buffer are returned starting with
/// one that is at most content_length. After a call to read_buffer()
/// supplying more data, the remaining space of the remaining content_length
/// are returned.
///
ircd::mutable_buffer
ircd::server::tag::make_read_buffer()
const
{
	assert(request);
	return !request?                 mutable_buffer{}:
	       !request->in.head.status? make_read_head_buffer():
	                                 make_read_content_buffer();
}

void
ircd::server::tag::wrote_buffer(const const_buffer &buffer)
{
	assert(request);
	const auto &req{*request};
	if(head_written < size(req.out.head))
	{
		head_written += size(buffer);
		assert(data(buffer) == data(req.out.head));
		assert(head_written <= size(req.out.head));
	}
	else if(content_written < size(req.out.content))
	{
		content_written += size(buffer);
		assert(data(buffer) == data(req.out.content));
		assert(content_written <= size(req.out.content));
	}
}

ircd::const_buffer
ircd::server::tag::make_write_buffer()
const
{
	assert(request);
	const auto &req{*request};
	if(head_written < size(req.out.head))
	{
		const size_t remain{size(req.out.head) - head_written};
		const const_buffer window{data(req.out.head) + head_written, remain};
		return window;
	}

	if(content_written < size(req.out.content))
	{
		const size_t remain{size(req.out.content) - content_written};
		const const_buffer window{data(req.out.content) + content_written, remain};
		return window;
	}

	return {};
}

ircd::const_buffer
ircd::server::tag::read_head(const const_buffer &buffer,
                             bool &done)
{
	assert(request);
	auto &req{*request};
	auto &head{req.in.head};
	const auto &head_buffer{req.in.head_buffer};
	const auto &content_buffer{req.in.content_buffer};

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
		head_read += size(buffer);
		return {};
	}

	// The given buffer may go past the end of the head and we may already
	// have part of the head from a previous invocation. This indicates
	// how much dome was just received from this buffer only, including the
	// terminator which is considered part of the head.
	const size_t addl_head_bytes
	{
		pos + size(terminator)
	};

	assert(addl_head_bytes <= size(buffer));
	head_read += addl_head_bytes;

	// Setup the capstan and mark the end of the tape
	parse::buffer pb{mutable_buffer{data(head_buffer), head_read}};
	parse::capstan pc{pb};
	pc.read += head_read;

	head = http::response::head{pc};
	assert(pb.completed() == head_read);

	const size_t overrun_length
	{
		size(buffer) - addl_head_bytes
	};

	const size_t &content_read
	{
		std::min(head.content_length, overrun_length)
	};

	const const_buffer partial_content
	{
		data(head_buffer) + head_read, content_read
	};

	// Any partial content was written to the head buffer by accident,
	// that has to be copied over to the content buffer.
	this->content_read += copy(content_buffer, partial_content);
	assert(this->content_read == size(partial_content));

	// Anything remaining is not our response and must be given back
	assert(overrun_length >= content_read);
	const const_buffer overrun
	{
		data(head_buffer) + head_read + content_read, overrun_length - content_read
	};

	if(this->content_read == head.content_length)
	{
		done = true;
		p.set_value(http::status(head.status));
	}

	return overrun;
}

ircd::const_buffer
ircd::server::tag::read_content(const const_buffer &buffer,
                                bool &done)
{
	assert(request);
	auto &req{*request};
	const auto &head{req.in.head};
	const auto &content_buffer{req.in.content_buffer};

	// The amount of remaining content for the response sequence
	assert(head.content_length >= content_read);
	const size_t remaining
	{
		head.content_length - content_read
	};

	// The amount of content read in this buffer only.
	const size_t addl_content_read
	{
		std::min(size(buffer), remaining)
	};

	const size_t overrun_length
	{
		size(buffer) - addl_content_read
	};

	content_read += addl_content_read;

	// Report anything that doesn't belong to us.
	const const_buffer overrun
	{
		data(content_buffer) + content_read, overrun_length
	};

	assert(content_read <= head.content_length);
	if(content_read == head.content_length)
	{
		done = true;
		p.set_value(http::status(head.status));
	}

	return overrun;
}

ircd::mutable_buffer
ircd::server::tag::make_read_head_buffer()
const
{
	assert(request);
	const auto &req{*request};
	const auto &head_buffer{req.in.head_buffer};
	const auto &content_buffer{req.in.content_buffer};

	if(head_read >= size(head_buffer))
		return {};

	const size_t remaining
	{
		size(head_buffer) - head_read
	};

	assert(remaining <= size(head_buffer));
	const mutable_buffer buffer
	{
		data(head_buffer) + head_read, remaining
	};

	return buffer;
}

ircd::mutable_buffer
ircd::server::tag::make_read_content_buffer()
const
{
	assert(request);
	const auto &req{*request};
	const auto &head{req.in.head};
	const auto &content_buffer{req.in.content_buffer};

	// The amount of bytes we still have to read to for the response
	assert(head.content_length >= content_read);
	const size_t remaining
	{
		head.content_length - content_read
	};

	// The amount of bytes available in the user's buffer.
	assert(size(content_buffer) >= content_read);
	const size_t available
	{
		size(content_buffer) - content_read
	};

	// For now, this has to trip right here.
	assert(available >= remaining);
	const mutable_buffer buffer
	{
		data(content_buffer) + content_read, std::min(available, remaining)
	};

	return buffer;
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

	while(!links.empty())
		dock.wait();
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
	link &ret(link_get());
	ret.submit(request);
}

void
ircd::server::node::cancel(request &request)
{
}

ircd::server::link &
ircd::server::node::link_get()
{
	while(!remote.resolved())
		dock.wait();

	if(links.empty())
	{
		auto &ret(link_add(1));
		while(!ret.ready())
			dock.wait();

		return ret;
	}
	else return links.back();
}

ircd::server::link &
ircd::server::node::link_add(const size_t &num)
{
	links.emplace_back(*this);
	auto &link{links.back()};
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

	dock.notify_all();
}
catch(const std::exception &e)
{
	log::error("node(%p) link(%p) [%s]: open: %s",
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
	log::error("node(%p) link(%p) [%s]: close: %s",
	           this,
	           &link,
	           string(remote),
	           e.what());
}

void
ircd::server::node::handle_error(link &link,
                                 const boost::system::system_error &e)
{
	log::error("node(%p) link(%p) [%s]: error: %s",
	           this,
	           &link,
	           string(remote),
	           e.what());

	if(!link.busy())
	{
		link.close(net::close_opts_default);
		return;
	}
}

void
ircd::server::node::del(link &link)
{
	log::debug("node(%p) [%s]: removing link %p",
	           this,
	           string(remote),
	           &link);

	const auto it(std::find_if(begin(links), end(links), [&link]
	(const auto &link_)
	{
		return &link_ == &link;
	}));

	assert(it != end(links));
	links.erase(it);
	dock.notify_all();
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

	this->eptr = std::move(eptr);
	static_cast<net::ipport &>(this->remote) = ipport;
	dock.notify_all();
}
catch(const std::bad_weak_ptr &)
{
	return;
}

size_t
ircd::server::node::num_links_ready()
const
{
	return std::accumulate(begin(links), end(links), size_t(0), []
	(auto ret, const auto &link)
	{
		return ret += link.ready();
	});
}

size_t
ircd::server::node::num_links_busy()
const
{
	return std::accumulate(begin(links), end(links), size_t(0), []
	(auto ret, const auto &link)
	{
		return ret += link.busy();
	});
}

size_t
ircd::server::node::num_links()
const
{
	return links.size();
}

size_t
ircd::server::node::num_tags()
const
{
	return std::accumulate(begin(links), end(links), size_t(0), []
	(auto ret, const auto &link)
	{
		return ret += link.queue.size();
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

	wait_writable();
}

void
ircd::server::link::cancel(request &request)
{

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
		wait_readable();

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

	switch(ec.value())
	{
		case success:
			handle_writable_success();
			break;

		case operation_canceled:
			return;

		default:
			throw boost::system::system_error{ec};
	}
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
	}
}

bool
ircd::server::link::process_write(tag &tag)
{
	while(1)
	{
		const const_buffer buffer
		{
			tag.make_write_buffer()
		};

		if(empty(buffer))
			return true;

		const const_buffer written
		{
			process_write_next(buffer)
		};

		tag.wrote_buffer(written);
		if(size(written) < size(buffer))
			return false;
	}
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

	switch(ec.value())
	{
		case success:
			handle_readable_success();
			wait_readable();
			break;

		case operation_canceled:
			return;

		default:
			throw boost::system::system_error{ec};
	}
}
catch(const boost::system::system_error &e)
{
	if(node)
		node->handle_error(*this, e);
}
catch(const std::exception &e)
{
	log::critical("link::handle_readable(): %s", e.what());
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
			break;
	}
	while(!queue.empty());
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

	queue.pop_front();
	return true;
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
	log::warning("Link discarded %zu of %zu unexpected bytes",
	             discard,
	             discarded);
	assert(0);

	// for non-assert builds just in case; so this doesn't get loopy with
	// discarding zero with an empty queue...
	if(unlikely(!discard || !discarded))
		throw assertive("Queue is empty and nothing to discard.");
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
