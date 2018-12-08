// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd
{
	static enum runlevel _runlevel;
}

decltype(ircd::runlevel)
ircd::runlevel
{
	_runlevel
};

//
// runlevel_changed
//

template<>
decltype(ircd::runlevel_changed::list)
ircd::util::instance_list<ircd::runlevel_changed>::list
{};

decltype(ircd::runlevel_changed::dock)
ircd::runlevel_changed::dock;

//
// runlevel_changed::runlevel_changed
//

ircd::runlevel_changed::runlevel_changed(handler function)
:handler
{
	std::move(function)
}
{
}

ircd::runlevel_changed::~runlevel_changed()
noexcept
{
}

//
// util
//

/// Sets the runlevel of IRCd and notifies users. This should never be called
/// manually/directly, as it doesn't trigger a runlevel change itself, it just
/// notifies of one.
///
/// The notification will be posted to the io_context. This is important to
/// prevent the callback from continuing execution on some ircd::ctx stack and
/// instead invoke their function on the main stack in their own io_context
/// event slice.
bool
ircd::runlevel_set(const enum runlevel &new_runlevel)
try
{
	if(runlevel == new_runlevel)
		return false;

	log::debug
	{
		"IRCd runlevel transition from '%s' to '%s' (notifying %zu)",
		reflect(runlevel),
		reflect(new_runlevel),
		runlevel_changed::list.size()
	};

	_runlevel = new_runlevel;
	runlevel_changed::dock.notify_all();

	// This latch is used to block this call when setting the runlevel
	// from an ircd::ctx. If the runlevel is set from the main stack then
	// the caller will have to do synchronization themselves.
	ctx::latch latch
	{
		bool(ctx::current) // latch has count of 1 if we're on an ircd::ctx
	};

	// This function will notify the user of the change to IRCd. When there
	// are listeners, function is posted to the io_context ensuring THERE IS
	// NO CONTINUATION ON THIS STACK by the user.
	const auto call_users{[new_runlevel, &latch, latching(!latch.is_ready())]
	{
		assert(new_runlevel == ircd::runlevel);

		log::notice
		{
			"IRCd %s", reflect(new_runlevel)
		};

		if(new_runlevel == runlevel::HALT)
			log::fini();
		else
			log::flush();

		for(const auto &handler : runlevel_changed::list)
			(*handler)(new_runlevel);

		if(latching)
			latch.count_down();
	}};

	if(runlevel_changed::list.size())
		ircd::post(call_users);
	else
		call_users();

	if(ctx::current)
		latch.wait();

	return true;
}
catch(const std::exception &e)
{
	log::critical
	{
		"IRCd runlevel change to '%s': %s",
		reflect(new_runlevel),
		e.what()
	};

	ircd::terminate();
	return false;
}

ircd::string_view
ircd::reflect(const enum runlevel &level)
{
	switch(level)
	{
		case runlevel::HALT:      return "HALT";
		case runlevel::READY:     return "READY";
		case runlevel::START:     return "START";
		case runlevel::RUN:       return "RUN";
		case runlevel::QUIT:      return "QUIT";
		case runlevel::FAULT:     return "FAULT";
	}

	return "??????";
}
