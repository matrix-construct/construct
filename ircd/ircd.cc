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
	static void main(user_main) noexcept;
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

/// This item allows the library to indicate to the embedder that they should
/// restart their application (or reload this library if available). The
/// use-case here is for features like the `restart` command in the console
/// module. Such a command triggers a normal quit and the application may exit
/// normally; therefor the embedder should check this item to perform a restart
/// rather than exiting.
///
/// This item is a string to allow for different program options at restart.
/// For now this is limited to space-separated arguments without respect for
/// quoting (for now), so no arguments can have spaces.
///
/// Empty string disables restart. The name of the executable should not be
/// prefixed to the string.
decltype(ircd::restart)
ircd::restart
{
	{ "name",     "ircd.restart"       },
	{ "default",  std::string{}        },
	{ "persist",  false                },
};

/// Coarse mode indicator for debug/developer behavior when and if possible.
/// For example: additional log messages may be enabled by this mode. This
/// option is technically effective in both release builds and debug builds
/// but it controls far less in non-debug builds. This item may be toggled
/// at any time. It doesn't change operational functionality.
decltype(ircd::debugmode)
ircd::debugmode
{
	{ "name",     "ircd.debugmode"     },
	{ "default",  false                },
	{ "persist",  false                },
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

/// Coarse mode declaration for "maintenance mode" a.k.a. "single user mode"
/// which is intended to be similar to normal operating mode but without
/// services to clients or some background tasks. It is implied and set when
/// write_avoid=true which is itself implied and set by read_only.
decltype(ircd::maintenance)
ircd::maintenance
{
	{
		{ "name",     "ircd.maintenance"   },
		{ "default",  false                },
		{ "persist",  false                },
	}, []
	{
		if(!maintenance)
			return;

		net::listen.set("false");
	}
};

/// Coarse mode indicator for degraded operation known as "write-avoid" which
/// is similar to read_only but not hard-enforced. Writes may still occur,
/// such as those manually triggered by an admin. All subsystems and background
/// tasks otherwise depart from normal operation to avoid writes.
decltype(ircd::write_avoid)
ircd::write_avoid
{
	{
		{ "name",     "ircd.write_avoid"   },
		{ "default",  false                },
		{ "persist",  false                },
	}, []
	{
		if(!write_avoid)
			return;

		maintenance.set("true");
	}
};

/// Coarse mode declaration for read-only behavior. All subsystems and feature
/// modules respect this indicator by preventing any writes and persistence
/// during execution. This item should be set before ircd::init() to be most
/// effective.
decltype(ircd::read_only)
ircd::read_only
{
	{
		{ "name",     "ircd.read_only"     },
		{ "default",  false                },
		{ "persist",  false                },
	}, []
	{
		if(!read_only)
			return;

		write_avoid.set("true");
	}
};

/// Diagnostic options selection. This indicates whether any tests or special
/// behavior should occur rather than normal operation; also allowing for
/// fine-grained options to be conveyed to such tests/diagnostics. While this
/// appears here as coarse library-wide option it does not on its own affect
/// normal server operations just by being set. It affect things only if
/// specific functionality checks and alters its behavior based on the value
/// of this string contextually.
decltype(ircd::diagnostic)
ircd::diagnostic
{
	{ "name",     "ircd.diagnostic"     },
	{ "default",  string_view{}         },
	{ "persist",  false                 },
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
ircd::init(boost::asio::executor &&executor,
           user_main user)
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
		"main",
		512_KiB,
		std::bind(&ircd::main, std::move(user)),
		context::POST | context::SLICE_EXEMPT
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
	log::logf
	{
		log::star, log::level::DEBUG,
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

		case run::level::QUIT:
		case run::level::HALT:
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
	log::logf
	{
		log::star, log::level::DEBUG,
		"IRCd cont requested from runlevel:%s ctx:%p main_context:%p",
		reflect(run::level),
		(const void *)ctx::current,
		(const void *)main_context
	};

	switch(run::level)
	{
		case run::level::RUN:
			break;

		default:
			return;
	}

	log::notice
	{
		log::star, "IRCd resuming service in runlevel %s.",
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
ircd::main(user_main user)
noexcept try
{
	// When this function completes without exception, subsystems are done
	// shutting down and IRCd transitions to HALT.
	const unwind_defer halted{[]
	{
		run::set(run::level::HALT);
	}};

	// We block interruption/termination of the main context by default;
	// this covers most of the functionality performed by this function and
	// its callees. This provides consistent and complete runlevel transitions.
	const ctx::uninterruptible::nothrow disable_interruption {true};

	// When this function is entered IRCd will transition to START indicating
	// that subsystems are initializing.
	run::set(run::level::START);

	// These objects are the init()'s and fini()'s for each subsystem.
	// Appearing here ties their life to the main context. Initialization can
	// also occur in ircd::init() or static initialization itself if either are
	// more appropriate.

	ctx::ole::init _ole_;    // Thread OffLoad Engine
	fs::init _fs_;           // Local filesystem
	cl::init _cl_;           // OpenCL
	magic::init _magic_;     // libmagic
	magick::init _magick_;   // ImageMagick
	openssl::init _ossl_;    // openssl crypto
	net::init _net_;         // Networking
	db::init _db_;           // RocksDB
	client::init _client_;   // Client related
	server::init _server_;   // Server related
	js::init _js_;           // SpiderMonkey

	// Continuation passed to the user's main function.
	const auto continuation{[]
	{
		// Transition to the QUIT state on unwind.
		const unwind quit{[]
		{
			const ctx::uninterruptible::nothrow disable_interruption {true};
			ircd::run::set(run::level::QUIT);
		}};

		// Block interruptions again for runlevel transitions.
		const ctx::uninterruptible disable_interruption {true};

		// IRCd will now transition to the RUN state indicating full functionality.
		run::set(run::level::RUN);

		// Allow interrupts while running so we can be notified to quit.
		const ctx::uninterruptible reenable_interruption {false};

		// wait() blocks until the main context is notified or interrupted etc.
		// Waiting here will hold open this stack with all of the above objects
		// living on it.
		ctx::wait();
	}};

	if(!user)
		return continuation();

	// Allow interrupts again by default for the duration of the user.
	const ctx::uninterruptible reenable_interruption {false};

	// Call user.
	user(continuation);
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
