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

struct assertions
{
	assertions();
}
static assertions;

namespace ircd  {
namespace js    {

struct kernel
:trap
{
	JSPrincipals *principals;
	std::unique_ptr<struct star> star;
	std::shared_ptr<task> process;

	value on_call(object::handle, value::handle, const args &) override;
	value on_set(object::handle, id::handle, value::handle) override;
	value on_get(object::handle, id::handle, value::handle) override;
	void on_add(object::handle, id::handle, value::handle) override;
	bool on_del(object::handle, id::handle) override;
	bool on_has(object::handle, id::handle) override;
	void on_enu(object::handle) override;
	void on_new(object::handle, object &, const args &) override;
	void on_gc(JSObject *const &) override;

	void handle_contract_emit(task &, const uint64_t &id, object &);
	void handle_contract_callback(task &, const uint64_t &id, object &);
	void handle_contract_yield(task &, const uint64_t &id, object &);
	void handle_contract(task &, object &);
	void handle(contract &);

	ctx::dock termination;
	void terminate_tasks();
	void wait_terminate();

	void main() noexcept;
	ircd::context kctx;

	kernel();
	~kernel() noexcept;
}
static kernel;

} // namespace js
} // namespace ircd

using namespace ircd::js;
using namespace ircd;

kernel::kernel()
:trap
{
	"",
	JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_PRIVATE
}
,principals
{
	nullptr
}
,star{[]
{
	auto ret(std::make_unique<struct star>());
	cx->star = ret.get();
	return ret;
}()}
,kctx
{
	8_MiB, std::bind(&kernel::main, this), ctx::DEFER_POST
}
{
}

kernel::~kernel()
noexcept
{
	terminate_tasks();
	wait_terminate();
}

void
kernel::main()
noexcept try
{
	do
	{
		{
			// Wait for a completion event from one of the modules
			auto contract(std::move(star->completion.pop()));
			handle(contract);
		}

		//run_gc(*rt);
	}
	while(!star->tasks.empty());

	log.info("Kernel finished");
	termination.notify_all();
}
catch(const ircd::ctx::interrupted &e)
{
	log.debug("Kernel interrupted");
	termination.notify_all();
	return;
}
catch(const std::exception &e)
{
	log.critical("Kernel PANIC: %s", e.what());
	std::terminate();
}

void
kernel::wait_terminate()
{
	// Wait for userspace and kernel termination
	termination.wait([this]
	{
		return star->tasks.empty();
	});
}

void
kernel::terminate_tasks()
{
	std::vector<task *> tasks(cx->star->tasks.size());
	std::transform(begin(cx->star->tasks), end(cx->star->tasks), begin(tasks), []
	(const auto &pair)
	{
		return pair.second;
	});

	for(auto &task : tasks)
	{
		for(auto &pair : task->pending)
		{
			task::enter(*task, [&pair]
			(struct task &task)
			{
				auto &future(pair.second);
				if(!has(future, "cancel"))
					return;

				auto can(get(future, "cancel"));
				call(can, get(future, "emit.emitter"));
				del(future, "cancel");
			});
		}
	}
}

void
kernel::handle(contract &contract)
{
	// The contract itself is only a weak reference which does not keep the task alive.
	// The task may have been killed while this contract was queued, and if that is the
	// case this closure will not be entered. If this closure is entered the necessary
	// environment for this task will be re-established. see: `task::enter()`
	task::enter(contract, [this, &contract]
	(task &task)
	{
		object future(contract.future);
		handle_contract(task, future);
	});
}

void
kernel::handle_contract(task &task,
                        object &future)
try
{
	if(!has(future, "id"))
	{
		log.warning("no ID with future");
		return;
	}

	// This ID is no longer considered pending: if no more pending contracts exist after this
	// and none are created, every future yield will have non-blocking reunification.
	const uint64_t id(get(future, "id"));
	task.pending_del(id);

	const bool callback(has(future, "callback"));
	const bool emit(has(future, "emit"));

	if(callback)
		handle_contract_callback(task, id, future);

	if(emit)
		handle_contract_emit(task, id, future);

	if(task.yid)
		handle_contract_yield(task, id, future);
}
catch(const std::exception &e)
{
	log.error("task[%lu]: %s", task.pid, e.what());
}

void
kernel::handle_contract_yield(task &task,
                              const uint64_t &id,
                              object &future)
{
	// If this contract does not have the ID the task is yielding on, the result must be
	// deferred until it can be reunified with the proper yield.
	if(id != task.yid && task.yid)
	{
		// The deference involves mapping the id to the state object to be retrieved for
		// reunification at a future yield.
		task.complete.emplace(id, heap_object(future));
		return;
	}

	// When 'error' is set the result is an error to be thrown from the yield.
	if(has(future, "error"))
		task.generator._throw(get(future, "error"));
	else
		task.generator.next(get(future, "value"));

	// If the generator indicates a finished condition the task is not killed here, that will
	// happen automatically; there may still be callback contracts or other matters pending.
	// We just back off this stack now.
	if(task.generator.done())
		return;

	// The yield package must indicate an ID. The user can tamper with this, but if the ID
	// is removed and this exception propagates the task risks termination.
	if(!has(task.generator.last, "value.id"))
		throw error("Invalid yield package: contract without ID");

	// The task was reentered and now the new yield id is extracted from the package.
	task.yid = uint64_t(get(task.generator.last, "value.id"));
	const auto it(task.complete.find(task.yid));
	if(it == end(task.complete))
		return;

	// The latest yield identified a previously completed but deferred contract; this means
	// we can conduct reunification immediately. This is done recursively until the yielded
	// ID is not yet complete.
	future = std::move(it->second);
	task.complete.erase(it);
	handle_contract_yield(task, task.yid, future);
}

void
kernel::handle_contract_callback(task &task,
                                 const uint64_t &id,
                                 object &future)
{
	using js::function;

	function callback(get(future, "callback"));

	// When 'error' is set the result is an error.
	if(has(future, "error"))
	{
		// Keeping with Node.js callback-convention, the first argument to the callback is the
		// error object. The callback may have more arguments but they are left undefined.
		callback(task.global, get(future, "error"));
		return;
	}

	// The result is valid and contained in 'value'.
	value result(get(future, "value"));

	// Keeping with Node.js callback-convention, the first argument to the callback is the
	// error object, which is empty here.
	callback(task.global, value{}, result);
}

void
kernel::handle_contract_emit(task &task,
                             const uint64_t &id,
                             object &future)
{
	using js::function;

	object emission(get(future, "emit"));
	object emitter(get(emission, "emitter"));
	string event(get(emission, "event"));
	function emit(get(emitter, "emit"));

	// When 'error' is set the result is an error.
	if(has(future, "error"))
	{
		// Keeping with Node.js callback-convention, the first argument to the callback is the
		// error object. The callback may have more arguments but they are left undefined.
		emit(emitter, "error", get(future, "error"));
		return;
	}

	value val(has(future, "value")? get(future, "value") : value{});
	emit(emitter, event, val);
}

void
kernel::on_gc(JSObject *const &)
{
}

void
kernel::on_new(object::handle,
               object &obj,
               const args &args)
{
}

void
kernel::on_enu(object::handle obj)
{
	if(!JS_EnumerateStandardClasses(*cx, obj))
		throw internal_error("Failed to enumerate standard classes");
}

bool
kernel::on_has(object::handle obj,
               id::handle id)
{
	return trap::on_has(obj, id);
}

bool
kernel::on_del(object::handle obj,
               id::handle id)
{
	return true;
}

void
kernel::on_add(object::handle obj,
               id::handle id,
               value::handle val)
{
}

ircd::js::value
kernel::on_get(object::handle obj,
               id::handle id,
               value::handle val)
{
	return trap::on_get(obj, id, val);
}

ircd::js::value
kernel::on_set(object::handle obj,
               id::handle id,
               value::handle val)
{
	return val;
}

ircd::js::value
kernel::on_call(object::handle obj,
                value::handle,
                const args &args)
{
	return {};
}


const std::string test
{R"(

	var xx = 0;

	(function *()
	{
		var zz = 0;

		require('assert');
		require('console');
		require('events');
		require('stream');
		require('dns');
		require('net');

		var s = new net.Server();

		assert(true);

		s.on('close', (err) =>
		{
			console.log("listen closed: " + err);
		});

		s.on('error', (what) =>
		{
			console.error("listen error: " + what);
		});

		s.on('connection', (socket) =>
		{
			var yy = xx;
			console.log("connection " + yy);
		
			var hits = 0;
			
			socket.on('error', (what) =>
			{
				console.error("socket error: " + what + " " + hits);
			});

			socket.on('close', (what) =>
			{
				console.log("socket close: " + what + " " + hits);
			});

			socket.on('data', (buffer) =>
			{
				buffer = buffer.split("\n")[0];
				console.log("got: " + buffer);

				if(buffer == "QUIT")
				{
					xx += 1;
					socket.close();
					return;
				}

				var gg = hits;
				var somestring = "some string";
				socket.write("ECHO: " + buffer + " " + gg + "\r\n");
		
				if(hits++ > 5)
					socket.close();
			});

			socket.on('drain', () =>
			{
				console.log("sent " + hits);
			});
		});

		s.on('listening', () =>
		{
			console.log("listen listening " + zz);
		});

		var opts =
		{
			"port": 6667,
			"host": "127.0.0.1",
		};

		while(xx < 2)
		{
			yield s.listen(opts, () =>
			{
				console.log("listen callback " + zz + " " + xx);
			});
		}

	}());

)"s};

mapi::init init_test{[]
{
	using namespace ircd::js;

	try
	{
		mods::load("assert");
		mods::load("future");
		mods::load("require");
		mods::load("events");
		mods::load("console");
		mods::load("stream");
		mods::load("dns");
		mods::load("net");
		//mods::load("crypto");

		std::lock_guard<decltype(*cx)> lock{*cx};
		auto process = std::make_shared<task>(test);

		task::enter(process, []
		(task &process)
		{
			{
				auto &child(trap::find("require"));
				set(process.global, "require", ctor(child));
			}

			value futo(process.generator.next());
			if(!process.generator.done() && has(object(futo), "id"))
				process.yid = uint64_t(get(object(futo), "id"));
		});
	}
	catch(const std::exception &e)
	{
		ircd::js::log.error("test: %s", e.what());
	}
}};

mapi::fini on_unload{[]
{
	using namespace ircd::js;

	kernel.terminate_tasks();
	kernel.process.reset();
	kernel.wait_terminate();

	//mods::unload("crypto");
	mods::unload("net");
	mods::unload("dns");
	mods::unload("stream");
	mods::unload("console");
	mods::unload("events");
	mods::unload("future");
	mods::unload("require");
	mods::unload("assert");
}};

mapi::header IRCD_MODULE
{
	"IRCd.js kernel - program which helps programs run",
	mapi::NO_FLAGS,
	&init_test,
	&on_unload,
};

assertions::assertions()
{
	using ircd::js::error;
	using namespace ircd::js;

	if(!rt)
		throw error("Kernel cannot find required JS runtime instance on this thread.");

	if(!cx)
		throw error("Kernel cannot find required JS context instance on this thread.");
}
