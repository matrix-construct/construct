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

#include "net.h"

namespace ircd {
namespace js   {

extern trap socket;                       // socket.cc
extern trap server;                       // server.cc

struct net
:trap
{
	bool on_has(object::handle obj, id::handle id) override
	{
		if(id == "Server")
			return true;

		if(id == "Socket")
			return true;

		return false;
	}

	value on_get(object::handle obj, id::handle id, value::handle val) override;

	void on_new(object::handle outer, object &that, const args &args) override
	{
	}

	using trap::trap;
}
net
__attribute__((init_priority(1000)))
{
	"net"
};

} // namespace js
} // namespace ircd

ircd::js::value
ircd::js::net::on_get(object::handle obj,
                      id::handle id,
                      value::handle val)
{
	if(!undefined(val))
		return val;

	if(id == "Server")
		return ctor(server);

	if(id == "Socket")
		return ctor(socket);

	return val;
}

ircd::mapi::header IRCD_MODULE
{
	"Network server and socket support."
};
