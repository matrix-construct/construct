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

#include <ircd/server.h>
#include <ircd/asio.h>

namespace ircd::server
{
	std::shared_ptr<node> create(const net::hostport &);
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
	nodes.clear();
}

void
ircd::server::init::interrupt()
{
	nodes.clear();
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
// request::tag
//

ircd::server::request::tag::tag(server::request &request)
:request{&request}
{
	static_cast<ctx::future<http::code> &>(request) = p;
}

ircd::server::request::tag::tag(tag &&o)
noexcept
:request{std::move(o.request)}
{
	assert(o.request->tag == &o);
	o.request = nullptr;
	request->tag = this;
}

struct ircd::server::request::tag &
ircd::server::request::tag::operator=(tag &&o)
noexcept
{
	request = std::move(o.request);

	assert(o.request->tag == &o);
	o.request = nullptr;
	request->tag = this;

	return *this;
}

ircd::server::request::tag::~tag()
noexcept
{
}

bool
ircd::server::request::tag::read_buffer(const const_buffer &buffer,
                                        const_buffer &overrun)
{
	assert(request);
	return !request?              true:
	       !request->head.status? read_head(buffer, overrun):
	                              read_content(buffer, overrun);
}

ircd::mutable_buffer
ircd::server::request::tag::make_read_buffer()
const
{
	assert(request);
	return !request?              mutable_buffer{}:
	       !request->head.status? make_head_buffer():
	                              make_content_buffer();
}

bool
ircd::server::request::tag::read_head(const const_buffer &buffer,
                                      const_buffer &overrun)
{
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
		head_read += size(buffer);
		return false;
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

	// The head bytes accounting can be updated and this will be the final
	// value of what is legitimate head in the req.in.head buffer.
	head_read += addl_head_bytes;

	// Setup the capstan and mark the end of the tape
	parse::buffer pb{mutable_buffer{data(req.in.head), head_read}};
	parse::capstan pc{pb};
	pc.read += head_read;

	// The HTTP head is parsed here and saved in the user's object but they
	// do not know about it yet and shouldn't be touching it.
	req.head = http::response::head{pc};
	assert(pb.completed() == head_read);

	// As stated, the buffer may contain data past the head, which includes
	// our content or the next response which doesn't even belong to us.
	const size_t overrun_length
	{
		size(buffer) - addl_head_bytes
	};

	// Calculate the amount of overrun which belongs to our content.
	const size_t &content_read
	{
		std::min(req.head.content_length, overrun_length)
	};

	// Where the partial content would be written to
	const const_buffer partial_content
	{
		data(req.in.head) + head_read, content_read
	};

	// Any partial content was written to the head buffer by accident,
	// that has to be copied over to the content buffer.
	this->content_read += copy(req.in.content, partial_content);
	assert(this->content_read == size(partial_content));

	// Anything remaining is not our response and must be given back
	assert(overrun_length >= content_read);
	overrun = const_buffer
	{
		data(req.in.head) + head_read + content_read, overrun_length - content_read
	};

	// When lucky, the content was recieved already (or there is no content) and
	// we can notify the user in one shot.
	if(this->content_read == req.head.content_length)
	{
		p.set_value(http::status(req.head.status));
		return true;
	}

	return false;
}

bool
ircd::server::request::tag::read_content(const const_buffer &buffer,
                                         const_buffer &overrun)
{
	auto &req{*request};

	// The amount of remaining content for the response sequence
	assert(req.head.content_length >= content_read);
	const size_t remaining
	{
		req.head.content_length - content_read
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
	overrun = const_buffer
	{
		data(req.in.content) + content_read, overrun_length
	};

	assert(content_read <= req.head.content_length);
	if(content_read == req.head.content_length)
	{
		p.set_value(http::status(req.head.status));
		return true;
	}
	else return false;
}

ircd::mutable_buffer
ircd::server::request::tag::make_head_buffer()
const
{
	const auto &req{*request};
	if(head_read >= size(req.in.head))
		return {};

	const size_t remaining
	{
		size(req.in.head) - head_read
	};

	assert(remaining <= size(req.in.head));
	const mutable_buffer buffer
	{
		data(req.in.head) + head_read, remaining
	};

	return buffer;
}

ircd::mutable_buffer
ircd::server::request::tag::make_content_buffer()
const
{
	auto &req{*request};

	// The amount of bytes we still have to read to for the response
	assert(req.head.content_length >= content_read);
	const size_t remaining
	{
		req.head.content_length - content_read
	};

	// The amount of bytes available in the user's buffer.
	assert(size(req.in.content) >= content_read);
	const size_t available
	{
		size(req.in.content) - content_read
	};

	// For now, this has to trip right here.
	assert(available >= remaining);
	const mutable_buffer buffer
	{
		data(req.in.content) + content_read, std::min(available, remaining)
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
	else
		return links.back();
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
ircd::server::node::link_del(const size_t &num)
{
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

	write(*socket, request.out.head);
	write(*socket, request.out.content);
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
	eptr = std::move(eptr);
	init = false;
	node->dock.notify_all();

	if(!eptr)
		wait_readable();
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
	eptr = std::move(eptr);
	fini = false;
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
ircd::server::link::handle_readable(const error_code &ec)
{
	using namespace boost::system::errc;
	using boost::system::system_category;

	switch(ec.value())
	{
		case success:
			handle_readable_success();
			wait_readable();
			break;

		default:
			throw boost::system::system_error{ec};
	}
}

void
ircd::server::link::handle_readable_success()
{
	if(queue.empty())
		throw std::out_of_range("queue empty");

	auto &tag
	{
		queue.front()
	};

	const mutable_buffer buffer
	{
		tag.make_read_buffer()
	};

	const size_t bytes
	{
		read_one(*socket, buffer)
	};

	const const_buffer received
	{
		data(buffer), bytes
	};

	const_buffer overrun;
	const bool done
	{
		tag.read_buffer(received, overrun)
	};

	if(!empty(overrun))
		std::cout << "got overrun: " << size(overrun) << std::endl;

	if(done)
		queue.pop_front();
}

void
ircd::server::link::handle_writable(const error_code &ec)
{

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
	return connected() && !init && !fini && !eptr;
}

bool
ircd::server::link::connected()
const noexcept
{
	return bool(socket) && net::connected(*socket);
}
