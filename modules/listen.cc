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

#include <boost/asio.hpp>
#include <ircd/ctx/continuation.h>
#include <ircd/socket.h>

using namespace ircd;

const size_t STACK_SIZE
{
	256_KiB  // can be optimized
};

struct listener
{
	static struct log::log log;

	std::string name;
	size_t backlog;
	ip::address host;
	ip::tcp::endpoint ep;
	ctx::dock cond;
	ircd::context context;
	asio::ssl::context ssl;
	ip::tcp::acceptor acceptor;

	bool configured() const;

  private:
	bool accept();
	void main();

  public:
	listener(const std::string &name = {});
	listener(listener &&) = default;
};

struct log::log listener::log
{
	"listener", 'P'
};

listener::listener(const std::string &name)
try
:name
{
	name
}
,backlog
{
	boost::asio::socket_base::max_connections
}
,context
{
	"listener", STACK_SIZE, std::bind(&listener::main, this)
}
,ssl
{
	asio::ssl::context::method::sslv23_server
}
,acceptor
{
	*ios
}
{
	ssl.use_certificate_file("/home/jason/cdc.z.cert", asio::ssl::context::pem);
	ssl.use_private_key_file("/home/jason/cdc.z.key", asio::ssl::context::pem);
}
catch(const boost::system::system_error &e)
{
	throw error("listener: %s", e.what());
}

bool
listener::configured()
const
{
	return ep != ip::tcp::endpoint{};
}

void
listener::main()
try
{
	// The listener context starts after there is a valid configuration
	cond.wait([this]
	{
		return configured();
	});

	log.debug("Attempting bind() to [%s]:%u",
	                ep.address().to_string().c_str(),
	                ep.port());

	acceptor.open(ep.protocol());
	acceptor.set_option(ip::tcp::acceptor::reuse_address(true));
	acceptor.bind(ep);
	acceptor.listen(backlog);
	log.info("Listener bound to [%s]:%u",
	         ep.address().to_string().c_str(),
	         ep.port());

	while(accept());

	log.info("Listener closing @ [%s]:%u",
	          ep.address().to_string().c_str(),
	          ep.port());
}
catch(const ircd::ctx::interrupted &e)
{
	log.warning("Listener closing @ [%s]:%u: %s",
	            ep.address().to_string().c_str(),
	            ep.port(),
	            e.what());
}
catch(const std::exception &e)
{
	log.error("Listener closing @ [%s]:%u: %s",
	          ep.address().to_string().c_str(),
	          ep.port(),
	          e.what());
}

bool
listener::accept()
try
{
	auto sock(std::make_shared<ircd::socket>(ssl));
	acceptor.async_accept(sock->ssl.lowest_layer(), yield(continuation()));
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
	log.error("Listener @ [%s]:%u: accept(): %s",
	          ep.address().to_string().c_str(),
	          ep.port(),
	          e.what());

	return true;
}

std::map<std::string, listener> listeners;

extern "C" void gogo()
{
	const auto iit(listeners.emplace("foo"s, "foo"s));
	auto &foo(iit.first->second);
	foo.host = ip::address::from_string("127.0.0.1");
	foo.ep = ip::tcp::endpoint(foo.host, 6667);
	foo.cond.notify_one();
}

mapi::header IRCD_MODULE
{
	"P-Line - instructions for listening sockets", &gogo
};
