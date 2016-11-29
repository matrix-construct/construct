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

#pragma once
#define HAVE_IRCD_JS_TASK_H

namespace ircd {
namespace js   {

struct task
:std::enable_shared_from_this<task>
{
	uint64_t pid;                                // unique process ID
	uint64_t yid;                                // ID of current yield attempting unification
	std::map<uint64_t, object> complete;         // futures waiting for yield unification
	std::map<uint64_t, object> pending;          // pending futures awaiting results
	std::shared_ptr<task> work;                  // references self when there is unfinished work
	struct global global;                        // global / this / root scope object
	script main;                                 // main generator wrapper script
	struct generator generator;                  // generator state
	bool canceling = false;

  private:
	static uint64_t tasks_next_pid();
	uint64_t tasks_insert();
	bool tasks_remove();

  public:
	bool pending_add(const uint64_t &id, object);
	bool pending_del(const uint64_t &id);

	task(const std::u16string &source);
	task(const std::string &source);
	task(task &&) = delete;
	task(const task &) = delete;
	~task() noexcept;

	static bool enter(task &, const std::function<void (task &)> &closure);
	static bool enter(const std::weak_ptr<task> &, const std::function<void (task &)> &closure);
	static task &get(const object &global);
	static task &get();
};

string decompile(const task &, const bool &pretty = false);
object reflect(const task &);

inline task &
task::get()
{
	return get(current_global());
}

inline task &
task::get(const object &global)
{
	return *reinterpret_cast<task *>(JS_GetPrivate(global));
}

} // namespace js
} // namespace ircd
