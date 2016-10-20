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
#include <ircd/ctx/ctx.h>

namespace ip = boost::asio::ip;
using namespace ircd;

struct m_host
:cmd
{
	void operator()(client &, line) override;

	using cmd::cmd;
}

static m_host
{
	"host"
};

ip::tcp::resolver tcp_resolver
{
	*ircd::ios
};

ctx::pool pool
{
	1,      // There can be X concurrent hostname lookups (if posted to this pool)
	16_KiB  // The stack size is 16 kilobytes (which is pretty small, so just resolve hostnames)
};

mapi::fini fini{[]
{
	tcp_resolver.cancel();
	pool.del(pool.size());
}};

mapi::header IRCD_MODULE
{
	"host - DNS command",
	mapi::NO_FLAGS,
	&fini
};

void
m_host::operator()(client &client,
                   line line)
try
{
	const life_guard<struct client> lg(client);

	const auto &host(line[0]);
	const ip::tcp::resolver::query query(host, std::string{});
	auto epit(tcp_resolver.async_resolve(query, yield(continuation())));
	static const ip::tcp::resolver::iterator end;
	for(; epit != end; ++epit)
		sendf(client, "lookup for %s returned [%s]",
		      host,
		      epit->endpoint().address().to_string());
}
catch(const std::exception &e)
{
	std::cerr << "error: " << e.what() << std::endl;
}
