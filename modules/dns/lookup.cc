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

#include "dns.h"

namespace ircd {
namespace js   {

extern trap dns;

struct lookup
:trap::function
{
	trap &future{trap::find("future")};

	value handle_result(const boost::system::error_code &, ip::tcp::resolver::iterator &);
	value on_call(object::handle obj, value::handle, const args &args) override;
	using trap::function::function;
}
static lookup
{
	dns, "lookup"
};


} // namespace js
} // namespace ircd

// system-level hostname resolution
//
// args[0]: hostname     - string
// args[1]: [options]    - object or integer; integer = 4 or 6
// args[2]: [callback]   - function; undefined = yield
//
// https://nodejs.org/api/dns.html#dns_dns_lookup_hostname_options_callback
//
ircd::js::value
ircd::js::lookup::on_call(object::handle obj,
                          value::handle that,
                          const args &args)
{
	const string hostname{args[0]};
	const object options{args.has(1)? args[1] : value{}};         //TODO: Node.js options
	const value callback{args.has(2)? args[2] : value{}};
	const ip::tcp::resolver::query query
	{
		std::string(hostname),
		std::string{}
	};

	auto &task(task::get());
	contract result(ctor(future, vector<value>{{callback}}));
	tcp_resolver.async_resolve(query, [this, result]
	(const boost::system::error_code &ec, ip::tcp::resolver::iterator it)
	mutable
	{
		result(std::bind(&lookup::handle_result, this, std::ref(ec), std::ref(it)));
	});

	return result;
}

ircd::js::value
ircd::js::lookup::handle_result(const boost::system::error_code &ec,
                                ip::tcp::resolver::iterator &it)
{
	static const ip::tcp::resolver::iterator end;

	if(ec)
		throw jserror("%s", ec.message().c_str());

	vector<value> addrs;
	for(; it != end; ++it)
		addrs.append(string(it->endpoint().address().to_string()));

	return object(addrs);
}
