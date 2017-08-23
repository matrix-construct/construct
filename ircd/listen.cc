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

#include <ircd/socket.h>

namespace ircd {

const size_t DEFAULT_STACK_SIZE
{
	1_MiB  // can be optimized
};

struct listener::acceptor
{
	using error_code = boost::system::error_code;

	static log::log log;

	std::string name;
	size_t backlog;
	asio::ssl::context ssl;
	ip::tcp::endpoint ep;
	ip::tcp::acceptor a;

	explicit operator std::string() const;
	void configure(const json::object &opts);

	// Handshake stack
	bool handshake_error(const error_code &ec);
	void handshake(const error_code &ec, std::shared_ptr<socket>) noexcept;

	// Acceptance stack
	bool accept_error(const error_code &ec);
	void accept(const error_code &ec, std::shared_ptr<socket>) noexcept;

	// Accept next
	void next();

	acceptor(const json::object &opts);
	~acceptor() noexcept;
};

} // namespace ircd

///////////////////////////////////////////////////////////////////////////////
//
// ircd::listener
//

ircd::listener::listener(const json::object &opts)
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

ircd::listener::acceptor::acceptor(const json::object &opts)
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
{
	static const ip::tcp::acceptor::reuse_address reuse_address{true};

	configure(opts);

	log.debug("%s configured listener SSL",
	          std::string(*this));

	a.open(ep.protocol());
	a.set_option(reuse_address);
	log.debug("%s opened listener socket",
	          std::string(*this));

	a.bind(ep);
	log.debug("%s bound listener socket",
	          std::string(*this));

	a.listen(backlog);
	log.debug("%s listening (backlog: %lu)",
	          std::string(*this),
	          backlog);

	next();
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
ircd::listener::acceptor::next()
try
{
	auto sock(std::make_shared<ircd::socket>(ssl));
	log.debug("%s: listening with next socket(%p)",
	          std::string(*this),
	          sock.get());

	// The context blocks here until the next client is connected.
	auto accept(std::bind(&acceptor::accept, this, ph::_1, sock));
	a.async_accept(sock->sd, accept);
}
catch(const std::exception &e)
{
	log.critical("%s: %s",
	             std::string(*this),
	             e.what());

	if(ircd::debugmode)
		throw;
}

void
ircd::listener::acceptor::accept(const error_code &ec,
                                 const std::shared_ptr<socket> sock)
noexcept try
{
	if(accept_error(ec))
		return;

	log.debug("%s: accepted %s",
	          std::string(*this),
	          string(sock->remote()));

	static const auto handshake_type(socket::handshake_type::server);
	auto handshake(std::bind(&acceptor::handshake, this, ph::_1, sock));
	sock->ssl.async_handshake(handshake_type, handshake);
	next();
}
catch(const std::exception &e)
{
	log.error("%s: in accept(): socket(%p): %s",
	          std::string(*this),
	          sock.get(),
	          e.what());
	next();
}

bool
ircd::listener::acceptor::accept_error(const error_code &ec)
{
	switch(ec.value())
	{
		using namespace boost::system::errc;

		case success:
			return false;

		case operation_canceled:
			return true;

		default:
			throw boost::system::system_error(ec);
	}
}

void
ircd::listener::acceptor::handshake(const error_code &ec,
                                    const std::shared_ptr<socket> sock)
noexcept try
{
	if(handshake_error(ec))
		return;

	log.debug("%s SSL handshook %s",
	          std::string(*this),
	          string(sock->remote()));

	add_client(sock);
}
catch(const std::exception &e)
{
	log.error("%s: in handshake(): socket(%p)[%s]: %s",
	          std::string(*this),
	          sock.get(),
	          string(sock->remote()),
	          e.what());
}

bool
ircd::listener::acceptor::handshake_error(const error_code &ec)
{
	switch(ec.value())
	{
		using namespace boost::system::errc;

		case success:
			return false;

		case operation_canceled:
			return true;

		default:
			throw boost::system::system_error(ec);
	}
}

void
ircd::listener::acceptor::configure(const json::object &opts)
{
	log.debug("%s preparing listener socket configuration...",
	          std::string(*this));

	ssl.set_options
	(
		ssl.default_workarounds
		// | ssl.no_sslv2
		// | ssl.single_dh_use
	);

	//TODO: XXX
	ssl.set_password_callback([this]
	(const auto &size, const auto &purpose)
	{
		log.debug("%s asking for password with purpose '%s' (size: %zu)",
		          std::string(*this),
		          purpose,
		          size);

		//XXX: TODO
		return "foobar";
	});

	if(opts.has("ssl_certificate_chain_file"))
	{
		const std::string filename
		{
			unquote(opts["ssl_certificate_chain_file"])
		};

		ssl.use_certificate_chain_file(filename);
		log.info("%s using certificate chain file '%s'",
		          std::string(*this),
		          filename);
	}

	if(opts.has("ssl_certificate_file_pem"))
	{
		const std::string filename
		{
			unquote(opts["ssl_certificate_file_pem"])
		};

		ssl.use_certificate_file(filename, asio::ssl::context::pem);
		log.info("%s using certificate file '%s'",
		          std::string(*this),
		          filename);
	}

	if(opts.has("ssl_private_key_file_pem"))
	{
		const std::string filename
		{
			unquote(opts["ssl_private_key_file_pem"])
		};

		ssl.use_private_key_file(filename, asio::ssl::context::pem);
		log.info("%s using private key file '%s'",
		          std::string(*this),
		          filename);
	}

	if(opts.has("ssl_tmp_dh_file"))
	{
		const std::string filename
		{
			unquote(opts["ssl_tmp_dh_file"])
		};

		ssl.use_tmp_dh_file(filename);
		log.info("%s using tmp dh file '%s'",
		          std::string(*this),
		          filename);
	}
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
