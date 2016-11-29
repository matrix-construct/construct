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

ircd::client::client(const char *const &type)
:type{type}
,clit{clients.emplace(end(clients), this)}
{
}

ircd::client::client(const char *const &type,
                     std::shared_ptr<socket> sock)
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
	char arbuf[2048];
	parse::buffer pb(arbuf, arbuf + sizeof(arbuf));

    parse::context pc(pb, [this]
    (char *&read, char *const &stop)
    {
        read += sock->read_some(mutable_buffers{{read, stop}});
    });

    const http::request::head header{pc};
    const http::request::body content{pc, header};

	try
	{
		log::debug("Requesting resource \"%s\"", std::string(header.resource).c_str());
		const auto &resource(*resource::resources.at(header.resource));
		const auto &meth(*resource.methods.at(header.method));
		const json::doc doc(content);
		for(const auto &member : doc)
		{
			const auto it(meth.members.find(member.first));
			if(it == meth.members.end())
			{
				log::warning("In method %s of resource %s: Unhandled member \"%s\"",
				             std::string(header.method).data(),
				             std::string(header.resource).data(),
				             std::string(member.first).data());
				continue;
			}

			const auto &mem(*it->second);
			if(mem.valid) try
			{
				mem.valid(member.second);
			}
			catch(const m::error &e)
			{
				throw http::error(http::code::BAD_REQUEST, e.what());
			}
		}

		resource::request req{header, content};
		resource::response response; try
		{
			resource(*this, req, response);
		}
		catch(const std::exception &e)
		{
			ircd::log::error("resource[%s]: %s", resource.name, e.what());
			throw http::error(http::code::INTERNAL_SERVER_ERROR);
		}
	}
	catch(const std::out_of_range &e)
	{
		throw http::error(http::code::NOT_FOUND);
	}

	return true;
}
catch(const http::error &e)
{
	char clen[64];
	const auto clen_size
	{
		size_t(snprintf(clen, sizeof(clen), "Content-Length: %zu\r\n", e.content.size()))
	};

	const const_buffers iov
	{
		{ "HTTP/1.1 ", 9                      },
		{ e.what(), strlen(e.what())          },
		{ "\r\n", 2                           },
		{ clen, clen_size                     },
		{ "\r\n", 2                           },
		{ e.content.data(), e.content.size()  }
	};

	sock->write(iov);
	log::error("http::error: %s", e.what());
	return true;
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

std::shared_ptr<ircd::client>
ircd::add_client(std::shared_ptr<socket> s)
{
	auto client(make_client("client", std::move(s)));
	log::info("New client[%s] on local[%s]",
	          string(remote_address(*client)).c_str(),
	          string(local_address(*client)).c_str());

	async_recv_next(client, seconds(30));
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
	sock(timeout, [client(std::move(client)), timeout]
	(const error_code &ec)
	noexcept
	{
		if(!handle_ec(*client, ec))
			return;

		request([client(std::move(client)), timeout]
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
	          string(remote_address(client)).c_str());

	sock.disconnect();
	return false;
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
{
	if(!client.sock)
		return { "0.0.0.0"s, 0 };

	const auto &sock(*client.sock);
	return { local_ip(sock), local_port(sock) };
}

ircd::ip_port
ircd::remote_address(const client &client)
{
	if(!client.sock)
		return { "0.0.0.0"s, 0 };

	const auto &sock(*client.sock);
	return { remote_ip(sock), remote_port(sock) };
}
