// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/matrix.h>
#include <ircd/asio.h>
#include "construct.h"
#include "homeserver.h"
#include "signals.h"
#include "console.h"

namespace construct
{
	namespace ph = std::placeholders;

	static void handle_cont();
	static void handle_usr2();
	static void handle_usr1();
	static void handle_quit();
	static void handle_interrupt();
	static void handle_hangup();
	static void handle_signal(const int &);
}

construct::signals::signals(boost::asio::io_context &ios)
:signal_set
{
	std::make_unique<boost::asio::signal_set>(ios)
}
,runlevel_changed
{
	std::bind(&signals::on_runlevel, this, ph::_1)
}
{
	signal_set->add(SIGHUP);
	signal_set->add(SIGINT);
	signal_set->add(SIGQUIT);
	signal_set->add(SIGTERM);
	signal_set->add(SIGUSR1);
	signal_set->add(SIGUSR2);
	signal_set->add(SIGCONT);
	set_handle();
}

construct::signals::~signals()
noexcept
{
}

// Because we registered signal handlers with the io_context, ios->run()
// is now shared between those handlers and libircd. This means the run()
// won't return even if we call ircd::quit(). We use this callback to
// cancel the signal handlers so run() can return and the program can exit.
void
construct::signals::on_runlevel(const enum ircd::run::level &level)
{
	switch(level)
	{
		case ircd::run::level::HALT:
			signal_set.reset(nullptr);
			break;

		default:
			break;
	}
}

void
construct::signals::on_signal(const boost::system::error_code &ec,
                              int signum)
noexcept
{
	assert(!ec || ec.category() == boost::system::system_category());

	switch(ec.value())
	{
		// Signal received
		case boost::system::errc::success:
			break;

		// Shutdown
		case boost::system::errc::operation_canceled:
			return;

		// Not expected
		default:
			ircd::throw_system_error(ec);
			__builtin_unreachable();
	}

	handle_signal(signum);

	switch(ircd::run::level)
	{
		// Reinstall handler for next signal
		default:
			set_handle();
			break;

		// No reinstall of handler.
		case ircd::run::level::HALT:
			break;
	}
}

void
construct::signals::set_handle()
{
	static ircd::ios::descriptor desc
	{
		"construct.signals"
	};

	auto handler
	{
		std::bind(&signals::on_signal, this, ph::_1, ph::_2)
	};

	signal_set->async_wait(ircd::ios::handle(desc, std::move(handler)));
}

void
construct::handle_signal(const int &signum)
{
	switch(signum)
	{
		case SIGHUP:   return handle_hangup();
		case SIGINT:   return handle_interrupt();
		case SIGQUIT:  return handle_quit();
		case SIGTERM:  return handle_quit();
		case SIGUSR1:  return handle_usr1();
		case SIGUSR2:  return handle_usr2();
		case SIGCONT:  return handle_cont();
		default:       break;
	}

	ircd::log::error
	{
		"Caught unhandled signal %d", signum
	};
}

void
construct::handle_hangup()
try
{
	static bool console_disabled;
	console_disabled =! console_disabled;

	if(console_disabled)
		ircd::log::console_disable();
	else
		ircd::log::console_enable();

}
catch(const std::exception &e)
{
	ircd::log::error
	{
		"SIGHUP handler: %s", e.what()
	};
}

void
construct::handle_interrupt()
try
{
	// The console owns the keyboard and ctrl-c whenever active.
	if(console::active())
	{
		console::interrupt();
		return;
	}

	// Interrupt/ctrl-c opens the console
	if(ircd::run::level == ircd::run::level::RUN)
	{
		console::spawn();
		return;
	}

	// Interrupt/ctrl-c can be used to initiate a clean shutdown from any
	// point in any transitional runlevel.
	ircd::quit();
}
catch(const std::exception &e)
{
	ircd::log::error
	{
		"SIGINT handler: %s", e.what()
	};
}

void
construct::handle_quit()
try
{
	ircd::quit();
}
catch(const std::exception &e)
{
	ircd::log::error
	{
		"SIGQUIT handler: %s", e.what()
	};
}

void
construct::handle_usr1()
try
{
	// Spawning the context that follows this branch and doing a rehash
	// when not in a stable state like run::level::RUN will just make a mess
	// so any signal received is just dropped and the user can try again.
	if(ircd::run::level != ircd::run::level::RUN)
	{
		ircd::log::warning
		{
			"Not rehashing conf from SIGUSR1 in runlevel %s",
			reflect(ircd::run::level)
		};

		return;
	}

	if(!homeserver::primary || !homeserver::primary->module[0])
		return;

	// This signal handler (though not a *real* signal handler) is still
	// running on the main async stack and not an ircd::ctx. The reload
	// function does a lot of IO so it requires an ircd::ctx.
	ircd::context{[]
	{
		static ircd::mods::import<void (ircd::m::homeserver *)> rehash
		{
			homeserver::primary->module[0], "ircd::m::homeserver::rehash"
		};

		assert(homeserver::primary->hs);
		rehash(homeserver::primary->hs.get());
	}};
}
catch(const std::exception &e)
{
	ircd::log::error
	{
		"SIGUSR1 handler :%s",
		e.what()
	};
}

void
construct::handle_usr2()
try
{
	if(ircd::run::level != ircd::run::level::RUN)
	{
		ircd::log::warning
		{
			"Not synchronizing database from SIGUSR2 in runlevel %s",
			reflect(ircd::run::level)
		};

		return;
	}

	if(!ircd::slave)
		return;

	if(!homeserver::primary || !homeserver::primary->module[0])
		return;

	ircd::context{[]
	{
		static ircd::mods::import<bool (ircd::m::homeserver *)> refresh
		{
			homeserver::primary->module[0], "ircd::m::homeserver::refresh"
		};

		assert(homeserver::primary->hs);
		const bool refreshed
		{
			refresh(homeserver::primary->hs.get())
		};
	}};
}
catch(const std::exception &e)
{
	ircd::log::error
	{
		"SIGUSR2 handler :%s",
		e.what()
	};
}

void
construct::handle_cont()
try
{
	ircd::ios::continuing();
}
catch(const std::exception &e)
{
	ircd::log::error
	{
		"SIGCONT handler: %s", e.what()
	};
}
