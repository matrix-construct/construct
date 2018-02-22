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
#include <RB_INC_SYS_RESOURCE_H
#include "lgetopt.h"
#include "construct.h"

namespace fs = ircd::fs;

static void sigfd_handler(const boost::system::error_code &, int) noexcept;
static bool startup_checks();
static void enable_coredumps();
static void print_version();

const char *const fatalerrstr
{R"(
***
*** A fatal error has occurred. Please contact the developer with the message below.
*** Create a coredump by reproducing the error using the -debug command-line option.
***
%s
)"};

const char *const usererrstr
{R"(
***
*** A fatal startup error has occurred. Please fix the problem to continue. ***
***
%s
)"};

bool printversion;
bool testing_conf;
bool cmdline;
const char *configfile;
const char *execute;
lgetopt opts[] =
{
	{ "help",       nullptr,          lgetopt::USAGE,   "Print this text" },
	{ "version",    &printversion,    lgetopt::BOOL,    "Print version and exit" },
	{ "configfile", &configfile,      lgetopt::STRING,  "File to use for ircd.conf" },
	{ "conftest",   &testing_conf,    lgetopt::YESNO,   "Test the configuration files and exit" },
	{ "debug",      &ircd::debugmode, lgetopt::BOOL,    "Enable options for debugging" },
	{ "console",    &cmdline,         lgetopt::BOOL,    "Drop to a command line immediately after startup" },
	{ "execute",    &execute,         lgetopt::STRING,  "Execute command lines immediately after startup" },
	{ nullptr,      nullptr,          lgetopt::STRING,  nullptr },
};

std::unique_ptr<boost::asio::io_context> ios
{
	// Having trouble with static destruction in clang so this
	// has to become still-reachable
	std::make_unique<boost::asio::io_context>()
};

boost::asio::signal_set sigs
{
	*ios
};

// TODO: XXX: This has to be declared first before any modules
// are loaded otherwise the module will not be unloadable.
boost::asio::ip::tcp::acceptor _dummy_acceptor_ { *ios };
boost::asio::ip::tcp::socket _dummy_sock_ { *ios };
boost::asio::ip::tcp::resolver _dummy_resolver_ { *ios };

int main(int argc, char *const *argv)
try
{
	umask(077);       // better safe than sorry --SRB
	parseargs(&argc, &argv, opts);
	if(!startup_checks())
		return 1;

	// cores are not dumped without consent of the user to maintain the privacy
	// of cryptographic key material in memory at the time of the crash.
	if(RB_DEBUG_LEVEL || ircd::debugmode)
		enable_coredumps();

	if(printversion)
	{
		print_version();
		return 0;
	}

	// Determine the configuration file from either the user's command line
	// argument or fall back to the default.
	const std::string confpath
	{
		configfile?: fs::get(fs::IRCD_CONF)
	};

	// Associates libircd with our io_context and posts the initial routines
	// to that io_context. Execution of IRCd will then occur during ios::run()
	ircd::init(*ios, confpath);

	// libircd does no signal handling (or at least none that you ever have to
	// care about); reaction to all signals happens out here instead. Handling
	// is done properly through the io_context which registers the handler for
	// the platform and then safely posts the received signal to the io_context
	// event loop. This means we lose the true instant hardware-interrupt gratitude
	// of signals but with the benefit of unconditional safety and cross-
	// platformness with windows etc.
	sigs.add(SIGHUP);
	sigs.add(SIGINT);
	sigs.add(SIGTSTP);
	sigs.add(SIGQUIT);
	sigs.add(SIGTERM);
	sigs.async_wait(sigfd_handler);

	// Because we registered signal handlers with the io_context, ios->run()
	// is now shared between those handlers and libircd. This means the run()
	// won't return even if we call ircd::quit(). We use the callback to then
	// cancel the handlers so run() can return and the program can exit.
	ircd::runlevel_changed = [](const enum ircd::runlevel &mode)
	{
		switch(mode)
		{
			case ircd::runlevel::HALT:
			case ircd::runlevel::FAULT:
				sigs.cancel();
				break;

			default:
				break;
		}
	};

	// If the user wants to immediately drop to a command line without having to
	// send a ctrl-c for it, that is provided here.
	if(cmdline)
		console_spawn();

	if(execute)
		console_execute({execute});

	// Execution.
	// Blocks until a clean exit from a quit() or an exception comes out of it.
	ios->run();
}
catch(const ircd::user_error &e)
{
	if(ircd::debugmode)
		throw;

	fprintf(stderr, usererrstr, e.what());
	return EXIT_FAILURE;
}
catch(const std::exception &e)
{
	if(ircd::debugmode)
		throw;

	/*
	* Why EXIT_FAILURE here?
	* Because if ircd_die_cb() is called it's because of a fatal
	* error inside libcharybdis, and we don't know how to handle the
	* exception, so it is logical to return a FAILURE exit code here.
	*    --nenolod
	*
	* left the comment but the code is gone -jzk
	*/
	fprintf(stderr, fatalerrstr, e.what());
	return EXIT_FAILURE;
}

void print_version()
{
	printf("VERSION :%s\n",
	       RB_VERSION);

	#ifdef CUSTOM_BRANDING
	printf("VERSION :based on %s-%s\n",
	       PACKAGE_NAME,
	       PACKAGE_VERSION);
	#endif
}

bool startup_checks()
try
{
	#ifndef _WIN32
	if(geteuid() == 0)
		throw ircd::error("Don't run ircd as root!!!");
	#endif

	fs::chdir(fs::get(fs::PREFIX));
	return true;
}
catch(const std::exception &e)
{
	fprintf(stderr, usererrstr, e.what());
	return false;
}

void
#ifdef HAVE_SYS_RESOURCE_H
enable_coredumps()
try
{
	//
	// Setup corefile size immediately after boot -kre
	//

	rlimit rlim;    // resource limits
	ircd::syscall(getrlimit, RLIMIT_CORE, &rlim);

	// Set corefilesize to maximum
	rlim.rlim_cur = rlim.rlim_max;
	ircd::syscall(setrlimit, RLIMIT_CORE, &rlim);
}
catch(const std::exception &e)
{
	std::cerr << "Failed to adjust rlimit: " << e.what() << std::endl;
}
#else
enable_coredumps()
{
}
#endif

static void handle_quit();
static void handle_interruption();
static void handle_termstop();
static void handle_hangup();

void
sigfd_handler(const boost::system::error_code &ec,
              int signum)
noexcept
{
	switch(ec.value())
	{
		using namespace boost::system::errc;

		case success:
			break;

		case operation_canceled:
			console_cancel();
			return;

		default:
			console_cancel();
			throw std::runtime_error(ec.message());
	}

	switch(signum)
	{
		case SIGINT:   handle_interruption();   break;
		case SIGTSTP:  handle_termstop();       break;
		case SIGHUP:   handle_hangup();         break;
		case SIGQUIT:  handle_quit();           return;
		case SIGTERM:  handle_quit();           return;
		default:                                break;
	}

	sigs.async_wait(sigfd_handler);
}

void
handle_quit()
try
{
	console_cancel();
	ircd::quit();
}
catch(const std::exception &e)
{
	ircd::log::error("SIGQUIT handler: %s", e.what());
}

void
handle_hangup()
try
{
	console_hangup();
}
catch(const std::exception &e)
{
	ircd::log::error("SIGHUP handler: %s", e.what());
}

void
handle_termstop()
try
{
	console_termstop();
}
catch(const std::exception &e)
{
	ircd::log::error("SIGTSTP handler: %s", e.what());
}

void
handle_interruption()
try
{
	console_cancel();
	console_spawn();
}
catch(const std::exception &e)
{
	ircd::log::error("SIGINT handler: %s", e.what());
}
