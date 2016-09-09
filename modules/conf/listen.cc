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
#include <ircd/lex_cast.h>
#include <ircd/ctx_ctx.h>

namespace ip = boost::asio::ip;
using namespace ircd;

struct clicli
{
	ip::tcp::socket socket;

	clicli(): socket{*ircd::ios} {}
};

const size_t STACK_SIZE
{
	256_KiB  // can be optimized
};

struct listener
{
	std::string name;
	size_t backlog;
	ip::address host;
	ip::tcp::endpoint ep;
	ctx::dock cond;
	ircd::context context;
	ip::tcp::acceptor acceptor;

	bool configured() const;

  private:
	bool accept();
	void main();

  public:
	listener(const std::string &name = {});
	listener(listener &&) = default;
};

listener::listener(const std::string &name)
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
	STACK_SIZE, std::bind(&listener::main, this)
}
,acceptor
{
	*ios
}
{
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

	conf::log.debug("Attempting bind() to [%s]:%u",
	                ep.address().to_string().c_str(),
	                ep.port());

	acceptor.open(ep.protocol());
	acceptor.set_option(ip::tcp::acceptor::reuse_address(true));
	acceptor.bind(ep);
	acceptor.listen(backlog);

	conf::log.info("Listener bound to [%s]:%u",
	               ep.address().to_string().c_str(),
	               ep.port());

	while(accept());

	conf::log.info("Listener closing @ [%s]:%u",
	               ep.address().to_string().c_str(),
	               ep.port());
}
catch(const std::exception &e)
{
	conf::log.error("Listener closing @ [%s]:%u: %s",
	                ep.address().to_string().c_str(),
	                ep.port(),
	                e.what());
}

bool
listener::accept()
try
{
	auto clit(std::make_unique<clicli>());
	acceptor.async_accept(clit->socket, yield(continuation()));
	printf("Got client!\n");
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
	conf::log.error("Listener @ [%s]:%u: accept(): %s",
	                ep.address().to_string().c_str(),
	                ep.port(),
	                e.what());
	return true;
}

std::map<std::string, listener> listeners;

struct P
:conf::top
{
	void set_backlog(listener &, std::string);
	void set_host(listener &, std::string);
	void set_port(listener &, std::string);

	void set(client::client &, std::string label, std::string key, std::string val) override;
	void del(client::client &, const std::string &label, const std::string &key) override;

	using conf::top::top;
}

static P
{
	'P', "listen",
};

mapi::header IRCD_MODULE
{
	"P-Line - instructions for listening sockets"
};

void
P::del(client::client &,
       const std::string &label,
       const std::string &key)
{
	listeners.erase(label);
}

void
P::set(client::client &,
       std::string label,
       std::string key,
       std::string val)
try
{
	auto it(listeners.lower_bound(label));
	if(it == end(listeners) || it->second.name != label)
		it = listeners.emplace_hint(it, label, label);

	auto &listener(it->second);
	switch(hash(key))
	{
		case hash("host"):     set_host(listener, std::move(val));      break;
		case hash("port"):     set_port(listener, std::move(val));      break;
		case hash("backlog"):  set_backlog(listener, std::move(val));   break;
		default:
			conf::log.warning("Unknown P-Line key \"%s\"", key.c_str());
			break;
	}
}
catch(const std::exception &e)
{
	conf::log.error("P-Line \"%s\" set \"%s\": %s",
	                label.c_str(),
	                key.c_str(),
	                e.what());
	throw;
}

void
P::set_port(listener &listener,
            std::string val)
{
	static const auto any(ip::address::from_string("0.0.0.0"));
	const auto &host(listener.host.is_unspecified()? any : listener.host);
/*
	tokens(val, ", ", [&]
	(const std::string &token)
	{
		const auto &port(boost::lexical_cast<uint16_t>(token));
		listener.ep = ip::tcp::endpoint(host, port);
	});
*/
	const auto &port(boost::lexical_cast<uint16_t>(val));
	listener.ep = ip::tcp::endpoint(host, port);
	listener.cond.notify_all();
}

void
P::set_host(listener &listener,
            std::string val)
{
	listener.host = ip::address::from_string(val);
}

void
P::set_backlog(listener &listener,
               std::string val)
{
	listener.backlog = boost::lexical_cast<size_t>(val);
}
