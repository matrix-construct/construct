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
#include <RB_INC_SYS_RESOURCE_H
#include <ircd/asio.h>
#include "lgetopt.h"
#include "construct.h"
#include "signals.h"
#include "console.h"

bool printversion;
bool cmdline;
std::vector<std::string> execute;
bool quietmode;
bool single;
bool debugmode;
bool nolisten;
bool nobackfill;
bool noautomod;
bool nocompact;
bool checkdb;
bool pitrecdb;
bool recoverdb;
bool repairdb;
bool nojs;
bool nodirect;
bool noaio;
bool noiou;
bool no6;
bool yes6;
bool norun;
bool read_only;
bool write_avoid;
bool slave;
std::array<bool, 8> smoketest;
bool soft_assert;
bool nomatrix;
bool matrix {true}; // matrix server by default.
bool defaults;
const char *bootstrap;
const char *diagnostic;

lgetopt opts[]
{
	{ "help",       nullptr,        lgetopt::USAGE,   "Print this text" },
	{ "version",    &printversion,  lgetopt::BOOL,    "Print version and exit" },
	{ "debug",      &debugmode,     lgetopt::BOOL,    "Enable options for debugging" },
	{ "quiet",      &quietmode,     lgetopt::BOOL,    "Suppress log messages at the terminal" },
	{ "single",     &single,        lgetopt::BOOL,    "Single user mode for maintenance and diagnostic" },
	{ "console",    &cmdline,       lgetopt::BOOL,    "Drop to a command line immediately after startup" },
	{ "execute",    &execute,       lgetopt::STRINGS, "Execute command lines immediately after startup" },
	{ "nolisten",   &nolisten,      lgetopt::BOOL,    "Normal execution but without listening sockets" },
	{ "nobackfill", &nobackfill,    lgetopt::BOOL,    "Disable initial backfill jobs after startup." },
	{ "noautomod",  &noautomod,     lgetopt::BOOL,    "Normal execution but without autoloading modules" },
	{ "nocompact",  &nocompact,     lgetopt::BOOL,    "Disable automatic database compaction" },
	{ "checkdb",    &checkdb,       lgetopt::BOOL,    "Perform complete checks of databases when opening" },
	{ "pitrecdb",   &pitrecdb,      lgetopt::BOOL,    "Allow Point-In-Time-Recover if DB reports corruption after crash" },
	{ "recoverdb",  &recoverdb,     lgetopt::BOOL,    "Allow earlier Point-In-Time when -pitrecdb does not work" },
	{ "repairdb",   &repairdb,      lgetopt::BOOL,    "Perform full DB repair after deep block/file corruption" },
	{ "nojs",       &nojs,          lgetopt::BOOL,    "Disable SpiderMonkey JS subsystem from initializing. (noop when not available)" },
	{ "nodirect",   &nodirect,      lgetopt::BOOL,    "Disable direct IO (O_DIRECT) for unsupporting filesystems" },
	{ "noaio",      &noaio,         lgetopt::BOOL,    "Disable the AIO interface in favor of traditional syscalls. " },
	{ "noiou",      &noiou,         lgetopt::BOOL,    "Disable the io_uring interface and fallback to AIO or system calls. " },
	{ "no6",        &no6,           lgetopt::BOOL,    "Disable IPv6 operations (default)" },
	{ "6",          &yes6,          lgetopt::BOOL,    "Enable IPv6 operations" },
	{ "norun",      &norun,         lgetopt::BOOL,    "[debug & testing only] Initialize but never run the event loop" },
	{ "ro",         &read_only,     lgetopt::BOOL,    "Read-only mode. No writes to database allowed" },
	{ "wa",         &write_avoid,   lgetopt::BOOL,    "Like read-only mode, but writes permitted if triggered" },
	{ "slave",      &slave,         lgetopt::BOOL,    "Like read-only mode; allows multiple instances of server" },
	{ "smoketest",  &smoketest[0],  lgetopt::BOOL,    "Starts and stops the daemon to return success" },
	{ "sassert",    &soft_assert,   lgetopt::BOOL,    "Softens assertion effects in debug mode" },
	{ "nomatrix",   &nomatrix,      lgetopt::BOOL,    "Prevent loading the matrix application module" },
	{ "matrix",     &matrix,        lgetopt::BOOL,    "Allow loading the matrix application module" },
	{ "defaults",   &defaults,      lgetopt::BOOL,    "Use configuration defaults without database load for this execution" },
	{ "bootstrap",  &bootstrap,     lgetopt::STRING,  "Bootstrap fresh database from event vector" },
	{ "diagnostic", &diagnostic,    lgetopt::STRING,  "Specify a diagnostic type in conjunction with other commands" },
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

[[noreturn]] static void do_restart(char *const *const &argv, char *const *const &envp);
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
	const char *const progname(_argv[0]);
	parseargs(&argc, &argv, opts);
	matrix &= !nomatrix;
	nomatrix |= !matrix;

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

	// Sets various other conf items based on the program options captured into
	// the globals preceding this frame.
	applyargs();

	// The network name (matrix origin) is the first positional argument after
	// any switched arguments. The matrix origin is the hostpart of MXID's for
	// the server.
	const ircd::string_view origin
	{
		argc > 0?
			argv[0]:
			nullptr
	};

	// The server_name is the unique name for this specific server. This is
	// generally the same as origin; but if origin is example.org with an
	// SRV record redirecting to matrix.example.org then server_name is
	// matrix.example.org. In clusters serving a single origin, all
	// server_names must be different.
	const ircd::string_view server_name
	{
		argc > 1?     // server_name given on command line
			argv[1]:
		argc > 0?     // server_name matches origin
			argv[0]:
			nullptr
	};

	// at least one server_name argument is required for now.
	if(!server_name && matrix)
		throw ircd::user_error
		{
			"usage :%s <origin> [servername]", progname
		};

	// The smoketest uses this ircd::run::level callback to set a flag when
	// each ircd::run::level is reached. All flags must be set to pass. The
	// smoketest is inert unless the -smoketest program option is used.
	const ircd::run::changed smoketester
	{
		[](const auto &level)
		{
			smoketest.at(size_t(level) + 1) = true;
			if(!smoketest[0])
				return;

			if(level != ircd::run::level::RUN)
				return;

			if(construct::console::active())
			{
				construct::console::quit_when_done = true;
				return;
			};

			ircd::post
			{
				ircd::quit
			};
		}
	};

	// Setup synchronization primitives on this stack for starting and stopping
	// the application (matrix homeserver). Note this stack cannot actually use
	// these; they'll be used to synchronize the closures below running in
	// different contexts.
	ircd::ctx::latch start(2), quit(2);
	std::exception_ptr eptr;

	// Setup the matrix homeserver application. This will be executed on an
	// ircd::context (dedicated stack). We construct several objects on the
	// stack which are the basis for our matrix homeserver. When the stack
	// unwinds, the homeserver will go out of service.
	const auto homeserver{[&origin, &server_name, &start, &quit, &eptr]
	{
		try
		{
			// 5 Load the matrix library dynamic shared object
			ircd::matrix matrix;

			// 6 Run a primary homeserver based on the program options given.
			struct ircd::m::homeserver::opts opts;
			opts.origin = origin;
			opts.server_name = server_name;
			opts.bootstrap_vector_path = bootstrap;
			opts.backfill = !nobackfill;
			const ircd::custom_ptr<ircd::m::homeserver> homeserver
			{
				// 6.1
				matrix.init(&opts), [&matrix]
				(ircd::m::homeserver *const homeserver)
				{
					// 11.1
					matrix.fini(homeserver);
				}
			};

			// 7 Notify the loader the homeserver is ready for service.
			start.count_down_and_wait();

			// 9 Yield until the loader notifies us; this stack will then unwind.
			quit.count_down_and_wait();
		}
		catch(...)
		{
			// ctx::exception_handler allows us to yield an ircd::ctx (stack
			// switch) inside a catch block, which we'll need in this scope.
			const ircd::ctx::exception_handler eh;

			// Copy a reference to the current exception which was saved by eh.
			// Note it is not safe to call std::current_exception() here.
			eptr = eh;

			// 7' Notify the loader the homeserver failed to start
			start.count_down_and_wait();

			// 9' Yield until the loader notifies us; this stack will then unwind.
			quit.count_down_and_wait();
		}

		// 11
	}};

	// This object registers a callback for a specific event in libircd; the
	// closure is called from the main context (#1) running ircd::main().
	// after libircd is ready for service in runlevel LOAD but before entering
	// runlevel RUN. It is called again immediately after entering runlevel
	// QUIT, but before any functionality of libircd destructs. This cues us
	// to start and stop the homeserver.
	const ircd::run::changed loader
	{
		[&homeserver, &start, &quit, &eptr](const auto &level)
		{
			static ircd::context context;

			// 2 This branch is taken the first time this function is called,
			// and not taken the second time.
			if(level == ircd::run::level::LOAD && !context && !nomatrix)
			{
				using namespace ircd::util;

				// 3 Launch the homeserver context (asynchronous).
				context = { "matrix", 1_MiB, ircd::context::POST, homeserver };

				// 4 Yield until the homeserver function notifies `start`; waiting
				// here prevents ircd::main() from entering runlevel RUN.
				start.count_down_and_wait();

				// 8 Check if error on start; rethrowing that here propagates
				// to ircd::main() and triggers a runlevel QUIT sequence.
				if(!!eptr)
					std::rethrow_exception(eptr);

				// 8.1
				return;
			}

			if(level != ircd::run::level::UNLOAD || !context)
				return;

			// 10 Notify the waiting homeserver context to quit; this will
			// start shutting down the homeserver.
			quit.count_down_and_wait();

			// 12 Wait for the homeserver context to finish before we return.
			context.join();
		}
	};

	// This is the sole io_context for Construct, and the ios.run() below is the
	// the only place where the program actually blocks.
	boost::asio::io_context ios;

	// libircd does no signal handling (or at least none that you ever have to
	// care about); reaction to all signals happens out here instead. Handling
	// is done properly through the io_context which registers the handler for
	// the platform and then safely posts the received signal to the io_context
	// event loop. This means we lose the true instant hardware-interrupt gratitude
	// of signals but with the benefit of unconditional safety and cross-
	// platformness with windows etc.
	construct::signals signals{ios};

	// Associates libircd with our io_context and posts the initial routines
	// to that io_context. Execution of IRCd will then occur during ios::run()
	ircd::init(ios.get_executor());

	// If the user wants to immediately drop to an interactive command line
	// without having to send a ctrl-c for it, that is provided here. This does
	// not actually take effect until it's processed in the ios.run() below.
	if(cmdline || !execute.empty())
		construct::console::spawn();

	// If the user wants to immediately process console commands
	// non-interactively from a program argument input, that is enqueued here.
	for(auto &&cmd : execute)
		construct::console::queue.emplace_back(std::move(cmd));

	// For developer debugging and testing this branch from a "-norun" argument
	// will exit before committing to the ios.run().
	if(norun)
		return EXIT_SUCCESS;

	// 1
	// Execution.
	// Blocks until a clean exit from a quit() or an exception comes out of it.
	const size_t handled
	{
		ios.run()
	};

	ircd::log::debug
	{
		"epoch construct:%zu ircd:%zu",
		handled,
		ircd::ios::epoch(),
	};

	// 13
	// The smoketest is enabled if the first value is true; then all of the
	// values must be true for the smoketest to pass.
	if(smoketest[0])
		return std::count(begin(smoketest), end(smoketest), true) == smoketest.size()?
			EXIT_SUCCESS:
			EXIT_FAILURE;

	// The restart flag can be set by the console command "restart" which
	// calls ircd::quit() to clean break from the run() loop.
	if(ircd::restart)
		do_restart(_argv, _envp);

	return EXIT_SUCCESS;
}
catch(const ircd::user_error &e)
{
	if(ircd::debugmode)
		ircd::terminate{e};

	fprintf(stderr, usererrstr, e.what());
	return EXIT_FAILURE;
}
catch(const std::exception &e)
{
	if(ircd::debugmode)
		ircd::terminate{e};

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

	if(RB_TIME_CONFIGURED != ircd::info::configured_time)
		ircd::log::warning
		{
			"Header configuration time:%ld (%s) %s than library configuration time:%ld (%s).",
			RB_TIME_CONFIGURED,
			RB_VERSION_TAG,
			RB_TIME_CONFIGURED > ircd::info::configured_time? "newer" : "older",
			ircd::info::configured_time,
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
do_restart(char *const *const &_argv,
           char *const *const &_envp)
{
	const auto args
	{
		ircd::replace(std::string(ircd::restart) + ' ', ' ', '\0')
	};

	std::vector<char *> argv{_argv[0]};
	for(size_t i(0); i < args.size(); ++i)
	{
		argv.emplace_back(const_cast<char *>(args.data() + i));
		for(; i < args.size() && args.at(i) != '\0'; ++i);
	}

	argv.emplace_back(nullptr);
	ircd::syscall(::execve, _argv[0], argv.data(), _envp);
	__builtin_unreachable();
}

/// These operations are safe to call before ircd::init() and anytime after
/// static initialization.
void
applyargs()
{
	if(diagnostic)
		ircd::diagnostic.set(diagnostic);

	if(single && !bootstrap)
	{
		ircd::write_avoid.set("true");
		cmdline = true;
	}

	if(bootstrap)
		ircd::maintenance.set("true");

	if(defaults)
		ircd::defaults.set("true");

	if(slave)
	{
		ircd::db::open_slave.set("true");
		read_only = true; // slave implies read_only
	}

	if(read_only)
	{
		ircd::read_only.set("true");
		write_avoid = true; // read_only implies write_avoid.
	}

	if(write_avoid)
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

	if(recoverdb)
		ircd::db::open_recover.set("recover");

	if(repairdb)
	{
		ircd::db::open_repair.set("true");
		nocompact = true;
		cmdline = true;
	}
	else ircd::db::open_repair.set("false");

	if(nocompact)
		ircd::db::auto_compact.set("false");

	if(nodirect)
		ircd::fs::fd::opts::direct_io_enable.set("false");
	else
		ircd::fs::fd::opts::direct_io_enable.set("true");

	if(noaio)
		ircd::fs::aio::enable.set("false");

	if(noiou)
		ircd::fs::iou::enable.set("false");

	if(yes6)
		ircd::net::enable_ipv6.set("true");
	else if(no6)
		ircd::net::enable_ipv6.set("false");
	else
		ircd::net::enable_ipv6.set("false");

	if(soft_assert)
		ircd::soft_assert.set("true");

	if(quietmode)
		ircd::log::console_disable();
}
