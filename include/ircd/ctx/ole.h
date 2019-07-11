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
#define HAVE_IRCD_CTX_OLE_H

/// Context OffLoad Engine (OLE).
///
/// This system allows moving a task off the main IRCd thread by passing a function to a
/// worker thread for execution. The context on the main IRCd thread yields until the offload
/// function has returned (or thrown).
///
namespace ircd::ctx::ole
{
	struct init;
	struct opts;
	struct control;
	using closure = std::function<void ()>;

	void offload(const opts &, const closure &);
}

namespace ircd::ctx
{
	using ole::offload;
}

struct ircd::ctx::ole::opts
{
	/// Optionally give this offload task a name for any tasklist.
	string_view name;

	/// The function will be executed on each thread.
	size_t concurrency {1};

	/// Queuing priority; in the form of a nice value.
	int8_t prio {0};
};

struct ircd::ctx::ole::init
{
	init();
	~init() noexcept;
};
