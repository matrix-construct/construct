// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_JS_TASK_H

namespace ircd {
namespace js   {

struct task
:std::enable_shared_from_this<task>
{
	uint64_t pid;                                // unique process ID
	struct global global;                        // global / this / root scope object
	struct module main;                          // main module script

  public:
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
