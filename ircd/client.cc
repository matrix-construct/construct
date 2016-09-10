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
#include <ircd/client_sock.h>

using namespace ircd;

namespace ircd   {
namespace client {

} // namespace client
} // namespace ircd

///////////////////////////////////////////////////////////////////////////////
//
// client.h
//

client::client::client(boost::asio::io_service *const &ios)
:sock
{
	std::make_unique<struct sock>(ios)
}
{
}

client::client::~client()
noexcept
{
}

///////////////////////////////////////////////////////////////////////////////
//
// client_sock.h
//

client::sock::sock(boost::asio::io_service *const &ios)
:sd
{
	*ios
}
,timer
{
	*ios
}
{
}
