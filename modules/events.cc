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

#include <ircd/js/js.h>

namespace ircd  {
namespace js    {

struct events
:trap
{
	struct on;
	struct once;
	struct emit;

	using trap::trap;
}
static events{"events"};

struct events::on
:trap::function
{
	value on_call(object::handle, value::handle, const args &) override;
	using trap::function::function;
}
static on{events, "on"};

struct events::once
:trap::function
{
	value on_call(object::handle, value::handle, const args &) override;
	using trap::function::function;
}
static once{events, "once"};

struct events::emit
:trap::function
{
	value on_call(object::handle, value::handle, const args &) override;
	using trap::function::function;
}
static emit{events, "emit"};

} // namespace js
} // namespace ircd

using namespace ircd::js;

value
events::emit::on_call(object::handle callee,
                      value::handle _that,
                      const args &args)
{
	using js::function;

	object that(_that);
	if(!has(that, "_events"))
		return {};

	object state(get(that, "_events"));
	const value name(args[0]);
	if(!has(state, name))
		return {};

	vector<value> argv(args.size() - 1);
	for(size_t i(0); i < args.size() - 1; ++i)
		argv.insert(argv.begin() + i, args[i+1]);

	size_t i(0);
	object array(get(state, name));
	for(; i < array.size(); ++i)
	{
		if(!has(array, i))
			continue;

		object package(get(array, i));
		function callback(get(package, "function"));
		if(has(package, "once") && bool(get(package, "once")))
			del(array, i);

		call(callback, that, argv);
	}

	array.resize(i);
	return {};
}

value
events::once::on_call(object::handle callee,
                      value::handle _that,
                      const args &args)
{
	object that(_that);
	vector<value> argv
	{
		args[0],
		args[1],
		value(true)
	};

	return call(get(that, "on"), that, argv);
}

value
events::on::on_call(object::handle callee,
                    value::handle _that,
                    const args &args)
{
	using js::function;

	object that(_that);
	const string name(args[0]);
	const function callback(args[1]);

	if(!has(that, "_events"))
		set(that, "_events", object{});

	object state(get(that, "_events"));
	if(!has(state, name))
		set(state, name, object(object::array, 0));

	object package;
	set(package, "function", object(callback));

	if(args.has(2) && bool(args[2]))
		set(package, "once", args[2]);

	object array(get(state, name));
	array.resize(array.size() + 1);
	set(array, array.size() - 1, package);
	return {};
}

ircd::mapi::header IRCD_MODULE
{
	"Asynchronous event prototype interface: include on() and emit() in objects.",
	ircd::mapi::NO_FLAGS
};
