// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/ircd.h>
#include <ircd/asio.h>
#include "construct.h"
#include "signals.h"
#include "console.h"

namespace construct
{
	namespace ph = std::placeholders;

	static void handle_cont();
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
	signal_set->add(SIGCONT);
	set_handle();
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
			signal_set->cancel();
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
	auto handler
	{
		std::bind(&signals::on_signal, this, ph::_1, ph::_2)
	};

	signal_set->async_wait(std::move(handler));
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
	if(ircd::run::level != ircd::run::level::RUN)
	{
		ircd::quit();
		return;
	}

	if(!console::active())
		console::spawn();
	else
		console::interrupt();
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

	// This signal handler (though not a *real* signal handler) is still
	// running on the main async stack and not an ircd::ctx. The reload
	// function does a lot of IO so it requires an ircd::ctx.
	ircd::context{[]
	{
		ircd::mods::import<void ()> reload_conf
		{
			"s_conf", "reload_conf"
		};

		reload_conf();
	}};
}
catch(const std::exception &e)
{
	ircd::log::error
	{
		"SIGUSR1 handler: %s", e.what()
	};
}

void
construct::handle_cont()
try
{
	ircd::cont();
}
catch(const std::exception &e)
{
	ircd::log::error
	{
		"SIGCONT handler: %s", e.what()
	};
}
