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

#include <ircd/lex_cast.h>
#include <ircd/ctx/continuation.h>
#include <ircd/socket.h>

namespace ircd {

ip::tcp::resolver *resolver;

} // namespace ircd

boost::asio::ssl::context
ircd::sslv23_client
{
	boost::asio::ssl::context::method::sslv23_client
};

ircd::socket::init::init()
{
	assert(ircd::ios);
	resolver = new ip::tcp::resolver(*ios);
}

ircd::socket::init::~init()
{
	delete resolver;
	resolver = nullptr;
}

size_t
ircd::write(socket &socket,
            const string_view &s)
{
	return write(socket, s.data(), s.size());
}

size_t
ircd::write(socket &socket,
            const char *const &buf,
            const size_t &max)
{
	const std::array<const_buffer, 1> cbufs
	{{
		{ buf, max }
	}};

	return socket.write(cbufs);
}

ircd::string_view
ircd::readline(socket &socket,
               char *&start,
               char *const &stop)
{
	size_t pos;
	string_view ret;
	char *const base(start); do
	{
		const std::array<mutable_buffer, 1> buf
		{{
			{ start, stop }
		}};

		start += socket.read_some(buf);
		ret = {base, start};
		pos = ret.find("\r\n");
	}
	while(pos != std::string_view::npos);

	return { begin(ret), std::next(begin(ret), pos + 2) };
}

char *
ircd::read(socket &socket,
           char *&start,
           char *const &stop)
{
	const std::array<mutable_buffer, 1> buf
	{{
		{ start, stop }
	}};

	char *const base(start);
	start += socket.read_some(buf);
	return base;
}

ircd::socket::socket(const std::string &host,
                     const uint16_t &port,
                     asio::ssl::context &ssl,
                     boost::asio::io_service *const &ios)
:socket
{
	[&host, &port]() -> ip::tcp::endpoint
	{
		assert(resolver);
		const ip::tcp::resolver::query query(host, lex_cast(port));
		auto epit(resolver->async_resolve(query, yield(continuation())));
		static const ip::tcp::resolver::iterator end;
		if(epit == end)
			throw nxdomain("host '%s' not found", host.data());

		return *epit;
	}(),
	ssl,
	ios
}
{
}

ircd::socket::socket(const ip::tcp::endpoint &remote,
                     asio::ssl::context &ssl,
                     boost::asio::io_service *const &ios)
:socket{ssl, ios}
{
	connect(remote);
}

ircd::socket::socket(asio::ssl::context &ssl,
                     boost::asio::io_service *const &ios)
:ssl{*ios, ssl}
,sd{this->ssl.next_layer()}
,timer{*ios}
,timedout{false}
{
}

ircd::socket::~socket()
noexcept
{
}

void
ircd::socket::connect(const ip::tcp::endpoint &ep)
{
	sd.async_connect(ep, yield(continuation()));
	ssl.async_handshake(socket::handshake_type::client, yield(continuation()));
}

void
ircd::socket::disconnect(const dc &type)
{
	if(timer.expires_from_now() > 0ms)
		timer.cancel();

	if(sd.is_open()) switch(type)
	{
		default:
		case dc::RST:       sd.close();                                      break;
		case dc::FIN:       sd.shutdown(ip::tcp::socket::shutdown_both);     break;
		case dc::FIN_SEND:  sd.shutdown(ip::tcp::socket::shutdown_send);     break;
		case dc::FIN_RECV:  sd.shutdown(ip::tcp::socket::shutdown_receive);  break;
	}
}

void
ircd::socket::cancel()
{
	timer.cancel();
	sd.cancel();
}

void
ircd::socket::operator()(handler h)
{
	operator()(milliseconds(-1), std::move(h));
}

void
ircd::socket::operator()(const milliseconds &timeout,
                         handler h)
{
	static char buffer[1];
	static const mutable_buffers buffers{{buffer, sizeof(buffer)}};
	static const auto flags(ip::tcp::socket::message_peek);

	set_timeout(timeout);
	sd.async_receive(buffers, flags, [this, handler(h), wp(weak_from(*this))]
	(const error_code &ec, const size_t &bytes)
	{
		assert(bytes <= sizeof(buffer));

		if(unlikely(wp.expired()))
		{
			log::warning("socket(%p): belated callback to handler...", (const void *)this);
			return;
		}

		if(!handle_ready(ec))
			return;

		handler(ec);
	});
}

bool
ircd::socket::connected()
const noexcept try
{
	return sd.is_open();
}
catch(const boost::system::system_error &e)
{
	return false;
}

bool
ircd::socket::handle_ready(const error_code &ec)
noexcept
{
 	using namespace boost::system::errc;

	if(ec)
		log::debug("socket(%p): %s", (const void *)this, ec.message().c_str());

	if(!timedout)
		timer.cancel();

	switch(ec.value())
	{
		case success:
			return true;

		case boost::asio::error::eof:
			return true;

		case operation_canceled:
			return timedout;

		case bad_file_descriptor:
			return false;

		default:
			return true;
	}
}

void
ircd::socket::handle_timeout(const std::weak_ptr<socket> wp,
                             const error_code &ec)
noexcept
{
 	using namespace boost::system::errc;

	if(!wp.expired()) switch(ec.value())
	{
		case success:
			timedout = true;
			cancel();
			break;

		case operation_canceled:
			timedout = false;
			break;

		default:
			log::error("socket::handle_timeout(): unexpected: %s\n", ec.message().c_str());
			break;
	}
}
