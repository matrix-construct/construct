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
#include "homeserver.h"
#include "signals.h"
#include "console.h"

bool printversion;
bool cmdline;
std::vector<std::string> execute;
bool quietmode;
bool single;
bool safemode;
bool debugmode;
bool nolisten;
bool nobackfill;
bool noautoapps;
bool noautomod;
bool nocompact;
const char *recoverdb;
bool nojs;
bool nodirect;
bool noaio;
bool noiou;
bool no6;
bool yes6;
bool norun;
bool nomain;
bool read_only;
bool write_avoid;
bool slave;
std::array<bool, 6> smoketest;
bool megatest;
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
	{ "safe",       &safemode,      lgetopt::BOOL,    "Safe mode; like -single but with even less functionality." },
	{ "console",    &cmdline,       lgetopt::BOOL,    "Drop to a command line immediately after startup" },
	{ "execute",    &execute,       lgetopt::STRINGS, "Execute command lines immediately after startup" },
	{ "nolisten",   &nolisten,      lgetopt::BOOL,    "Normal execution but without listening sockets" },
	{ "nobackfill", &nobackfill,    lgetopt::BOOL,    "Disable initial backfill jobs after startup." },
	{ "noautoapps", &noautoapps,    lgetopt::BOOL,    "Disable automatic execution of managed child applications." },
	{ "noautomod",  &noautomod,     lgetopt::BOOL,    "Normal execution but without autoloading modules" },
	{ "nocompact",  &nocompact,     lgetopt::BOOL,    "Disable automatic database compaction" },
	{ "recoverdb",  &recoverdb,     lgetopt::STRING,  "Specify recovery mode if DB reports corruption (try: point)" },
	{ "nojs",       &nojs,          lgetopt::BOOL,    "Disable SpiderMonkey JS subsystem from initializing. (noop when not available)" },
	{ "nodirect",   &nodirect,      lgetopt::BOOL,    "Disable direct IO (O_DIRECT) for unsupporting filesystems" },
	{ "noaio",      &noaio,         lgetopt::BOOL,    "Disable the AIO interface in favor of traditional syscalls. " },
	{ "noiou",      &noiou,         lgetopt::BOOL,    "Disable the io_uring interface and fallback to AIO or system calls. " },
	{ "no6",        &no6,           lgetopt::BOOL,    "Disable IPv6 operations (default)" },
	{ "6",          &yes6,          lgetopt::BOOL,    "Enable IPv6 operations" },
	{ "norun",      &norun,         lgetopt::BOOL,    "[debug] Initialize but never run the event loop" },
	{ "nomain",     &nomain,        lgetopt::BOOL,    "[debug] Initialize and run without entering ircd::main()" },
	{ "ro",         &read_only,     lgetopt::BOOL,    "Read-only mode. No writes to database allowed" },
	{ "wa",         &write_avoid,   lgetopt::BOOL,    "Like read-only mode, but writes permitted if triggered" },
	{ "slave",      &slave,         lgetopt::BOOL,    "Like read-only mode; allows multiple instances of server" },
	{ "smoketest",  &smoketest[0],  lgetopt::BOOL,    "Starts and stops the daemon to return success" },
	{ "megatest",   &megatest,      lgetopt::BOOL,    "Trap execution every millionth tick for diagnostic and statistics." },
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
static void smoketest_handler(const enum ircd::run::level &);
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

	// Setup the matrix homeserver application. This will be executed on an
	// ircd::context (dedicated stack). We construct several objects on the
	// stack which are the basis for our matrix homeserver. When the stack
	// unwinds, the homeserver will go out of service.
	struct ircd::m::homeserver::opts opts;
	opts.origin = origin;
	opts.server_name = server_name;
	opts.bootstrap_vector_path = bootstrap;
	opts.backfill = !nobackfill;
	opts.autoapps = !noautoapps;
	const std::function<void (ircd::main_continuation)> homeserver
	{
		[&opts](const ircd::main_continuation &main)
		{
			// Load the matrix module
			ircd::matrix matrix;

			// Construct the homeserver.
			construct::homeserver homeserver
			{
				matrix, opts
			};

			// Bail for debug/testing purposes.
			if(nomain)
				return;

			// Call main()'s continuation.
			main();
		}
	};

	// The smoketest uses this ircd::run::level callback to set a flag when
	// each ircd::run::level is reached. All flags must be set to pass. The
	// smoketest is inert unless the -smoketest program option is used.
	const ircd::run::changed smoketester
	{
		smoketest_handler
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
	ircd::init(ios.get_executor(), matrix? homeserver: nullptr);

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

	// Execution.
	// Loops until a clean exit from a quit() or an exception comes out of it.
	// Alternatively, ios.run() could be otherwise used here.
	size_t epoch{0};
	if(likely(!megatest))
		for(; !ios.stopped(); ++epoch)
		{
			ios.run_one();
			if constexpr(ircd::ios::profile::logging)
				ircd::log::logf
				{
					ircd::ios::log, ircd::log::DEBUG,
					"EPOCH ----- construct:%zu ircd:%zu %zu",
					epoch,
					ircd::ios::epoch(),
				};
		}

	// megatest is a mock execution which traps on every 1048576th tick.
	if(megatest)
		while(!ios.stopped())
		{
			ios.run_one();
			if constexpr(ircd::ios::profile::logging)
				ircd::log::logf
				{
					ircd::ios::log, ircd::log::DEBUG,
					"EPOCH ----- construct:%zu ircd:%zu %zu",
					epoch,
					ircd::ios::epoch(),
				};

			if(++epoch % 1048576 == 0)
				ircd::debugtrap();
		}

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

void
smoketest_handler(const enum ircd::run::level &level)
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

	static ircd::ios::descriptor descriptor
	{
		"construct.smoketest"
	};

	ircd::dispatch
	{
		descriptor, ircd::ios::defer, ircd::quit
	};
}

/// These operations are safe to call before ircd::init() and anytime after
/// static initialization.
void
applyargs()
{
	if(diagnostic)
		ircd::diagnostic.set(diagnostic);

	if(safemode)
	{
		single = true;
		nocompact = true;
		noautoapps = true;
		ircd::server::enable.set("false");
		ircd::db::auto_deletion.set("false");
	}

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

	if(ircd::string_view(recoverdb) == "repair")
	{
		ircd::db::open_repair.set("true");
		nocompact = true;
		cmdline = true;
	}
	else if(recoverdb)
		ircd::db::open_recover.set(recoverdb);

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

///////////////////////////////////////////////////////////////////////////////
//
// Unfortunate but worthwhile hack hook which allows us to optimize the
// behavior of the boost::asio event loop by executing more queued tasks
// before dropping to epoll_wait(2). This reduces the number of syscalls to
// epoll_wait(2), which tend to occur at the start of every epoch except in a
// minority of cases, and produce nothing most of the time. See addl docs.
//
#if defined(BOOST_ASIO_HAS_EPOLL)

extern "C" int
__real_epoll_wait(int __epfd,
                  struct epoll_event *__events,
                  int __maxevents,
                  int __timeout);

extern "C" int
__wrap_epoll_wait(int __epfd,
                  struct epoll_event *const __events,
                  int __maxevents,
                  int __timeout)
{
	// see addl documentation in ircd/ios
	return ircd::ios::epoll_wait<__real_epoll_wait>
	(
		__epfd,
		__events,
		__maxevents,
		__timeout
	);
}

#endif
