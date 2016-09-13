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

#include <boost/asio.hpp>
#include <ircd/sock.h>
#include "rbuf.h"

namespace ircd
{
	struct client
	:std::enable_shared_from_this<client>
	{
		struct rbuf rbuf;
		clist::const_iterator clit;
		std::shared_ptr<struct sock> sock;

		client();
		client(const client &) = delete;
		client &operator=(const client &) = delete;
		~client() noexcept;
	};

	clist clients_list;

	bool handle_ec_eof(client &);
	bool handle_ec_cancel(client &);
	bool handle_ec_success(client &);
	bool handle_ec(client &, const error_code &);
	void handle_recv(client &, const error_code &, const size_t);
}

using namespace ircd;

client::client()
{
}

client::~client()
noexcept
{
}

const clist &
ircd::clients()
{
	return clients_list;
}

std::shared_ptr<client>
ircd::add_client(std::shared_ptr<struct sock> sock)
{
	auto client(add_client());
	client->sock = std::move(sock);
	log::info("New client[%s] on local[%s]",
	          string(remote_address(*client)).c_str(),
	          string(local_address(*client)).c_str());

	recv_next(*client);
	return client;
}

std::shared_ptr<client>
ircd::add_client()
{
	auto client(std::make_shared<client>());
	client->clit = clients_list.emplace(end(clients_list), client);
	return client;
}

bool
ircd::disconnect(std::nothrow_t,
                 client &client,
                 const dc &type)
noexcept try
{
	disconnect(client, type);
	return true;
}
catch(...)
{
	return false;
}

void
ircd::disconnect(client &client,
                 const dc &type)
{
	auto &sd(socket(client).sd);
	switch(type)
	{
		case dc::RST:
			sd.close();
			break;

		case dc::FIN:
			sd.shutdown(ip::tcp::socket::shutdown_both);
			break;

		case dc::FIN_SEND:
			sd.shutdown(ip::tcp::socket::shutdown_send);
			break;

		case dc::FIN_RECV:
			sd.shutdown(ip::tcp::socket::shutdown_receive);
			break;
	}
}

bool
ircd::connected(const client &client)
noexcept
try
{
	return socket(client).sd.is_open();
}
catch(...)
{
	return false;
}

void
ircd::recv_next(client &client)
{
	recv_next(client, std::chrono::milliseconds(-1));
}

void
ircd::recv_next(client &client,
                const std::chrono::milliseconds &timeout)
{
	using boost::asio::async_read;

	auto &rbuf(client.rbuf);
	rbuf.reset();

	auto &sock(*client.sock);
	sock.set_timeout(timeout);
	async_read(sock.sd, mutable_buffers_1(rbuf.buf.data(), rbuf.buf.size()),
	           std::bind(&rbuf::handle_pck, &rbuf, ph::_1, ph::_2),
	           std::bind(&ircd::handle_recv, std::ref(client), ph::_1, ph::_2));
}

void
ircd::recv_cancel(client &client)
{
	auto &sock(socket(client));
	sock.sd.cancel();
}

void
ircd::handle_recv(client &client,
                  const error_code &ec,
                  const size_t bytes)
try
{
	if(!handle_ec(client, ec))
		return;

	auto &rbuf(client.rbuf);
	execute(client, rbuf.reel);
}
catch(const std::exception &e)
{
	log::error("client[%s]: error: %s",
	           string(remote_address(client)).c_str(),
	           e.what());

	finished(client);
}

bool
ircd::handle_ec(client &client,
                const error_code &ec)
{
	using namespace boost::system::errc;
	using boost::asio::error::eof;

	if(client.rbuf.eptr)
		std::rethrow_exception(client.rbuf.eptr);

	switch(ec.value())
	{
		case success:                return handle_ec_success(client);
		case operation_canceled:     return handle_ec_cancel(client);
		case eof:                    return handle_ec_eof(client);
		default:                     throw boost::system::system_error(ec);
	}
}

bool
ircd::handle_ec_success(client &client)
{
	auto &sock(*client.sock);
	{
		error_code ec;
		sock.timer.cancel(ec);
		assert(ec == boost::system::errc::success);
	}

	return true;
}

bool
ircd::handle_ec_cancel(client &client)
{
	auto &sock(*client.sock);

	// The cancel can come from a timeout or directly.
	// If directly, the timer may still needs to be canceled
	if(!sock.timedout)
	{
		error_code ec;
		sock.timer.cancel(ec);
		assert(ec == boost::system::errc::success);
		log::debug("client[%s]: recv canceled", string(remote_address(client)).c_str());
		return false;
	}

	log::debug("client[%s]: recv timeout", string(remote_address(client)).c_str());
	finished(client);
	return false;
}

bool
ircd::handle_ec_eof(client &client)
{
	log::debug("client[%s]: eof", string(remote_address(client)).c_str());
	finished(client);
	return false;
}

void
ircd::finished(client &client)
{
	const auto p(shared_from(client));
	clients_list.erase(client.clit);
	log::debug("client[%p] finished. (refs: %zu)", (const void *)p.get(), p.use_count());
}

std::string
ircd::string(const ip_port &pair)
{
	std::string ret(64, '\0');
	ret.resize(snprintf(&ret.front(), ret.size(), "%s:%u",
	                    pair.first.c_str(),
	                    pair.second));
	return ret;
}

ircd::ip_port
ircd::local_address(const client &client)
try
{
	const auto &sock(socket(client));
	return { local_ip(sock), local_port(sock) };
}
catch(const std::bad_weak_ptr &)
{
	return { "0.0.0.0"s, 0 };
}

ircd::ip_port
ircd::remote_address(const client &client)
try
{
	const auto &sock(socket(client));
	return { remote_ip(sock), remote_port(sock) };
}
catch(const std::bad_weak_ptr &)
{
	return { "0.0.0.0"s, 0 };
}

sock &
ircd::socket(client &client)
{
	if(unlikely(!has_socket(client)))
		throw std::bad_weak_ptr();

	return *client.sock;
}

const sock &
ircd::socket(const client &client)
{
	if(unlikely(!has_socket(client)))
		throw std::bad_weak_ptr();

	return *client.sock;
}

bool
ircd::has_socket(const client &client)
{
	return bool(client.sock);
}

std::weak_ptr<client>
ircd::weak_from(client &client)
{
	return shared_from(client);
}

std::weak_ptr<const client>
ircd::weak_from(const client &client)
{
	return shared_from(client);
}

std::shared_ptr<client>
ircd::shared_from(client &client)
{
	return client.shared_from_this();
}

std::shared_ptr<const client>
ircd::shared_from(const client &client)
{
	return client.shared_from_this();
}
