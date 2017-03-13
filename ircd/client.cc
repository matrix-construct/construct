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

#include <ircd/socket.h>
#include <ircd/lex_cast.h>

namespace ircd {

ctx::pool request
{
	"request", 1_MiB
};

client::list client::clients
{};

bool handle_ec_timeout(client &);
bool handle_ec_eof(client &);
bool handle_ec_success(client &);
bool handle_ec(client &, const error_code &);

void async_recv_next(std::shared_ptr<client>, const milliseconds &timeout);
void async_recv_next(std::shared_ptr<client>);

void disconnect(client &, const socket::dc & = socket::dc::FIN);
void disconnect_all();

template<class... args> std::shared_ptr<client> make_client(args&&...);

} // namespace ircd

ircd::client::init::init()
{
	request.add(1);
}

ircd::client::init::~init()
noexcept
{
	request.interrupt();
	disconnect_all();
}

ircd::http::response::write_closure
ircd::write_closure(client &client)
{
	return [&client](const const_buffers &iov)
	{
		write(*client.sock, iov);
	};
}

ircd::parse::read_closure
ircd::read_closure(client &client)
{
    return [&client](char *&start, char *const &stop)
    {
        read(client, start, stop);
    };
}

ircd::string_view
ircd::readline(client &client,
               char *&start,
               char *const &stop)
{
	auto &sock(*client.sock);

	size_t pos;
	string_view ret;
	char *const base(start); do
	{
		const std::array<mutable_buffer, 1> bufs
		{{
			{ start, stop }
		}};

		start += sock.read_some(bufs);
		ret = {base, start};
		pos = ret.find("\r\n");
	}
	while(pos != std::string_view::npos);

	return { begin(ret), std::next(begin(ret), pos + 2) };
}

char *
ircd::read(client &client,
           char *&start,
           char *const &stop)
{
	auto &sock(*client.sock);
	const std::array<mutable_buffer, 1> bufs
	{{
		{ start, stop }
	}};

	char *const base(start);
	start += sock.read_some(bufs);
	return base;
}

const char *
ircd::write(client &client,
            const char *&start,
            const char *const &stop)
{
	auto &sock(*client.sock);
	const std::array<const_buffer, 1> bufs
	{{
		{ start, stop }
	}};

	const char *const base(start);
	start += sock.write(bufs);
	return base;
}

ircd::host_port
ircd::local_addr(const client &client)
{
	if(!client.sock)
		return { "0.0.0.0"s, 0 };

	const auto &sock(*client.sock);
	const auto &ep(sock.local());
	return { hostaddr(ep), port(ep) };
}

ircd::host_port
ircd::remote_addr(const client &client)
{
	if(!client.sock)
		return { "0.0.0.0"s, 0 };

	const auto &sock(*client.sock);
	const auto &ep(sock.remote());
	return { hostaddr(ep), port(ep) };
}

ircd::client::client()
:client{std::shared_ptr<socket>{}}
{
}

ircd::client::client(const host_port &host_port,
                     const seconds &timeout)
:client
{
	std::make_shared<socket>(host_port.first, host_port.second, timeout)
}
{
}

ircd::client::client(std::shared_ptr<socket> sock)
:type{type}
,clit{clients.emplace(end(clients), this)}
,sock{std::move(sock)}
{
}

ircd::client::~client()
noexcept
{
	clients.erase(clit);
}

bool
ircd::client::main()
noexcept try
{
	return serve();
}
catch(const boost::system::system_error &e)
{
	log::error("system_error: %s", e.what());
	return false;
}
catch(const std::exception &e)
{
	log::error("exception: %s", e.what());
	return false;
}

namespace ircd
{
	void handle_request(client &client, parse::capstan &pc, const http::request::head &head);
	bool handle_request(client &client, parse::capstan &pc);

} // namepace ircd

bool
ircd::client::serve()
try
{
	char buffer[2048];
	parse::buffer pb{buffer, buffer + sizeof(buffer)};
	parse::capstan pc{pb, read_closure(*this)}; do
	{
		if(!handle_request(*this, pc))
			break;

		pb.remove();
	}
	while(pc.unparsed());

	return true;
}
catch(const std::exception &e)
{
	log::error("client[%s] [500 Internal Error]: %s",
	           string(remote_addr(*this)).c_str(),
	           e.what());

	return false;
}

bool
ircd::handle_request(client &client,
                     parse::capstan &pc)
try
{
	http::request
	{
		pc, nullptr, write_closure(client), [&client, &pc]
		(const auto &head)
		{
			handle_request(client, pc, head);
		}
	};

	return true;
}
catch(const http::error &e)
{
	log::debug("client[%s] http::error(%d): %s",
	           string(remote_addr(client)).c_str(),
	           int(e.code),
	           e.what());

	switch(e.code)
	{
		case http::BAD_REQUEST:               return false;
		case http::INTERNAL_SERVER_ERROR:     return false;
		default:                              return true;
	}
}

void
ircd::handle_request(client &client,
                     parse::capstan &pc,
                     const http::request::head &head)
try
{
	log::debug("client[%s] requesting resource \"%s\"",
	           string(remote_addr(client)).c_str(),
	           std::string(head.resource).c_str());

	const auto &resource(*resource::resources.at(head.resource));
	resource(client, pc, head);
}
catch(const std::out_of_range &e)
{
	throw http::error(http::code::NOT_FOUND);
}

std::shared_ptr<ircd::client>
ircd::add_client(std::shared_ptr<socket> s)
{
	const auto client(std::make_shared<ircd::client>(std::move(s)));
	log::info("New client[%s] on local[%s]",
	          string(remote_addr(*client)).c_str(),
	          string(local_addr(*client)).c_str());

	async_recv_next(client, 30s);
	return client;
}

template<class... args>
std::shared_ptr<ircd::client>
ircd::make_client(args&&... a)
{
	return std::make_shared<client>(std::forward<args>(a)...);
}

void
ircd::disconnect_all()
{
	for(auto &client : client::clients) try
	{
		disconnect(*client, socket::dc::RST);
	}
	catch(...)
	{
	}
}

void
ircd::disconnect(client &client,
                 const socket::dc &type)
{
	auto &sock(*client.sock);
	sock.disconnect(type);
}

void
ircd::async_recv_next(std::shared_ptr<client> client)
{
	async_recv_next(std::move(client), milliseconds(-1));
}

void
ircd::async_recv_next(std::shared_ptr<client> client,
                      const milliseconds &timeout)
{
	auto &sock(*client->sock);
	sock(timeout, [client, timeout]
	(const error_code &ec)
	noexcept
	{
		if(!handle_ec(*client, ec))
			return;

		request([client, timeout]
		{
			if(client->main())
				async_recv_next(client, timeout);
		});
	});
}

bool
ircd::handle_ec(client &client,
                const error_code &ec)
{
	using namespace boost::system::errc;
	using boost::asio::error::eof;

	switch(ec.value())
	{
		case success:                return handle_ec_success(client);
		case eof:                    return handle_ec_eof(client);
		case operation_canceled:     return handle_ec_timeout(client);
		default:                     throw boost::system::system_error(ec);
	}
}

bool
ircd::handle_ec_success(client &client)
{
	return true;
}

bool
ircd::handle_ec_eof(client &client)
{
	return false;
}

bool
ircd::handle_ec_timeout(client &client)
{
	auto &sock(*client.sock);
	log::debug("client[%s]: disconnecting after inactivity timeout",
	          string(remote_addr(client)).c_str());

	sock.disconnect();
	return false;
}

std::string
ircd::string(const host_port &pair)
{
	std::string ret(64, '\0');
	ret.resize(snprintf(&ret.front(), ret.size(), "%s:%u",
	                    pair.first.c_str(),
	                    pair.second));
	return ret;
}
