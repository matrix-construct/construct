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
	// Fundamental context #1; all subsystems live as objects on this stack.
	// This is created by ircd::init(), and it executes ircd::main(), it then
	// deletes itself and nulls this pointer when finished.
	extern ctx::ctx *main_context;

	// Main function. This frame anchors the initialization and destruction of
	// all non-static assets provided by the library.
	static void main() noexcept;
}

// internal interface to ircd::run (see ircd/run.h, ircd/run.cc)
namespace ircd::run
{
	// change the current runlevel
	bool set(const enum level &);
}

/// Records compile-time header version information.
decltype(ircd::version_api)
ircd::version_api
{
	"IRCd", info::versions::API, 0, {0, 0, 0}, RB_VERSION
};

/// Records runtime linked library (this library) version information.
decltype(ircd::version_abi)
ircd::version_abi
{
	"IRCd", info::versions::ABI, 0, {0, 0, 0}, ircd::info::version
};

/// When assertions are enabled this further softens runtime behavior to be
/// non-disruptive/non-terminating for diagnostic purposes. Debugging/developer
/// use only. This item may be toggled at any time.
decltype(ircd::soft_assert)
ircd::soft_assert
{
	{ "name",     "ircd.soft_assert"   },
	{ "default",  false                },
	{ "persist",  false                },
};

/// Coarse mode indicator for degraded operation known as "write-avoid" which
/// is similar to read_only but not hard-enforced. Writes may still occur,
/// such as those manually triggered by an admin. All subsystems and background
/// tasks otherwise depart from normal operation to avoid writes.
decltype(ircd::write_avoid)
ircd::write_avoid
{
	{ "name",     "ircd.write_avoid"   },
	{ "default",  false                },
	{ "persist",  false                },
};

/// Coarse mode declaration for read-only behavior. All subsystems and feature
/// modules respect this indicator by preventing any writes and persistence
/// during execution. This item should be set before ircd::init() to be most
/// effective.
decltype(ircd::read_only)
ircd::read_only
{
	{ "name",     "ircd.read_only"     },
	{ "default",  false                },
	{ "persist",  false                },
};

/// Coarse mode indicator for debug/developer behavior when and if possible.
/// For example: additional log messages may be enabled by this mode. This
/// option is technically effective in both release builds and debug builds
/// but it controls far less in non-debug builds. This item may be toggled
/// at any time.
decltype(ircd::debugmode)
ircd::debugmode
{
	{ "name",     "ircd.debugmode"     },
	{ "default",  false                },
	{ "persist",  false                },
};

decltype(ircd::restart)
ircd::restart
{
	{ "name",     "ircd.restart"       },
	{ "default",  false                },
	{ "persist",  false                },
};

/// Main context pointer placement.
decltype(ircd::main_context)
ircd::main_context;

/// Sets up the IRCd and its main context, then returns without blocking.
///
/// Pass the executor obtained from your io_context instance.
///
/// This function will setup the main program loop of libircd. The execution will
/// occur when your io_context.run() or poll() is further invoked.
///
/// init() can only be called from a run::level::HALT state
void
ircd::init(boost::asio::executor &&executor)
try
{
	// This function must only be called from a HALT state.
	if(run::level != run::level::HALT)
		throw error
		{
			"Cannot init() IRCd from runlevel %s", reflect(run::level)
		};

	// Setup the core event loop system starting with the user's supplied ios.
	ios::init(std::move(executor));

	// The log is available. but it is console-only until conf opens files.
	log::init();
	log::mark("DEADSTART"); // 6600

	// This starts off the log with library information.
	info::dump();

	// Setup the main context, which is a new stack executing the function
	// ircd::main(). The main_context is the first ircd::ctx to be spawned
	// and will be the last to finish.
	//
	// The context::POST will delay this spawn until the next io_context
	// event slice, so no context switch will occur here. Note that POST has
	// to be used here because: A. This init() function is executing on the
	// main stack, and context switches can only occur between context stacks,
	// not between contexts and the main stack. B. The user's io_context may or
	// may not even be running yet anyway.
	//
	// (debug compilation) The context::SLICE_EXEMPT flag exempts the context
	// from assertions that it's not blocking the process with excessive CPU
	// usage or long syscall. Main context can't meet this requirement.
	//
	context main_context
	{
		"main", 256_KiB, &ircd::main, context::POST | context::SLICE_EXEMPT
	};

	// The default behavior for ircd::context is to join the ctx on dtor. We
	// can't have that here because this is strictly an asynchronous function
	// on the main stack. Under normal circumstances, the mc will be entered
	// and be able to delete this pointer itself when it finishes. Otherwise
	// this must be manually deleted with assurance that mc will never enter.
	ircd::main_context = main_context.detach();

	// Finally, without prior exception, the commitment to run::level::READY
	// is made here. The user can now invoke their ios.run(), or, if they
	// have already, IRCd will begin main execution shortly...
	run::set(run::level::READY);
}
catch(const std::exception &e)
{
	throw;
}

/// Notifies IRCd to shutdown. A shutdown will occur asynchronously and this
/// function will return immediately. A runlevel change to HALT will be
/// indicated when IRCd has no more work for the ios. When the HALT state
/// is observed the user is free to destruct all resources related to libircd.
///
/// This function is the proper way to shutdown libircd after an init(), and while
/// your io_context.run() is invoked without stopping your io_context shared by
/// other activities unrelated to libircd. If your io_context has no other activities
/// the run() will then return immediately after IRCd posts its transition to
/// the HALT state.
///
bool
ircd::quit()
noexcept
{
	log::debug
	{
		"IRCd quit requested from runlevel:%s ctx:%p main_context:%p",
		reflect(run::level),
		(const void *)ctx::current,
		(const void *)main_context
	};

	if(main_context) switch(run::level)
	{
		case run::level::READY:
		{
			ctx::terminate(*main_context);
			main_context = nullptr;
			ircd::run::set(run::level::HALT);
			return true;
		}

		case run::level::LOAD:
		case run::level::START:
		{
			ctx::terminate(*main_context);
			main_context = nullptr;
			return true;
		}

		case run::level::RUN:
		{
			ctx::notify(*main_context);
			main_context = nullptr;
			return true;
		}

		case run::level::HALT:
		case run::level::QUIT:
		case run::level::FAULT:
			return false;
	}

	return false;
}

/// Notifies IRCd that execution is being resumed after a significant gap.
/// Basically this is connected to a SIGCONT handler and beneficial after
/// user stops, debugging and ACPI suspensions, etc. It is not required at
/// this time, but its connection is advised for best behavior.
void
ircd::cont()
noexcept
{
	log::debug
	{
		"IRCd cont requested from runlevel:%s ctx:%p main_context:%p",
		reflect(run::level),
		(const void *)ctx::current,
		(const void *)main_context
	};

	switch(run::level)
	{
		case run::level::HALT:
		case run::level::READY:
		case run::level::FAULT:
			return;

		case run::level::START:
		case run::level::QUIT:
			return;

		case run::level::LOAD:
		case run::level::RUN:
			break;
	}

	log::notice
	{
		"IRCd resuming service in runlevel:%s.",
		reflect(run::level),
	};
}

/// Main context; Main program. Do not call this function directly.
///
/// This function manages the lifetime for all resources and subsystems
/// that don't/can't have their own static initialization. When this
/// function is entered, subsystem init objects are constructed on the
/// frame. The lifetime of those objects is the handle to the lifetime
/// of the subsystem, so destruction will shut down that subsystem.
///
/// The status of this function and IRCd overall can be observed through
/// the ircd::runlevel. The ircd::run::changed callback can be set
/// to be notified on a runlevel change. The user should wait for a runlevel
/// of HALT before destroying IRCd related resources and stopping their
/// io_context from running more jobs.
///
void
ircd::main()
noexcept try
{
	// When this function completes without exception, subsystems are done shutting down and IRCd
	// transitions to HALT.
	const unwind_defer halted{[]
	{
		run::set(run::level::HALT);
	}};

	// When this function is entered IRCd will transition to START indicating
	// that subsystems are initializing.
	run::set(run::level::START);

	// These objects are the init()'s and fini()'s for each subsystem.
	// Appearing here ties their life to the main context. Initialization can
	// also occur in ircd::init() or static initialization itself if either are
	// more appropriate.

	fs::init _fs_;           // Local filesystem
	magic::init _magic_;     // libmagic
	ctx::ole::init _ole_;    // Thread OffLoad Engine
	openssl::init _ossl_;    // openssl crypto
	net::init _net_;         // Networking
	db::init _db_;           // RocksDB
	client::init _client_;   // Client related
	server::init _server_;   // Server related
	js::init _js_;           // SpiderMonkey

	// Transition to the QUIT state on unwind.
	const unwind quit{[]
	{
		ircd::run::set(run::level::QUIT);
	}};

	// IRCd will now transition to the LOAD state allowing library user's to
	// load their applications using the run::changed callback.
	run::set(run::level::LOAD);

	// IRCd will now transition to the RUN state indicating full functionality.
	run::set(run::level::RUN);

	// This call blocks until the main context is notified or interrupted etc.
	// Waiting here will hold open this stack with all of the above objects
	// living on it.
	ctx::wait();
}
catch(const std::exception &e)
{
	log::critical
	{
		"IRCd main :%s", e.what()
	};
}
catch(const ctx::terminated &)
{
	return;
}
catch(...)
{
	log::critical
	{
		"IRCd main error."
	};
}

/// IRCd uptime in seconds
ircd::seconds
ircd::uptime()
{
	return seconds(ircd::time() - info::startup_time);
}
