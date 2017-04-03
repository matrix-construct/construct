/* 
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
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

#include <ircd/listen.h>

namespace ircd {

const size_t DEFAULT_STACK_SIZE
{
	256_KiB  // can be optimized
};

struct listener::acceptor
{
	static log::log log;

	std::string name;
	size_t backlog;

	asio::ssl::context ssl;
	ip::tcp::endpoint ep;
	ip::tcp::acceptor a;
	ctx::context context;

	explicit operator std::string() const;

	bool accept();
	void main();

	acceptor(const json::doc &opts);
	~acceptor() noexcept;
};

} // namespace ircd

///////////////////////////////////////////////////////////////////////////////
//
// ircd::listener
//

ircd::listener::listener(const json::doc &opts)
:acceptor{std::make_unique<struct acceptor>(opts)}
{
}

ircd::listener::~listener()
noexcept
{
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd::listener::acceptor
//

ircd::log::log
ircd::listener::acceptor::log
{
	"listener"
};

ircd::listener::acceptor::acceptor(const json::doc &opts)
try
:name
{
	unquote(opts.get("name", "IRCd (ssl)"s))
}
,backlog
{
	opts.get<size_t>("backlog", asio::socket_base::max_connections)
}
,ssl
{
	asio::ssl::context::method::sslv23_server
}
,ep
{
	ip::address::from_string(unquote(opts.get("host", "127.0.0.1"s))),
	opts.get<uint16_t>("port", 6667)
}
,a
{
	*ircd::ios
}
,context
{
	"listener",
	opts.get<size_t>("stack_size", DEFAULT_STACK_SIZE),
	std::bind(&acceptor::main, this),
	context.POST // defer until ctor body binds the socket
}
{
	log.debug("%s attempting to open listening socket", std::string(*this));

	a.open(ep.protocol());
	a.set_option(ip::tcp::acceptor::reuse_address(true));
	a.bind(ep);

	if(opts.has("ssl.certificate.file.pem"))
	{
		const std::string filename
		{
			unquote(opts["ssl.certificate.file.pem"])
		};

		ssl.use_certificate_file(filename, asio::ssl::context::pem);
		log.info("%s using certificate file '%s'",
		          std::string(*this),
		          filename);
	}

	if(opts.has("ssl.private_key.file.pem"))
	{
		const std::string filename
		{
			unquote(opts["ssl.private_key.file.pem"])
		};

		ssl.use_private_key_file(filename, asio::ssl::context::pem);
		log.info("%s using private key file '%s'",
		          std::string(*this),
		          filename);
	}

	a.listen(backlog);

	// Allow main() to run and print its log message
	ctx::yield();
}
catch(const boost::system::system_error &e)
{
	throw error("listener: %s", e.what());
}

ircd::listener::acceptor::~acceptor()
noexcept
{
	a.cancel();
}

void
ircd::listener::acceptor::main()
try
{
	log.info("%s ready", std::string(*this));

	while(accept());

	log.info("%s closing", std::string(*this));
}
catch(const ircd::ctx::interrupted &e)
{
	log.warning("%s interrupted", std::string(*this));
}
catch(const std::exception &e)
{
	log.error("%s %s", std::string(*this), e);
}

bool
ircd::listener::acceptor::accept()
try
{
	auto sock(std::make_shared<ircd::socket>(ssl));
	a.async_accept(sock->ssl.lowest_layer(), yield(continuation()));
	sock->ssl.async_handshake(socket::handshake_type::server, yield(continuation()));
	add_client(std::move(sock));
	return true;
}
catch(const boost::system::system_error &e)
{
	switch(e.code().value())
	{
		using namespace boost::system::errc;

		case success:               return true;
		case operation_canceled:    return false;
		default:                    throw;
	}
}
catch(const std::exception &e)
{
	log.error("%s: in accept(): %s", std::string(*this), e);
	return true;
}

ircd::listener::acceptor::operator std::string()
const
{
	std::string ret(256, char());
	const auto length
	{
		fmt::snprintf(&ret.front(), ret.size(), "'%s' @ [%s]:%u",
		              name,
		              string(ep.address()),
		              ep.port())
	};

	ret.resize(length);
	return ret;
}
