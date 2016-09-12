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

#ifdef __cplusplus
namespace ircd {

struct rbuf
{
	tape reel;
	std::exception_ptr eptr;
	std::array<char, BUFSIZE> buf alignas(16);
	uint16_t checked;
	uint16_t length;

	bool terminated() const;                 // rbuf has CRLF termination
	uint16_t remaining() const;              // bytes remaining in rbuf

	size_t handle_pck(const error_code &, const size_t) noexcept;
	void reset();

	rbuf();
};

static_assert(BUFSIZE == 512, "");

inline
rbuf::rbuf()
:checked{0}
,length{0}
{
}

inline void
rbuf::reset()
{
	checked = 0;
	length = 0;
}

inline size_t
rbuf::handle_pck(const error_code &ec,
                 const size_t bytes)
noexcept try
{
	if(ec)
		return 0;

	length += bytes;
	if(reel.append(buf.data(), length))
		return 0;

	if(terminated())
		throw rfc1459::syntax_error("invalid syntax"); //TODO: eps + ERR_

	checked = length;
	return remaining()?: throw rfc1459::syntax_error("message length exceeded"); //TODO: ERR_
}
catch(...)
{
	eptr = std::current_exception();
	return 0;
}

inline uint16_t
ircd::rbuf::remaining()
const
{
	return buf.size() - length;
}

inline bool
rbuf::terminated()
const
{
	const auto b(std::next(buf.rbegin(), remaining()));
	const auto e(std::next(buf.rbegin(), buf.size() - checked));
	return std::find(b, e, '\n') != e;
}

}      // namespace ircd
#endif // __cplusplus

namespace ircd
{
	struct client
	:std::enable_shared_from_this<client>
	{
		struct rbuf rbuf;
		clist::const_iterator clit;
		std::unique_ptr<struct sock> sock;

		client();
		client(const client &) = delete;
		client &operator=(const client &) = delete;
		~client() noexcept;
	};

	clist clients_list;

	bool handle_error(client &, const error_code &);
	void handle_recv(client &, const error_code &, const size_t);
	void set_recv(client &);
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
ircd::add_client(std::unique_ptr<struct sock> sock)
{
	auto client(add_client());
	client->sock = std::move(sock);
	log::info("New client[%s] on local[%s]",
	          string(remote_address(*client)).c_str(),
	          string(local_address(*client)).c_str());

	set_recv(*client);
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
ircd::set_recv(client &client)
{
	using boost::asio::async_read;

	auto &rbuf(client.rbuf);
	rbuf.reset();

	auto &sock(*client.sock);
	async_read(sock.sd, mutable_buffers_1(rbuf.buf.data(), rbuf.buf.size()),
	           std::bind(&rbuf::handle_pck, &rbuf, ph::_1, ph::_2),
	           std::bind(&ircd::handle_recv, std::ref(client), ph::_1, ph::_2));
}

void
ircd::handle_recv(client &client,
                  const error_code &ec,
                  const size_t bytes)
try
{
	if(!handle_error(client, ec))
		return;

	auto &rbuf(client.rbuf);
	auto &reel(rbuf.reel);
	for(const auto &line : reel)
		std::cout << line << std::endl;

	reel.clear();
	set_recv(client);
}
catch(const rfc1459::syntax_error &e)
{
	std::cerr << e.what() << std::endl;
}
catch(const std::exception &e)
{
	std::cerr << "errored: " << e.what() << std::endl;
}

bool
ircd::handle_error(client &client,
                   const error_code &ec)
{
	using namespace boost::system::errc;
	using boost::asio::error::eof;

	if(client.rbuf.eptr)
		std::rethrow_exception(client.rbuf.eptr);

	switch(ec.value())
	{
		case success:    return true;
		default:         throw boost::system::system_error(ec);
	}
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

std::shared_ptr<client>
ircd::shared_from(client &client)
{
	return client.shared_from_this();
}
