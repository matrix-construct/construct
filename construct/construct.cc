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
#include <RB_INC_SYS_RESOURCE_H
#include <ircd/asio.h>
#include "lgetopt.h"
#include "construct.h"

static void sigfd_handler(const boost::system::error_code &, int) noexcept;
static bool startup_checks();
static void enable_coredumps();
static int print_version();

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
bool cmdline;
bool quietmode;
const char *configfile;
const char *execute;
lgetopt opts[] =
{
	{ "help",       nullptr,          lgetopt::USAGE,   "Print this text" },
	{ "version",    &printversion,    lgetopt::BOOL,    "Print version and exit" },
	{ "configfile", &configfile,      lgetopt::STRING,  "File to use for ircd.conf" },
	{ "debug",      &ircd::debugmode, lgetopt::BOOL,    "Enable options for debugging" },
	{ "quiet",      &quietmode,       lgetopt::BOOL,    "Suppress log messages at the terminal." },
	{ "console",    &cmdline,         lgetopt::BOOL,    "Drop to a command line immediately after startup" },
	{ "execute",    &execute,         lgetopt::STRING,  "Execute command lines immediately after startup" },
	{ "nolisten",   &ircd::nolisten,  lgetopt::BOOL,    "Normal execution but without listening sockets" },
	{ "noautomod",  &ircd::noautomod, lgetopt::BOOL,    "Normal execution but without autoloading modules" },
	{ "checkdb",    &ircd::checkdb,   lgetopt::BOOL,    "Perform complete checks of databases when opening" },
	{ "pitrecdb",   &ircd::pitrecdb,  lgetopt::BOOL,    "Allow Point-In-Time-Recover if DB reports corruption after crash" },
	{ "nojs",       &ircd::nojs,      lgetopt::BOOL,    "Disable SpiderMonkey JS subsystem from initializing. (noop when not available)." },
	{ "nodirect",   &ircd::nodirect,  lgetopt::BOOL,    "Disable direct IO (O_DIRECT) for unsupporting filesystems." },
	{ "noaio",      &ircd::noaio,     lgetopt::BOOL,    "Disable the AIO interface in favor of traditional syscalls. " },
	{ nullptr,      nullptr,          lgetopt::STRING,  nullptr },
};

std::unique_ptr<boost::asio::io_context> ios
{
	// Having trouble with static dtor in clang so this has tp be dynamic
	std::make_unique<boost::asio::io_context>()
};

boost::asio::signal_set sigs
{
	*ios
};

int main(int argc, char *const *argv)
try
{
	umask(077);       // better safe than sorry --SRB

	// '-' switched arguments come first; this function incs argv and decs argc
	parseargs(&argc, &argv, opts);

	// cores are not dumped without consent of the user to maintain the privacy
	// of cryptographic key material in memory at the time of the crash.
	if(RB_DEBUG_LEVEL || ircd::debugmode)
		enable_coredumps();

	if(!startup_checks())
		return EXIT_FAILURE;

	if(printversion)
		return print_version();

	if(quietmode)
		ircd::log::console_disable();

	// The matrix origin is the first positional argument after any switched
	// arguments. The matrix origin is the hostpart of MXID's for the server.
	const ircd::string_view origin
	{
		argc > 0?
			argv[0]:
			nullptr
	};

	// The hostname is the unique name for this specific server. This is
	// generally the same as origin; but if origin is example.org with an
	// SRV record redirecting to matrix.example.org then hostname is
	// matrix.example.org. In clusters serving a single origin, all
	// hostnames must be different.
	const ircd::string_view hostname
	{
		argc > 1?     // hostname given on command line
			argv[1]:
		argc > 0?     // hostname matches origin
			argv[0]:
			nullptr
	};

	// at least one hostname argument is required for now.
	if(!hostname)
		throw ircd::user_error
		{
			"Must specify the origin after any switched parameters."
		};

	// Associates libircd with our io_context and posts the initial routines
	// to that io_context. Execution of IRCd will then occur during ios::run()
	// note: only supports service for one hostname at this time.
	ircd::init(*ios, origin, hostname);

	// libircd does no signal handling (or at least none that you ever have to
	// care about); reaction to all signals happens out here instead. Handling
	// is done properly through the io_context which registers the handler for
	// the platform and then safely posts the received signal to the io_context
	// event loop. This means we lose the true instant hardware-interrupt gratitude
	// of signals but with the benefit of unconditional safety and cross-
	// platformness with windows etc.
	sigs.add(SIGHUP);
	sigs.add(SIGINT);
	sigs.add(SIGQUIT);
	sigs.add(SIGTERM);
	sigs.add(SIGUSR1);
	sigs.async_wait(sigfd_handler);

	// Because we registered signal handlers with the io_context, ios->run()
	// is now shared between those handlers and libircd. This means the run()
	// won't return even if we call ircd::quit(). We use the callback to then
	// cancel the handlers so run() can return and the program can exit.
	const ircd::runlevel_changed handler{[]
	(const auto &runlevel)
	{
		switch(runlevel)
		{
			case ircd::runlevel::HALT:
			case ircd::runlevel::FAULT:
				sigs.cancel();
				break;

			default:
				break;
		}
	}};

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

int
print_version()
{
	printf("VERSION :%s\n",
	       RB_VERSION);

	#ifdef CUSTOM_BRANDING
	printf("VERSION :based on %s-%s\n",
	       PACKAGE_NAME,
	       PACKAGE_VERSION);
	#endif

	return EXIT_SUCCESS;
}

bool
startup_checks()
try
{
	namespace fs = ircd::fs;

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
static void handle_hangup();
static void handle_usr1();
static void handle(const int &signum);

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

	handle(signum);

	switch(ircd::runlevel)
	{
		case ircd::runlevel::QUIT:
		case ircd::runlevel::FAULT:
			return;

		default:
			sigs.async_wait(sigfd_handler);
	}
}

void
handle(const int &signum)
{
	switch(signum)
	{
		case SIGINT:   return handle_interruption();
		case SIGHUP:   return handle_hangup();
		case SIGQUIT:  return handle_quit();
		case SIGTERM:  return handle_quit();
		case SIGUSR1:  return handle_usr1();
	}
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
	ircd::log::error
	{
		"SIGQUIT handler: %s", e.what()
	};
}

void
handle_hangup()
try
{
	console_hangup();
}
catch(const std::exception &e)
{
	ircd::log::error
	{
		"SIGHUP handler: %s", e.what()
	};
}

void
handle_interruption()
try
{
	if(console_active)
		console_cancel();
	else
		console_spawn();
}
catch(const std::exception &e)
{
	ircd::log::error
	{
		"SIGINT handler: %s", e.what()
	};
}

void
handle_usr1()
try
{
	// Spawning the context that follows this branch and doing a rehash
	// when not in a stable state like runlevel::RUN will just make a mess
	// so any signal received is just dropped and the user can try again.
	if(ircd::runlevel != ircd::runlevel::RUN)
	{
		ircd::log::warning
		{
			"Not rehashing conf from SIGUSR1 in runlevel %s",
			reflect(ircd::runlevel)
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
