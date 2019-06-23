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
#include "signals.h"
#include "console.h"

bool printversion;
bool cmdline;
bool quietmode;
bool debugmode;
bool nolisten;
bool noautomod;
bool checkdb;
bool pitrecdb;
bool nojs;
bool nodirect;
bool noaio;
bool no6;
bool yes6;
bool norun;
bool read_only;
bool write_avoid;
bool soft_assert;
const char *execute;
std::array<bool, 6> smoketest;

lgetopt opts[]
{
	{ "help",       nullptr,        lgetopt::USAGE,   "Print this text" },
	{ "version",    &printversion,  lgetopt::BOOL,    "Print version and exit" },
	{ "debug",      &debugmode,     lgetopt::BOOL,    "Enable options for debugging" },
	{ "quiet",      &quietmode,     lgetopt::BOOL,    "Suppress log messages at the terminal." },
	{ "console",    &cmdline,       lgetopt::BOOL,    "Drop to a command line immediately after startup" },
	{ "execute",    &execute,       lgetopt::STRING,  "Execute command lines immediately after startup" },
	{ "nolisten",   &nolisten,      lgetopt::BOOL,    "Normal execution but without listening sockets" },
	{ "noautomod",  &noautomod,     lgetopt::BOOL,    "Normal execution but without autoloading modules" },
	{ "checkdb",    &checkdb,       lgetopt::BOOL,    "Perform complete checks of databases when opening" },
	{ "pitrecdb",   &pitrecdb,      lgetopt::BOOL,    "Allow Point-In-Time-Recover if DB reports corruption after crash" },
	{ "nojs",       &nojs,          lgetopt::BOOL,    "Disable SpiderMonkey JS subsystem from initializing. (noop when not available)." },
	{ "nodirect",   &nodirect,      lgetopt::BOOL,    "Disable direct IO (O_DIRECT) for unsupporting filesystems." },
	{ "noaio",      &noaio,         lgetopt::BOOL,    "Disable the AIO interface in favor of traditional syscalls. " },
	{ "no6",        &no6,           lgetopt::BOOL,    "Disable IPv6 operations (default)" },
	{ "6",          &yes6,          lgetopt::BOOL,    "Enable IPv6 operations" },
	{ "norun",      &norun,         lgetopt::BOOL,    "[debug & testing only] Initialize but never run the event loop." },
	{ "ro",         &read_only,     lgetopt::BOOL,    "Read-only mode. No writes to database allowed." },
	{ "wa",         &write_avoid,   lgetopt::BOOL,    "Like read-only mode, but writes permitted if triggered." },
	{ "smoketest",  &smoketest[0],  lgetopt::BOOL,    "Starts and stops the daemon to return success."},
	{ "sassert",    &soft_assert,   lgetopt::BOOL,    "Softens assertion effects in debug mode."},
	{ nullptr,      nullptr,        lgetopt::STRING,  nullptr },
};

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
*** A fatal startup error has occurred:
***

%s

***
*** Please fix the problem to continue.
***
)"};

static bool startup_checks();
static void applyargs();
static void enable_coredumps();
static int print_version();

int
main(int _argc, char *const *_argv, char *const *const _envp)
noexcept try
{
	umask(077);       // better safe than sorry --SRB

	// '-' switched arguments come first; this function incs argv and decs argc
	auto argc(_argc);
	auto argv(_argv), envp(_envp);
	parseargs(&argc, &argv, opts);
	applyargs();

	// cores are not dumped without consent of the user to maintain the privacy
	// of cryptographic key material in memory at the time of the crash. Note
	// that on systems that support MADV_DONTDUMP such material will be excluded
	// from the coredump. Nevertheless, other sensitive material such as user
	// data may still be present in the core.
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

	// The smoketest uses this ircd::run::level callback to set a flag when
	// each ircd::run::level is reached. All flags must be set to pass.
	const ircd::run::changed smoketester{[](const auto &level)
	{
		smoketest.at(size_t(level) + 1) = true;
		if(smoketest[0] && level == ircd::run::level::RUN) ircd::post{[]
		{
			ircd::quit();
		}};
	}};

	// This is the sole io_context for Construct, and the ios.run() below is the
	// the only place where the program actually blocks.
	boost::asio::io_context ios(1);

	// Associates libircd with our io_context and posts the initial routines
	// to that io_context. Execution of IRCd will then occur during ios::run()
	// note: only supports service for one hostname/origin at this time.
	ircd::init(ios, origin, hostname);

	// libircd does no signal handling (or at least none that you ever have to
	// care about); reaction to all signals happens out here instead. Handling
	// is done properly through the io_context which registers the handler for
	// the platform and then safely posts the received signal to the io_context
	// event loop. This means we lose the true instant hardware-interrupt gratitude
	// of signals but with the benefit of unconditional safety and cross-
	// platformness with windows etc.
	const construct::signals signals{ios};

	// If the user wants to immediately drop to an interactive command line
	// without having to send a ctrl-c for it, that is provided here. This does
	// not actually take effect until it's processed in the ios.run() below.
	if(cmdline)
		construct::console::spawn();

	// If the user wants to immediately process console commands
	// non-interactively from a program argument input, that is enqueued here.
	if(execute)
		construct::console::execute({execute});

	// For developer debugging and testing this branch from a "-norun" argument
	// will exit before committing to the ios.run().
	if(norun)
		return EXIT_SUCCESS;

	// Execution.
	// Blocks until a clean exit from a quit() or an exception comes out of it.
	ios.run();

	// The smoketest is enabled if the first value is true; then all of the
	// values must be true for the smoketest to pass.
	if(smoketest[0])
		return std::count(begin(smoketest), end(smoketest), true) == smoketest.size()?
			EXIT_SUCCESS:
			EXIT_FAILURE;

	// The restart flag can be set by the console command "restart" which
	// calls ircd::quit() to clean break from the run() loop.
	if(ircd::restart)
		ircd::syscall(::execve, _argv[0], _argv, _envp);

	return EXIT_SUCCESS;
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
	if(RB_VERSION != ircd::info::version)
		ircd::log::warning
		{
			"Header version '%s' mismatch library '%s'",
			RB_VERSION,
			ircd::info::version,
		};

	if(RB_VERSION_TAG != ircd::info::tag)
		ircd::log::warning
		{
			"Header version tag '%s' mismatch library '%s'",
			RB_VERSION_TAG,
			ircd::info::tag,
		};

	if(RB_TIME_CONFIGURED > ircd::info::configured_time || RB_TIME_CONFIGURED < ircd::info::configured_time)
		ircd::log::warning
		{
			"Header configuration time:%ld (%s) %s than library configuration time:%ld (%s).",
			RB_TIME_CONFIGURED,
			RB_VERSION_TAG,
			RB_TIME_CONFIGURED > ircd::info::configured_time? "newer" : "older",
			ircd::info::configured,
			ircd::info::tag,
		};

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

void
applyargs()
{
	if(read_only)
		ircd::read_only.set("true");

	// read_only implies write_avoid.
	if(write_avoid || read_only)
		ircd::write_avoid.set("true");

	if(debugmode)
		ircd::debugmode.set("true");
	else
		ircd::debugmode.set("false");

	if(nolisten)
		ircd::net::listen.set("false");
	else
		ircd::net::listen.set("true");

	if(noautomod)
		ircd::mods::autoload.set("false");
	else
		ircd::mods::autoload.set("true");

	if(checkdb)
		ircd::db::open_check.set("true");
	else
		ircd::db::open_check.set("false");

	if(pitrecdb)
		ircd::db::open_recover.set("point");
	else
		ircd::db::open_recover.set("absolute");

	if(nodirect)
		ircd::fs::fd::opts::direct_io_enable.set("false");
	else
		ircd::fs::fd::opts::direct_io_enable.set("true");

	if(noaio)
		ircd::fs::aio::enable.set("false");
	else
		ircd::fs::aio::enable.set("true");

	if(yes6)
		ircd::net::enable_ipv6.set("true");
	else if(no6)
		ircd::net::enable_ipv6.set("false");
	else
		ircd::net::enable_ipv6.set("false");

	if(soft_assert)
		ircd::soft_assert.set("true");
}
