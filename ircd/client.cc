/*
 *  charybdis: an advanced ircd.
 *  client.c: Controls clients.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
 *  Copyright (C) 2007 William Pitcock
 *  Copyright (C) 2016 Charybdis Development Team
 *  Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 */

#include <ircd/asio.h>

namespace ircd {

// Default time limit for how long a client connection can be in "async mode"
// (or idle mode) after which it is disconnected.
const auto async_timeout
{
	40s
};

// Time limit for how long a connected client can be in "request mode." This
// should never be hit unless there's an error in the handling code.
const auto request_timeout
{
	20s
};

// The pool of request contexts. When a client makes a request it does so by acquiring
// a stack from this pool. The request handling and response logic can then be written
// in a synchronous manner as if each connection had its own thread.
ctx::pool request
{
	"request", 4_MiB
};

// Container for all active clients (connections) for iteration purposes.
client::list client::clients;

static bool handle_ec(client &, const error_code &);
void async_recv_next(std::shared_ptr<client>, const milliseconds &timeout);
void async_recv_next(std::shared_ptr<client>);

void close(client &, const net::close_opts &, net::close_callback);
ctx::future<void> close(client &, const net::close_opts & = {});
void close_all();

template<class... args> std::shared_ptr<client> make_client(args&&...);

} // namespace ircd

ircd::client::init::init()
{
	request.add(128);
}

void
ircd::client::init::interrupt()
{
	if(request.active() || !client::clients.empty())
		log::warning("Interrupting %zu requests; dropping %zu requests; closing %zu clients...",
		             request.active(),
		             request.pending(),
		             client::clients.size());

	request.interrupt();
	close_all();
}

ircd::client::init::~init()
noexcept
{
	interrupt();

	if(request.active())
		log::warning("Joining %zu active of %zu remaining request contexts...",
		             request.active(),
		             request.size());
	else
		log::debug("Waiting for %zu request contexts to join...",
		           request.size());

	request.join();

	if(unlikely(!client::clients.empty()))
	{
		log::error("%zu clients are unterminated...", client::clients.size());
		assert(client::clients.empty());
	}
}

ircd::ipport
ircd::local(const client &client)
{
	if(!client.sock)
		return {};

	return net::local_ipport(*client.sock);
}

ircd::ipport
ircd::remote(const client &client)
{
	if(!client.sock)
		return {};

	return net::remote_ipport(*client.sock);
}

ircd::http::response::write_closure
ircd::write_closure(client &client)
{
	// returns a function that can be called to send an iovector of data to a client
	return [&client](const ilist<const const_buffer> &iov)
	{
		//std::cout << "<<<< " << size(iov) << std::endl;
		//std::cout << iov << std::endl;
		//std::cout << "---- " << std::endl;
		const auto written
		{
			write(*client.sock, iov)
		};
	};
}

ircd::parse::read_closure
ircd::read_closure(client &client)
{
	// Returns a function the parser can call when it wants more data
	return [&client](char *&start, char *const &stop)
	{
		try
		{
			char *const got(start);
			read(client, start, stop);
			//std::cout << ">>>> " << std::distance(got, start) << std::endl;
			//std::cout << string_view{got, start} << std::endl;
			//std::cout << "----" << std::endl;
		}
		catch(const boost::system::system_error &e)
		{
			using namespace boost::system::errc;

			switch(e.code().value())
			{
				case operation_canceled:
					throw http::error(http::REQUEST_TIMEOUT);

				default:
					throw;
			}
		}
    };
}

char *
ircd::read(client &client,
           char *&start,
           char *const &stop)
{
	assert(client.sock);
	auto &sock(*client.sock);
	const mutable_buffer buf
	{
		start, stop
	};

	char *const base(start);
	start += net::read(sock, buf);
	return base;
}

const char *
ircd::write(client &client,
            const char *&start,
            const char *const &stop)
{
	assert(client.sock);
	auto &sock(*client.sock);
	const const_buffer buf
	{
		start, stop
	};

	const char *const base(start);
	start += net::write(sock, buf);
	return base;
}

void
ircd::async_recv_next(std::shared_ptr<client> client)
{
	async_recv_next(std::move(client), milliseconds(-1));
}

///
/// This function is the basis for the client's request loop. We still use
/// an asynchronous pattern until there is activity on the socket (a request)
/// in which case the switch to synchronous mode is made by jumping into an
/// ircd::context drawn from the request pool. When the request is finished,
/// the client exits back into asynchronous mode until the next request is
/// received and rinse and repeat.
//
/// This sequence exists to avoid any possible c10k-style limitation imposed by
/// dedicating a context and its stack space to the lifetime of a connection.
/// This is similar to the thread-per-request pattern before async was in vogue.
//
/// Developers: Pay close attention to the comments to know exactly where you
/// are and what you can do at any given point in this sequence.
///
void
ircd::async_recv_next(std::shared_ptr<client> client,
                      const milliseconds &timeout)
{
	assert(bool(client));
	assert(bool(client->sock));

	// This call returns immediately so we no longer block the current context and
	// its stack while waiting for activity on idle connections between requests.

	auto &sock(*client->sock);
	static const auto &op{sock.sd.wait_read};
	sock(op, timeout, [client(std::move(client)), timeout](const error_code &ec)
	noexcept
	{
		// Right here this handler is executing on the main stack (not in any
		// ircd::context).
		if(!handle_ec(*client, ec))
			return;

		// This call returns immediately because we can never block the main stack outside
		// of the ircd::context system. The context the closure ends up getting is the next
		// available from the request pool, which may not be available immediately so this
		// handler might be queued for some time after this call returns.
		request([client(std::move(client)), timeout]
		{
			// Right here this handler is executing on an ircd::context with its own
			// stack dedicated to the lifetime of this request. If client::main()
			// returns true, we bring the client back into async mode to wait for
			// the next request.
			if(client->main())
				async_recv_next(std::move(client), timeout);
		});
	});
}

//
// client
//

ircd::client::client()
:client{std::shared_ptr<socket>{}}
{
}

ircd::client::client(const hostport &hostport,
                     const seconds &timeout)
:client
{
	net::open(hostport)
}
{
}

ircd::client::client(std::shared_ptr<socket> sock)
:clit{clients, clients.emplace(end(clients), this)}
,sock{std::move(sock)}
,request_timer{ircd::timer::nostart}
{
}

ircd::client::~client()
noexcept try
{
	//assert(!sock || !connected(*sock));
}
catch(const std::exception &e)
{
	log::critical("~client(%p): %s", this, e.what());
	return;
}

/// Client main.
///
/// Before main(), the client had been sitting in async mode waiting for
/// socket activity. Once activity with data was detected indicating a request,
/// the client was dispatched to the request pool where it is paired to an
/// ircd::ctx with a stack. main() is then invoked on that ircd::ctx stack.
/// Nothing from the socket has been read into userspace before main().
///
bool
ircd::client::main()
noexcept try
{
	const auto header_max{8_KiB};
	const auto content_max{8_MiB}; //TODO: XXX
	const unique_buffer<const mutable_buffer> buffer
	{
		header_max + content_max
	};

	parse::buffer pb{buffer};
	return handle(pb);
}
catch(const std::exception &e)
{
	log::error("client[%s] [500 Internal Error]: %s",
	           string(remote(*this)),
	           e.what());

	#ifdef RB_DEBUG
		throw;
	#else
		return false;
	#endif
}

/// Main request loop.
///
/// This function parses requests off the socket in a loop until there are no
/// more requests or there is a fatal error. The ctx will "block" to wait for
/// more data off the socket during the middle of a request until the request
/// timeout is reached. main() will not "block" to wait for more data after a
/// request; it will simply `return true` which puts this client back into
/// async mode and relinquishes this stack. returning false will disconnect
/// the client rather than putting it back into async mode. Exceptions do not
/// pass below main() therefor anything unhandled is an internal server error
/// and the client is disconnected as well.
///
bool
ircd::client::handle(parse::buffer &pb)
try
{
	parse::capstan pc{pb, read_closure(*this)}; do
	{
		request_timer = ircd::timer{};
		const socket::scope_timeout timeout
		{
			*sock, request_timeout, [client(shared_from(*this))]
			(const error_code &ec)
			{
				if(!ec)
					close(*client, net::dc::SSL_NOTIFY, net::close_ignore);
			}
		};

		if(!handle_request(*this, pc))
			return false;

		pb.remove();
	}
	while(pc.unparsed());

	return true;
}
catch(const boost::system::system_error &e)
{
	using namespace boost::system::errc;
	using boost::system::system_category;
	using boost::asio::error::get_ssl_category;
	using boost::asio::error::get_misc_category;

	log::debug("client(%p): handle error: %s",
	           (const void *)this,
	           string(e.code()));

	const error_code &ec{e.code()};
	const int &value{ec.value()};
	if(ec.category() == system_category()) switch(value)
	{
		case success:
			assert(0);
			return true;

		case broken_pipe:
		case connection_reset:
		case not_connected:
			close(*this, net::dc::RST, net::close_ignore);
			return false;

		case operation_canceled:
			close(*this, net::dc::SSL_NOTIFY).wait();
			return false;

		case bad_file_descriptor:
			return false;

		default:
			break;
	}
	else if(ec.category() == get_ssl_category()) switch(uint8_t(value))
	{
		case SSL_R_SHORT_READ:
			close(*this, net::dc::RST, net::close_ignore);
			return false;

		case SSL_R_PROTOCOL_IS_SHUTDOWN:
			close(*this, net::dc::RST, net::close_ignore);
			return false;

		default:
			break;
	}
	else if(ec.category() == get_misc_category()) switch(value)
	{
		case boost::asio::error::eof:
			close(*this, net::dc::RST, net::close_ignore);
			return false;

		default:
			break;
	}

	log::error("client(%p): (unexpected) %s: (%d) %s",
	           (const void *)this,
	            ec.category().name(),
	            value,
	            ec.message());

	close(*this, net::dc::RST, net::close_ignore);
	return false;
}

std::shared_ptr<ircd::client>
ircd::add_client(std::shared_ptr<socket> s)
{
	const auto client
	{
		make_client(std::move(s))
	};

	log::debug("client[%s] CONNECTED local[%s]",
	           string(remote(*client)),
	           string(local(*client)));

	async_recv_next(client, async_timeout);
	return client;
}

template<class... args>
std::shared_ptr<ircd::client>
ircd::make_client(args&&... a)
{
	return std::make_shared<client>(std::forward<args>(a)...);
}

void
ircd::close_all()
{
	auto it(begin(client::clients));
	while(it != end(client::clients))
	{
		auto *const client(*it);
		++it; try
		{
			close(*client, net::dc::RST, net::close_ignore);
		}
		catch(const std::exception &e)
		{
			log::warning("Error disconnecting client @%p: %s", client, e.what());
		}
	}
}

ircd::ctx::future<void>
ircd::close(client &client,
            const net::close_opts &opts)
{
	if(likely(client.sock))
		return close(*client.sock, opts);
	else
		return {};
}

void
ircd::close(client &client,
            const net::close_opts &opts,
            net::close_callback callback)
{
	close(*client.sock, opts, std::move(callback));
}

namespace ircd
{
	static bool handle_ec_success(client &);
	static bool handle_ec_timeout(client &);
	static bool handle_ec_eof(client &);
	static bool handle_ec_short_read(client &);
	static bool handle_ec_default(client &, const error_code &);
}

bool
ircd::handle_ec(client &client,
                const error_code &ec)
{
	using namespace boost::system::errc;
	using boost::system::system_category;
	using boost::asio::error::get_ssl_category;
	using boost::asio::error::get_misc_category;

	if(ec.category() == system_category()) switch(ec.value())
	{
		case success:                return handle_ec_success(client);
		case operation_canceled:     return handle_ec_timeout(client);
		default:                     return handle_ec_default(client, ec);
	}
	else if(ec.category() == get_misc_category()) switch(ec.value())
	{
		case asio::error::eof:       return handle_ec_eof(client);
		default:                     return handle_ec_default(client, ec);
	}
	else if(ec.category() == get_ssl_category()) switch(ec.value())
	{
		case SSL_R_SHORT_READ:       return handle_ec_short_read(client);
		default:                     return handle_ec_default(client, ec);
	}
	else return handle_ec_default(client, ec);
}

bool
ircd::handle_ec_default(client &client,
                        const error_code &ec)
{
	log::debug("client(%p): %s: %s",
	           &client,
	           ec.category().name(),
	           ec.message());

	close(client, net::dc::SSL_NOTIFY, net::close_ignore);
	return false;
}

bool
ircd::handle_ec_short_read(client &client)
try
{
	log::warning("client[%s]: short_read",
	             string(remote(client)));

	close(client, net::dc::RST, net::close_ignore);
	return false;
}
catch(const std::exception &e)
{
	log::error("client(%p): short_read: %s",
	           &client,
	           e.what());

	return false;
}

bool
ircd::handle_ec_eof(client &client)
try
{
	log::debug("client[%s]: EOF",
	           string(remote(client)));

	close(client, net::dc::RST, net::close_ignore);
	return false;
}
catch(const std::exception &e)
{
	log::error("client(%p): EOF: %s",
	           &client,
	           e.what());

	return false;
}

bool
ircd::handle_ec_timeout(client &client)
try
{
	assert(bool(client.sock));
	log::warning("client[%s]: disconnecting after inactivity timeout",
	             string(remote(client)));

	close(client, net::dc::SSL_NOTIFY, net::close_ignore);
	return false;
}
catch(const std::exception &e)
{
	log::error("client(%p): timeout: %s",
	           &client,
	           e.what());

	return false;
}

bool
ircd::handle_ec_success(client &client)
{
	return true;
}
