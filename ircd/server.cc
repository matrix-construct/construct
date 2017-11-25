/*
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <ircd/server.h>
#include <ircd/asio.h>

//
// init
//

ircd::server::init::init()
{
}

ircd::server::init::~init()
noexcept
{
	ircd::server::nodes.clear();
}

//
// server
//

ircd::http::request::write_closure
ircd::write_closure(server &server)
{
	// returns a function that can be called to send an iovector of data to a server
	return [&server](const ilist<const_buffer> &iov)
	{
		//std::cout << "<<<<" << std::endl;
		//std::cout << iov << std::endl;
		//std::cout << "----" << std::endl;
		write(server, iov);
	};
}

ircd::parse::read_closure
ircd::read_closure(server &server)
{
	// Returns a function the parser can call when it wants more data
	return [&server](char *&start, char *const &stop)
	{
		char *const s(start);
		read(server, start, stop);
		//std::cout << ">>>>" << std::endl;
		//std::cout << string_view{s, start} << std::endl;
		//std::cout << "----" << std::endl;
    };
}

char *
ircd::read(server &server,
           char *&start,
           char *const &stop)
try
{
	auto &sock
	{
		*(*begin(server.n->links)).s
	};

	const std::array<mutable_buffer, 1> bufs
	{{
		{ start, stop }
	}};

	char *const base(start);
	start += sock.read_some(bufs);
	return base;
}
catch(const boost::system::system_error &e)
{
	using namespace boost::system::errc;

	log::error("read error: %s: %s",
	           string(server.n->remote),
	           string(e));

	if(ircd::runlevel == ircd::runlevel::QUIT)
		throw;

	switch(e.code().value())
	{
		case SSL_R_SHORT_READ:
		case boost::asio::error::eof:
		{
			auto &link{*begin(server.n->links)};
			link.disconnect();
			if(!link.connect(server.n->remote))
				throw;

			return read(server, start, stop);
		}

		case operation_canceled:
			throw http::error(http::REQUEST_TIMEOUT);

		default:
			throw;
	}
}

size_t
ircd::write(server &server,
            const ilist<const_buffer> &iov)
try
{
	auto &sock
	{
		*(*begin(server.n->links)).s
	};

	return sock.write(iov);
}
catch(const boost::system::system_error &e)
{
	using namespace boost::system::errc;

	log::error("write error: %s %s",
	           string(server.n->remote),
	           string(e));

	switch(e.code().value())
	{
		case SSL_R_SHORT_READ:
		case boost::asio::error::eof:
		case protocol_error:
		case broken_pipe:
		{
			auto &link{*begin(server.n->links)};
			link.disconnect();
			if(!link.connect(server.n->remote))
				throw;

			return write(server, iov);
		}

		case operation_canceled:
			throw http::error(http::REQUEST_TIMEOUT);

		default:
			throw;
	}
}

//
// server
//

ircd::server::server(net::remote remote)
:n{[&remote]
{
	const auto &ipp
	{
		static_cast<const net::ipport &>(remote)
	};

	const auto it(nodes.lower_bound(ipp));
	if(it == nodes.end() || it->first != ipp)
	{
		const auto ipp{static_cast<net::ipport>(remote)};
		const auto n{std::make_shared<node>(std::move(remote))};
		nodes.emplace_hint(it, ipp, n);
		return n;
	}

	return it->second;
}()}
,it
{
	n->tags, n->tags.emplace(n->tags.end(), this)
}
{
}

ircd::server::~server()
noexcept
{
}

ircd::server::operator
const ircd::net::remote &()
const
{
	static const ircd::net::remote null_remote {};
	if(unlikely(!n))
		return null_remote;

	return n->remote;
}

decltype(ircd::server::nodes)
ircd::server::nodes
{};

//
// node
//

ircd::server::node::node(net::remote remote)
:remote{std::move(remote)}
{
	add(1);
}

ircd::server::node::~node()
noexcept
{
}

void
ircd::server::node::add(const size_t &num)
{
	links.emplace_back(remote);
}

void
ircd::server::node::del(const size_t &num)
{
}

//
// link
//

ircd::server::link::link(const net::remote &remote)
:s{std::make_shared<net::socket>()}
,state{state::DEAD}
{
	connect(remote);
}

bool
ircd::server::link::disconnect()
{
	return net::disconnect(*s);
}

bool
ircd::server::link::connect(const net::remote &remote)
{
	if(connected())
		return false;

	s->connect(remote, 30000ms);
	return true;
}

bool
ircd::server::link::connected()
const noexcept
{
	return net::connected(*s);
}
