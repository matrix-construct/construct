// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

// internal interface to ircd::run (see ircd/run.h)
namespace ircd::run
{
	// change the current runlevel
	static bool set(const enum level &);
}

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

decltype(ircd::version_api)
ircd::version_api
{
	"IRCd", info::versions::API, 0, {0, 0, 0}, RB_VERSION
};

decltype(ircd::version_abi)
ircd::version_abi
{
	"IRCd", info::versions::ABI, 0, {0, 0, 0}, ircd::info::version
};

decltype(ircd::soft_assert)
ircd::soft_assert
{
	{ "name",     "ircd.soft_assert"   },
	{ "default",  false                },
	{ "persist",  false                },
};

decltype(ircd::write_avoid)
ircd::write_avoid
{
	{ "name",     "ircd.write_avoid"   },
	{ "default",  false                },
	{ "persist",  false                },
};

decltype(ircd::read_only)
ircd::read_only
{
	{ "name",     "ircd.read_only"     },
	{ "default",  false                },
	{ "persist",  false                },
};

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

decltype(ircd::main_context)
ircd::main_context;

/// Sets up the IRCd and its main context, then returns without blocking.
//
/// Pass your io_context instance, it will share it with the rest of your program.
/// An exception will be thrown on error.
///
/// This function will setup the main program loop of libircd. The execution will
/// occur when your io_context.run() or poll() is further invoked.
///
/// init() can only be called from a run::level::HALT state
void
ircd::init(boost::asio::io_context &user_ios)
try
{
	// This function must only be called from a HALT state.
	if(run::level != run::level::HALT)
		throw error
		{
			"Cannot init() IRCd from runlevel %s", reflect(run::level)
		};

	// Setup the core event loop system starting with the user's supplied ios.
	ios::init(user_ios);

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

		case run::level::IDLE:
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

		case run::level::IDLE:
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
	// Resamples the thread this context was executed on which should be where
	// the user ran ios.run(). The user may have invoked ios.run() on multiple
	// threads, but we consider this one thread a main thread for now...
	ircd::ios::main_thread_id = std::this_thread::get_id();

	// When this function completes without exception, subsystems are done shutting down and IRCd
	// transitions to HALT.
	const unwind::defer halted{[]
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

	// IRCd will now transition to the IDLE state allowing library user's to
	// load their applications using the run::changed callback.
	run::set(run::level::IDLE);

	// IRCd will now transition to the RUN state indicating full functionality.
	run::set(run::level::RUN);

	// This call blocks until the main context is notified or interrupted etc.
	// Waiting here will hold open this stack with all of the above objects
	// living on it.
	ctx::wait();
}
catch(const http::error &e) // <-- m::error
{
	log::critical
	{
		"IRCd main :%s %s", e.what(), e.content
	};
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

///////////////////////////////////////////////////////////////////////////////
//
// ircd/run.h
//

namespace ircd::run
{
	static enum level _level, _chadburn;
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
	changed::dock.wait([]
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
		"IRCd %s", reflect(level)
	};

	log::flush();
	changed::dock.notify_all();
	for(const auto &handler : changed::list) try
	{
		handler->function(new_level);
	}
	catch(const std::exception &e)
	{
		switch(level)
		{
			default:           break;
			case level::IDLE:  throw;
			case level::QUIT:  continue;
			case level::HALT:  continue;
		}

		log::critical
		{
			"Runlevel change to %s handler(%p) :%s",
			reflect(new_level),
			handler,
			e.what()
		};
	}

	if(level == level::HALT)
		return true;

	log::debug
	{
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
	log::critical
	{
		"IRCd level change to '%s': %s",
		reflect(new_level),
		e.what()
	};

	ircd::terminate();
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
		case level::IDLE:      return "IDLE";
		case level::RUN:       return "RUN";
		case level::QUIT:      return "QUIT";
		case level::FAULT:     return "FAULT";
	}

	return "??????";
}
