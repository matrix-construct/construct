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
#include <ircd/bufs.h>

namespace ircd {
namespace js   {

struct socket socket __attribute__((init_priority(1001)));

ircd::js::socket::socket()
:trap
{
	"Socket",         // Uppercase for Node.js compatibility.
	JSCLASS_HAS_PRIVATE
}
{
	parent_prototype = &trap::find("stream");
}

ircd::js::socket::state::state(const std::pair<size_t, size_t> &buffer_size)
:socket{*ios}
,in{buffer_size.first}
,out{buffer_size.second}
{
}

ircd::js::socket::state::~state()
noexcept
{
}

void
ircd::js::socket::on_gc(JSObject *const &that)
try
{
	const scope always_parent([this, &that]
	{
		trap::on_gc(that);
	});

	if(!has(that, priv))
		return;

	auto &state(get<socket::state>(that, priv));
	auto &socket(state.socket);
	boost::system::error_code ec;
	socket.cancel(ec);
}
catch(const std::exception &e)
{
	log.warning("socket::on_gc(%p): %s\n",
	            (const void *)this,
	            e.what());
}

void
ircd::js::socket::on_new(object::handle that,
                         object &obj,
                         const args &args)
{
//	auto &stream(trap::find("stream"));
//	obj.prototype(ctor(stream));

//	set(obj, "connect", connect(obj));
//	set(obj, "close", close(obj));
//	set(obj, "write", write(obj));
//	set(obj, "read", read(obj));

	set(obj, std::make_shared<socket::state>());
}

struct socket::close
:trap::function
{
	trap &future{trap::find("future")};

	value on_call(object::handle obj, value::handle, const args &args) override;
	using trap::function::function;
}
static close{socket, "close"};

ircd::js::value
ircd::js::socket::close::on_call(object::handle obj,
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

	auto &state(get<socket::state>(that, priv));
	auto &socket(state.socket);
	result([&socket]
	{
		boost::system::error_code ec;
		socket.shutdown(ip::tcp::socket::shutdown_send, ec);
		socket.shutdown(ip::tcp::socket::shutdown_receive, ec);
		jserror e("%s", ec.message().c_str());
		return e.val.get();
	});

	return result;
}

struct socket::read
:trap::function
{
	trap &future{trap::find("future")};

	value on_call(object::handle obj, value::handle, const args &args) override;
	using trap::function::function;
}
static read{socket, "read"};

ircd::js::value
ircd::js::socket::read::on_call(object::handle obj,
                                value::handle _that,
                                const args &args)
{
	object that(_that);
	auto &state(get<socket::state>(that, priv));
	auto &socket(state.socket);

	object emission;
	set(emission, "event", "data");
	set(emission, "emitter", that);

	contract result(ctor(future));
	set(result.future, "emit", emission);
	set(result.future, "cancel", get(that, "close"));

	const auto finisher([result, that(heap_object(that)), state(shared_from(state))]
	(const boost::system::error_code &e, const size_t bytes)
	mutable -> void
	{
		result([&]() -> value
		{
			if(e)
				throw jserror("%s", e.message().c_str());

			if(!bytes)
				throw jserror("empty message");

			string ret(buffer_cast<const char *>(state->in.data()), bytes);
			state->in.consume(bytes);
			call("read", that);
			return ret;
		});
	});

	const auto condition([&state]
	(const boost::system::error_code &e, const size_t bytes)
	-> size_t
	{
		if(e)
			return 0;

		if(bytes > 0)
			return 0;

		return state.in.max_size() - state.in.size();
	});

	boost::asio::async_read(socket, state.in, condition, finisher);
	return result;
}

struct socket::write
:trap::function
{
	trap &future{trap::find("future")};

	value on_call(object::handle, value::handle, const args &args) override;
	using trap::function::function;
}
static write{socket, "write"};

ircd::js::value
ircd::js::socket::write::on_call(object::handle obj,
                                 value::handle _that,
                                 const args &args)
{
	const object that(_that);
	auto &state(get<socket::state>(that, priv));
	auto &socket(state.socket);

	const string data(args[0]);
	auto buffer(state.out.prepare(size(data)));
	copy(const_buffer(data.c_str(), data.size()), buffer);
	state.out.commit(size(data));

	object emission;
	set(emission, "event", "drain");
	set(emission, "emitter", that);

	contract result(ctor(future));
	set(result.future, "emit", emission);
	set(result.future, "cancel", get(that, "close"));

	boost::asio::async_write(socket, state.out, [result, state(shared_from(state))]
	(const boost::system::system_error &e, const size_t &bytes)
	mutable
	{
		result([&]() -> value
		{
			if(e.code())
				throw jserror("%s", e.what());

			return bytes;
		});
	});

	return result;
}

struct socket::connect
:trap::function
{
	trap &future{trap::find("future")};

	value on_call(object::handle obj, value::handle, const args &args) override;
	using trap::function::function;
}
static connect{socket, "connect"};

ircd::js::value
ircd::js::socket::connect::on_call(object::handle obj,
                                   value::handle _that,
                                   const args &args)
{
	const object that(_that);
	const object options(args[0]);
	const std::string host(get(options, "host"));
	const uint16_t port(get(options, "port"));
	ip::tcp::endpoint ep(ip::address::from_string(host), port);

	object emission;
	set(emission, "event", "connect");
	set(emission, "emitter", that);

	contract result(ctor(future));
	set(result.future, "emit", emission);
	set(result.future, "cancel", get(that, "close"));

	auto &state(get<socket::state>(that, priv));
	state.socket.async_connect(ep, [result, that(heap_object(that)), state(shared_from(state))]
	(const boost::system::system_error &e)
	mutable
	{
		result([&e, &that]() -> value
		{
			if(e.code())
				throw jserror("%s", e.what());

			call("read", that);
			return true;
		});
	});

	return result;
}

} // namespace js
} // namespace ircd
