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

extern trap socket;

struct server
:trap
{
	struct state; // privdata

	struct listen;
	struct close;

	void on_new(object::handle, object &, const args &) override;
	void on_trace(const JSObject *const &) override;
	void on_gc(JSObject *const &that) override;

	server();
}
server __attribute__((init_priority(1002)));

struct server::state
:priv_data
{
	ip::tcp::endpoint ep;
	ip::tcp::acceptor acceptor {*ios};

	state(const string &host, const value &port);
	~state() noexcept;
};

struct server::listen
:trap::function
{
	trap &future{trap::find("future")};

	value on_call(object::handle, value::handle, const args &args) override;
	using trap::function::function;
}
static listen{server, "listen"};

struct server::close
:trap::function
{
	trap &future{trap::find("future")};

	value on_call(object::handle, value::handle, const args &args) override;
	using trap::function::function;
}
static close{server, "close"};

} // namespace js
} // namespace ircd

ircd::js::server::state::state(const string &host,
                               const value &port)
:ep{ip::address::from_string(host), uint16_t(port)}
{
}

ircd::js::server::state::~state()
noexcept
{
}

ircd::js::server::server()
:trap
{
	"Server",         // Uppercase for Node.js compatibility.
	JSCLASS_HAS_PRIVATE,
}
{
	parent_prototype = &trap::find("events");
}

void
ircd::js::server::on_new(object::handle that,
                         object &obj,
                         const args &args)
{
}

void
ircd::js::server::on_trace(const JSObject *const &that)
{
	trap::on_trace(that);
}

void
ircd::js::server::on_gc(JSObject *const &that)
try
{
	const scope always_parent([this, &that]
	{
		trap::on_gc(that);
	});

	if(!has(that, priv))
		return;

	auto &state(get<struct state>(that, priv));
	auto &acceptor(state.acceptor);
	boost::system::error_code ec;
	acceptor.cancel(ec);
}
catch(const std::exception &e)
{
	log.warning("server::on_gc(%p): %s\n",
	            (const void *)this,
	            e.what());
}

ircd::js::value
ircd::js::server::close::on_call(object::handle obj,
                                 value::handle _that,
                                 const args &args)
{
	object that(_that);

	object emission;
	set(emission, "event", "close");
	set(emission, "emitter", that);

	contract result(ctor(future));
	set(result.future, "emit", emission);
	if(args.has(0))
		set(result.future, "callback", args[0]);

	auto &state(get<struct state>(that, priv));
	auto &acceptor(state.acceptor);
	result([&acceptor]
	{
		boost::system::error_code ec;
		acceptor.close(ec);
		jserror e("%s", ec.message().c_str());
		return e.val.get();
	});

	return result;
}


ircd::js::value
ircd::js::server::listen::on_call(object::handle obj,
                                  value::handle _that,
                                  const args &args)
{
	object that(_that);
	object opts(args[0]);

	const value port
	{
		has(opts, "port")? get(opts, "port") : value{6667}
	};

	const string host
	{
		has(opts, "host")? get(opts, "host") : value{"localhost"}
	};

	const value backlog
	{
		has(opts, "backlog")? get(opts, "backlog") : value{4096}
	};

	const string path
	{
		has(opts, "path")? get(opts, "path") : value{}
	};

	const bool exclusive
	{
		has(opts, "exclusive")? bool(get(opts, "exclusive")) : false
	};

	if(!has(that, priv))
	{
		auto state(std::make_shared<struct state>(host, port));
		state->acceptor.open(state->ep.protocol());
		state->acceptor.set_option(ip::tcp::socket::reuse_address(true));
		state->acceptor.bind(state->ep);
		state->acceptor.listen(uint(backlog));
		set(that, state);
	}

	auto &state(get<struct state>(that, priv));

	object emission;
	set(emission, "event", "connection");
	set(emission, "emitter", that);

	contract result(ctor(future));
	set(result.future, "emit", emission);
	set(result.future, "cancel", get(that, "close"));
	if(args.has(1))
		set(result.future, "callback", args[1]);

	object socket_instance(ctor(socket));
	auto &sstate(get<socket::state>(socket_instance, priv));
	auto accepted([result, socket_instance(heap_object(socket_instance)), state(shared_from(state)), sstate(shared_from(sstate))]
	(const boost::system::error_code &e)
	mutable
	{
		result([&e, &result, &state, &socket_instance]() -> value
		{
			if(e)
				throw jserror("%s", e.message().c_str());

			call("read", socket_instance);
			return socket_instance;
		});
	});

	state.acceptor.async_accept(sstate.socket, std::move(accepted));
	return value(result);
}
