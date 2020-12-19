// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

// internal interface to ircd::run (see ircd/run.h)
namespace ircd::run
{
	static enum level _level, _chadburn;

	// change the current runlevel (private use by ircd.cc only right now)
	bool set(const enum level &);
}

decltype(ircd::run::level)
ircd::run::level
{
	_level
};

decltype(ircd::run::chadburn)
ircd::run::chadburn
{
	_chadburn
};

//
// run::changed
//

template<>
decltype(ircd::run::changed::allocator)
ircd::util::instance_list<ircd::run::changed>::allocator
{};

template<>
decltype(ircd::run::changed::list)
ircd::util::instance_list<ircd::run::changed>::list
{
	allocator
};

decltype(ircd::run::changed::dock)
ircd::run::changed::dock;

//
// run::changed::changed
//

// Out-of-line placement
ircd::run::changed::changed()
noexcept
{
}

// Out-of-line placement
ircd::run::changed::~changed()
noexcept
{
}

void
ircd::run::changed::wait(const std::initializer_list<enum level> &levels)
{
	changed::dock.wait([&levels]
	{
		return std::any_of(begin(levels), end(levels), []
		(const auto &level)
		{
			return run::level == level;
		});
	});
}

//
// ircd::run
//

/// The notification will be posted to the io_context. This is important to
/// prevent the callback from continuing execution on some ircd::ctx stack and
/// instead invoke their function on the main stack in their own io_context
/// event slice.
bool
ircd::run::set(const enum level &new_level)
try
{
	// ignore any redundant calls during and after a transition.
	if(new_level == chadburn)
		return false;

	// When called during a transition already in progress, the behavior
	// is to wait for the transition to complete; if the caller is
	// asynchronous they cannot make this call.
	if(!ctx::current && level != chadburn)
		throw panic
		{
			"Transition '%s' -> '%s' already in progress; cannot wait without ctx",
			reflect(chadburn),
			reflect(level),
		};

	// Wait for any pending runlevel transition to complete before
	// continuing with another transition.
	if(ctx::current)
		changed::dock.wait([]() noexcept
		{
			return level == chadburn;
		});

	// Ignore any redundant calls which made it this far.
	if(level == new_level)
		return false;

	// Indicate the new runlevel here; the transition starts here, and ends
	// when chatburn=level, set unconditionally on unwind.
	_level = new_level;
	const unwind chadburn_{[]
	{
		_chadburn = level;
	}};

	log::notice
	{
		log::star, "IRCd %s",
		reflect(level)
	};

	log::flush();
	changed::dock.notify_all();
	for(const auto &handler : changed::list) try
	{
		handler->handler(level);
	}
	catch(const std::exception &e)
	{
		switch(level)
		{
			case level::HALT:   break;
			case level::QUIT:   break;
			case level::START:  throw;
			default:            throw;
		}

		log::error
		{
			"Runlevel transition to '%s' handler(%p) :%s",
			reflect(level),
			handler,
			e.what(),
		};

		continue;
	}

	if(level == level::HALT)
		return true;

	log::logf
	{
		log::general, log::level::DEBUG,
		"IRCd level transition from '%s' to '%s' (dock:%zu callbacks:%zu)",
		reflect(chadburn),
		reflect(level),
		changed::dock.size(),
		changed::list.size(),
	};

	return true;
}
catch(const std::exception &e)
{
	switch(new_level)
	{
		case level::START:  throw;
		default:            break;
	}

	log::critical
	{
		log::star, "IRCd level change to '%s' :%s",
		reflect(new_level),
		e.what()
	};

	return false;
}

ircd::string_view
ircd::run::reflect(const enum run::level &level)
{
	switch(level)
	{
		case level::HALT:      return "HALT";
		case level::READY:     return "READY";
		case level::START:     return "START";
		case level::RUN:       return "RUN";
		case level::QUIT:      return "QUIT";
		case level::FAULT:     return "FAULT";
	}

	return "??????";
}
