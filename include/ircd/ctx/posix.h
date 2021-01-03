// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::ctx::posix
{
	struct enable_pthread;
	struct disable_pthread;

	extern log::log log;
	extern bool hook_pthread_create;
	extern bool unhook_pthread_create;
	extern std::vector<context> ctxs;
}

/// Instances of this object force all pthread_create() to create real
/// pthreads. By default that decision is internally automated. The
/// assertion made by this object takes precedence over instances of
/// disable_pthread.
struct ircd::ctx::posix::enable_pthread
{
	bool theirs
	{
		unhook_pthread_create
	};

	enable_pthread(const bool &ours = true)
	{
		unhook_pthread_create = ours;
	}

	~enable_pthread() noexcept
	{
		unhook_pthread_create = theirs;
	}
};

/// Instances of this object force all pthread_create() to create ircd::context
/// userspace threads rather than real pthreads(). By default this is
/// determined internally, but instances of this object will force that behavior
/// in all cases except when instances of enable_pthread exist, which takes
/// precedence.
struct ircd::ctx::posix::disable_pthread
{
	bool theirs
	{
		hook_pthread_create
	};

	disable_pthread(const bool &ours = true)
	{
		hook_pthread_create = ours;
	}

	~disable_pthread()
	{
		hook_pthread_create = theirs;
	}
};
